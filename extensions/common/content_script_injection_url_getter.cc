// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/content_script_injection_url_getter.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "url/scheme_host_port.h"

namespace extensions {

ContentScriptInjectionUrlGetter::FrameAdapter::~FrameAdapter() = default;

// static
GURL ContentScriptInjectionUrlGetter::Get(
    const FrameAdapter& frame,
    const GURL& document_url,
    MatchOriginAsFallbackBehavior match_origin_as_fallback,
    bool allow_inaccessible_parents) {
  auto should_consider_origin = [&document_url, match_origin_as_fallback]() {
    switch (match_origin_as_fallback) {
      case MatchOriginAsFallbackBehavior::kNever:
        return false;
      case MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree:
        return document_url.SchemeIs(url::kAboutScheme);
      case MatchOriginAsFallbackBehavior::kAlways:
        // TODO(devlin): Add more schemes here - blob, filesystem, etc.
        return document_url.SchemeIs(url::kAboutScheme) ||
               document_url.SchemeIs(url::kDataScheme);
    }

    NOTREACHED();
  };

  // If we don't need to consider the origin, we're done.
  if (!should_consider_origin())
    return document_url;

  // Get the security origin for the `frame`. For about: frames, this is the
  // origin of that of the controlling frame - e.g., an about:blank frame on
  // https://example.com will have the security origin of https://example.com.
  // Other frames, like data: frames, will have an opaque origin. For these,
  // we can get the precursor origin.
  const url::Origin frame_origin = frame.GetOrigin();
  const url::SchemeHostPort& tuple_or_precursor_tuple =
      frame_origin.GetTupleOrPrecursorTupleIfOpaque();

  // When there's no valid tuple (which can happen in the case of e.g. a
  // browser-initiated navigation to an opaque URL), there's no origin to
  // fallback to. Bail.
  if (!tuple_or_precursor_tuple.IsValid())
    return document_url;

  const url::Origin origin_or_precursor_origin =
      url::Origin::Create(tuple_or_precursor_tuple.GetURL());

  if (!allow_inaccessible_parents &&
      !frame.CanAccess(origin_or_precursor_origin)) {
    // The `frame` can't access its precursor. Bail.
    return document_url;
  }

  // Note: Just because the frame origin can theoretically access its
  // precursor origin, there may be more restrictions in practice - such as
  // if the frame has the disallowdocumentaccess attribute. It's okay to
  // ignore this case for context classification because it's not meant as an
  // origin boundary (unlike e.g. a sandboxed frame).

  // Looks like the initiator origin is an appropriate fallback!

  if (match_origin_as_fallback == MatchOriginAsFallbackBehavior::kAlways) {
    // The easy case! We use the origin directly. We're done.
    return origin_or_precursor_origin.GetURL();
  }

  DCHECK_EQ(MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree,
            match_origin_as_fallback);

  // Unfortunately, in this case, we have to climb the frame tree. This is for
  // match patterns that are associated with paths as well, not just origins.
  // For instance, if an extension wants to run on google.com/maps/* with
  // match_about_blank true, then it should run on about:-scheme frames created
  // by google.com/maps, but not about:-scheme frames created by google.com
  // (which is what the precursor tuple origin would be).

  // Traverse the frame/window hierarchy to find the closest non-about:-page
  // with the same origin as the precursor and return its URL.
  // TODO(https://crbug.com/1186321): This can return the incorrect result, e.g.
  // if a parent frame navigates a grandchild frame to about:blank.
  std::unique_ptr<FrameAdapter> parent = frame.Clone();
  GURL parent_url;
  base::flat_set<uintptr_t> already_visited_frame_ids;
  do {
    already_visited_frame_ids.insert(parent->GetId());
    parent = parent->GetLocalParentOrOpener();

    // We reached the end of the ancestral chain without finding a valid parent,
    // or found a remote web frame (in which case, it's a different origin).
    // Bail and use the original URL.
    if (!parent)
      return document_url;

    // Avoid an infinite loop - see https://crbug.com/568432 and
    // https://crbug.com/883526.
    if (base::Contains(already_visited_frame_ids, parent->GetId()))
      return document_url;

    url::SchemeHostPort parent_tuple_or_precursor_tuple =
        url::Origin(parent->GetOrigin()).GetTupleOrPrecursorTupleIfOpaque();
    if (!parent_tuple_or_precursor_tuple.IsValid() ||
        parent_tuple_or_precursor_tuple != tuple_or_precursor_tuple) {
      // The parent has a different tuple origin than frame; this could happen
      // in edge cases where a parent navigates an iframe or popup of a child
      // frame at a different origin. [1] In this case, bail, since we can't
      // find a full URL (i.e., one including the path) with the same security
      // origin to use for the frame in question.
      // [1] Consider a frame tree like:
      // <html> <!--example.com-->
      //   <iframe id="a" src="a.com">
      //     <iframe id="b" src="b.com"></iframe>
      //   </iframe>
      // </html>
      // Frame "a" is cross-origin from the top-level frame, and so the
      // example.com top-level frame can't directly access frame "b". However,
      // it can navigate it through
      // window.frames[0].frames[0].location.href = 'about:blank';
      // In that case, the precursor origin tuple origin of frame "b" would be
      // example.com, but the parent tuple origin is a.com.
      // Note that usually, this would have bailed earlier with a remote frame,
      // but it may not if we're at the process limit.
      return document_url;
    }

    // If we don't allow inaccessible parents, the security origin may still
    // be restricted if the author has prevented same-origin access via the
    // disallowdocumentaccess attribute on iframe.
    if (!allow_inaccessible_parents && !frame.CanAccess(*parent)) {
      // The frame can't access its precursor. Bail.
      return document_url;
    }

    parent_url = parent->GetUrl();
  } while (parent_url.SchemeIs(url::kAboutScheme));

  DCHECK(!parent_url.is_empty());

  // We should know that the frame can access the parent document (unless we
  // explicitly allow it not to), since it has the same tuple origin as the
  // frame, and we checked the frame access above.
  DCHECK(allow_inaccessible_parents || frame.CanAccess(parent->GetOrigin()));
  return parent_url;
}

}  // namespace extensions
