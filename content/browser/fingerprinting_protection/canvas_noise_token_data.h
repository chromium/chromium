// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FINGERPRINTING_PROTECTION_CANVAS_NOISE_TOKEN_DATA_H_
#define CONTENT_BROWSER_FINGERPRINTING_PROTECTION_CANVAS_NOISE_TOKEN_DATA_H_

#include <cstdint>

#include "base/rand_util.h"
#include "content/public/browser/browser_context.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

namespace content {

// A user data class that generates and stores BrowserContext-associated noise
// tokens used for canvas noising.
class CONTENT_EXPORT CanvasNoiseTokenData
    : public base::SupportsUserData::Data {
 public:
  CanvasNoiseTokenData() = default;

  // Gets the 64 bit BrowserContext-associated noise token.
  static uint64_t GetToken(BrowserContext* context);

  // Regenerates the noise token, returning the updated token value.
  static uint64_t SetNewToken(BrowserContext* context);

 private:
  uint64_t session_token_ = base::RandUint64();
};

}  // namespace content

#endif  // CONTENT_BROWSER_FINGERPRINTING_PROTECTION_CANVAS_NOISE_TOKEN_DATA_H_
