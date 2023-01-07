// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/history_util.h"

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

  // We allow sandboxed documents, `data:`/`file:` URLs, etc. to use
  // 'pushState'/'replaceState' to modify the URL fragment: see
  // https://crbug.com/528681 for the compatibility concerns.
  if (document_origin->IsOpaque() || document_origin->IsLocal())
    return EqualIgnoringQueryAndFragment(url, document_url);
  if (!EqualIgnoringPathQueryAndFragment(url, document_url))
    return false;

  scoped_refptr<const SecurityOrigin> requested_origin =
      SecurityOrigin::Create(url);
  if (requested_origin->IsOpaque() ||
      !requested_origin->IsSameOriginWith(document_origin)) {
    return false;
  }
  return true;
}

}  // namespace blink
