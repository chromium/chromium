// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/history_util.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

bool EqualIgnoringPathQueryAndFragment(const KURL& a, const KURL& b) {
  return StringView(a.GetString(), 0, a.PathStart()) ==
         StringView(b.GetString(), 0, b.PathStart());
}

bool EqualIgnoringQueryAndFragment(const KURL& a, const KURL& b) {
  return StringView(a.GetString(), 0, a.PathEnd()) ==
         StringView(b.GetString(), 0, b.PathEnd());
}

}  // namespace

bool CanChangeToUrlForHistoryApi(const KURL& url,
                                 const SecurityOrigin* document_origin,
                                 const KURL& document_url) {
  if (!url.IsValid())
    return false;

  bool can_change = true;
  scoped_refptr<const SecurityOrigin> requested_origin =
      SecurityOrigin::Create(url);

  // We allow sandboxed documents, `data:`/`file:` URLs, etc. to use
  // 'pushState'/'replaceState' to modify the URL fragment: see
  // https://crbug.com/528681 for the compatibility concerns.
  if (document_origin->IsOpaque() || document_origin->IsLocal()) {
    can_change = EqualIgnoringQueryAndFragment(url, document_url);
  } else if (!EqualIgnoringPathQueryAndFragment(url, document_url)) {
    can_change = false;
  } else if (requested_origin->IsOpaque() ||
             !requested_origin->IsSameOriginWith(document_origin)) {
    can_change = false;
  }

  if (document_origin->IsGrantedUniversalAccess()) {
    // Log the case when 'pushState'/'replaceState' is allowed only because
    // of IsGrantedUniversalAccess ie there is no other condition which should
    // allow the change (!can_change).
    base::UmaHistogramBoolean(
        "Android.WebView.UniversalAccess.OriginUrlMismatchInHistoryUtil",
        !can_change);
    return true;
  }
  return can_change;
}

}  // namespace blink
