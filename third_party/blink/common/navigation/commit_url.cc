// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/navigation/commit_url.h"

#include "base/metrics/histogram_functions.h"
#include "url/url_constants.h"

namespace blink {

bool IsValidCommitUrl(const GURL& url) {
  // Invalid URLs are not accepted by the browser process.
  if (!url.is_valid()) {
    base::UmaHistogramEnumeration("Navigation.Renderer.BlockedForFilterURL",
                                  RendererBlockedURLReason::kInvalidURL);
    return false;
  }

  // Do not send a URL longer than Mojo will serialize.
  if (url.possibly_invalid_spec().length() > url::kMaxURLChars) {
    base::UmaHistogramEnumeration("Navigation.Renderer.BlockedForFilterURL",
                                  RendererBlockedURLReason::kTooLongURL);
    return false;
  }

  // Any about: URLs must be either about:blank or about:srcdoc (optionally with
  // fragments).
  if (url.SchemeIs(url::kAboutScheme) &&
      (!url.IsAboutBlank() && !url.IsAboutSrcdoc())) {
    base::UmaHistogramEnumeration("Navigation.Renderer.BlockedForFilterURL",
                                  RendererBlockedURLReason::kBadAboutURL);
    return false;
  }

  return true;
}

}  // namespace blink
