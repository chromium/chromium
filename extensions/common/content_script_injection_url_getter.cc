// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/content_script_injection_url_getter.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/trace_event/typed_macros.h"
#include "url/scheme_host_port.h"

namespace extensions {

// static
GURL ContentScriptInjectionUrlGetter::Get(
    const FrameContextData& context_data,
    const GURL& document_url,
    MatchOriginAsFallbackBehavior match_origin_as_fallback,
    bool allow_inaccessible_parents) {
  // The following schemes are considered for opaque origins if the
  // `match_origin_as_fallback` behavior is to always match.
  // NOTE(devlin): This isn't an exhaustive list of schemes: some schemes may
  // be missing, or more schemes may be added in the future. Would it make
  // sense to turn this into a blocklist? Just doing this for all opaque
  // schemes *should* be safe, since We still have a permission check against
  // the precursor origin. This would only be a problem if an
  // extension-accessible precursor origin can create an opaque-origin frame
  // that *shouldn't* be accessible.
  static const char* const kAllowedSchemesToMatchOriginAsFallback[] = {
      url::kAboutScheme,
      url::kBlobScheme,
      url::kDataScheme,
      url::kFileSystemScheme,
  };

  // TODO(crbug.com/40055997): Consider reducing tracing instrumentation
  // in the main function bodu and in the lambda below (once the bug is
  // understood and fixed).
  auto should_consider_origin = [&document_url, match_origin_as_fallback]() {
    bool result = false;
    switch (match_origin_as_fallback) {
      case MatchOriginAsFallbackBehavior::kNever: {
        TRACE_EVENT_INSTANT("extensions",
                            "ContentScriptInjectionUrlGetter::Get/"
                            "should_consider_origin: origin-never");
        result = false;
        break;
      }
      case MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree: {
        TRACE_EVENT_INSTANT("extensions",
                            "ContentScriptInjectionUrlGetter::Get/"
                            "should_consider_origin: origin-climb");
        result = document_url.SchemeIs(url::kAboutScheme);
        break;
      }
      case MatchOriginAsFallbackBehavior::kAlways: {
        TRACE_EVENT_INSTANT("extensions",
                            "ContentScriptInjectionUrlGetter::Get/"
                            "should_consider_origin: origin-always");
        result = base::Contains(kAllowedSchemesToMatchOriginAsFallback,
                                document_url.scheme());
        break;
      }
    }
    if (result) {
      TRACE_EVENT_INSTANT("extensions",
                          "ContentScriptInjectionUrlGetter::Get/"
                          "should_consider_origin=true");
    } else {
      TRACE_EVENT_INSTANT("extensions",
                          "ContentScriptInjectionUrlGetter::Get/"
                          "should_consider_origin=false");
    }
    return result;
  };

  // If we don't need to consider the origin, we're done.
  if (!should_consider_origin()) {
    TRACE_EVENT_INSTANT(
        "extensions", "ContentScriptInjectionUrlGetter::Get/!consider-origin");
    return document_url;
  }

  // Get the security origin for the `frame`. For about: frames, this is the
  // origin of that of the controlling frame - e.g., an about:blank frame on
  // https://example.com will have the security origin of https://example.com.
  // Other frames, like data: frames, will have an opaque origin. For these,
  // we can get the precursor origin.
  const url::Origin frame_origin = context_data.GetOrigin();
  const url::SchemeHostPort& tuple_or_precursor_tuple =
      frame_origin.GetTupleOrPrecursorTupleIfOpaque();

  // When there's no valid tuple (which can happen in the case of e.g. a
  // browser-initiated navigation to an opaque URL), there's no origin to
  // fallback to. Bail.
  if (!tuple_or_precursor_tuple.IsValid()) {
    TRACE_EVENT_INSTANT("extensions",
                        "ContentScriptInjectionUrlGetter::Get/invalid-tuple");
    return document_url;
  }

  const url::Origin origin_or_precursor_origin =
      url::Origin::Create(tuple_or_precursor_tuple.GetURL());

  if (!allow_inaccessible_parents &&
      !context_data.CanAccess(origin_or_precursor_origin)) {
    // The `context_data` can't access its precursor. Bail.
    TRACE_EVENT_INSTANT(
        "extensions",
        "ContentScriptInjectionUrlGetter::Get/no-precursor-access");
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
    TRACE_EVENT_INSTANT(
        "extensions",
        "ContentScriptInjectionUrlGetter::Get/origin-or-precursor");
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
  // TODO(crbug.com/40753677): This can return the incorrect result, e.g.
  // if a parent frame navigates a grandchild frame to about:blank.
  std::unique_ptr<FrameContextData> parent_context_data =
      context_data.CloneFrameContextData();
  GURL parent_url;
  base::flat_set<uintptr_t> already_visited_frame_ids;
  do {
    already_visited_frame_ids.insert(parent_context_data->GetId());
    parent_context_data = parent_context_data->GetLocalParentOrOpener();

    // We reached the end of the ancestral chain without finding a valid parent,
    // or found a remote web frame (in which case, it's a different origin).
    // Bail and use the original URL.
    if (!parent_context_data) {
      TRACE_EVENT_INSTANT(
          "extensions", "ContentScriptInjectionUrlGetter::Get/no-more-parents");
      return document_url;
    }

    // Avoid an infinite loop - see https://crbug.com/568432 and
    // https://crbug.com/883526.
    if (base::Contains(already_visited_frame_ids,
                       parent_context_data->GetId())) {
      TRACE_EVENT_INSTANT("extensions",
                          "ContentScriptInjectionUrlGetter::Get/infinite-loop");
      return document_url;
    }

    url::SchemeHostPort parent_tuple_or_precursor_tuple =
        url::Origin(parent_context_data->GetOrigin())
            .GetTupleOrPrecursorTupleIfOpaque();
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
      TRACE_EVENT_INSTANT("extensions",
                          "ContentScriptInjectionUrlGetter::Get/tuple-diff");
      return document_url;
    }

    // If we don't allow inaccessible parents, the security origin may still
    // be restricted if the author has prevented same-origin access via the
    // disallowdocumentaccess attribute on iframe.
    if (!allow_inaccessible_parents &&
        !context_data.CanAccess(*parent_context_data)) {
      // The frame can't access its precursor. Bail.
      TRACE_EVENT_INSTANT(
          "extensions",
          "ContentScriptInjectionUrlGetter::Get/no-parent-access");
      return document_url;
    }

    parent_url = parent_context_data->GetUrl();
  } while (parent_url.SchemeIs(url::kAboutScheme));

  DCHECK(!parent_url.is_empty());

  // We should know that the frame can access the parent document (unless we
  // explicitly allow it not to), since it has the same tuple origin as the
  // frame, and we checked the frame access above.
  TRACE_EVENT_INSTANT("extensions",
                      "ContentScriptInjectionUrlGetter::Get/parent-url");
  DCHECK(allow_inaccessible_parents ||
         context_data.CanAccess(parent_context_data->GetOrigin()));
  return parent_url;
}

}  // namespace extensions
