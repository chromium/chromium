// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_COMMIT_URL_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_COMMIT_URL_H_

#include "third_party/blink/public/common/common_export.h"
#include "url/gurl.h"

namespace blink {

// Special URL used to rewrite URLs coming from renderer processes when the
// requested URL is not allowed to be committed (must match
// `content::kBlockedURL`).
inline constexpr char kBlockedURL[] = "about:blank#blocked";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(RendererBlockedURLReason)
enum class RendererBlockedURLReason {
  kInvalidURL = 0,
  kTooLongURL = 1,
  kBadAboutURL = 2,

  kMaxValue = kBadAboutURL
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:RendererBlockedURLReason)

// Ensures that the renderer does not send commit URLs to the browser process
// that are known to be unsupported. Ideally these would be caught earlier in
// Blink and not get this far. Histograms are reported (similar to those in
// RenderProcessHostImpl::FilterURL) to track the cases that should be handled
// earlier. See https://crbug.com/40066983.
BLINK_COMMON_EXPORT bool IsValidCommitUrl(const GURL& url);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_COMMIT_URL_H_
