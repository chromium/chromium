// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FINGERPRINTING_PROTECTION_CANVAS_NOISE_TOKEN_DATA_H_
#define CONTENT_BROWSER_FINGERPRINTING_PROTECTION_CANVAS_NOISE_TOKEN_DATA_H_

#include <cstdint>

#include "base/rand_util.h"
#include "content/public/browser/browser_context.h"
#include "third_party/blink/public/common/fingerprinting_protection/noise_token.h"
#include "url/origin.h"

namespace content {

// TODO(https://crbug.com/442616874): Key CanvasNoiseTokens by (BrowserContext,
// StorageKey) instead of (BrowserContext, Origin).

// A user data class that generates and stores BrowserContext-associated noise
// tokens used for canvas noising.
class CONTENT_EXPORT CanvasNoiseTokenData
    : public base::SupportsUserData::Data {
 public:
  CanvasNoiseTokenData() = default;

  // Gets the 64 bit BrowserContext-associated noise token computed with the
  // main frame's |origin|. If the origin is opaque, a random value will be
  // used.
  static blink::NoiseToken GetToken(BrowserContext* context,
                                    const url::Origin& origin);

  // Regenerates the noise token, returning the updated token value.
  static blink::NoiseToken SetNewToken(BrowserContext* context);

 private:
  // Helper to generate the 64 bit BrowserContext-associated token, which will
  // be different per BrowserContext.
  static blink::NoiseToken GetBrowserToken(BrowserContext* context);

  blink::NoiseToken session_token_{base::RandUint64()};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FINGERPRINTING_PROTECTION_CANVAS_NOISE_TOKEN_DATA_H_
