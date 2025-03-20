// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/fingerprinting_protection/canvas_noise_token.h"

namespace blink {

namespace {
uint64_t noise_token_ = 0;
}  // namespace

// static
void CanvasNoiseToken::Set(uint64_t token) {
  noise_token_ = token;
}

// static
uint64_t CanvasNoiseToken::Get() {
  return noise_token_;
}

}  // namespace blink
