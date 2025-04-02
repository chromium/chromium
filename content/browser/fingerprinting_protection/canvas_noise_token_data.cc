// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fingerprinting_protection/canvas_noise_token_data.h"

#include <cstdint>
#include <memory>

#include "base/feature_list.h"
#include "base/numerics/byte_conversions.h"
#include "base/supports_user_data.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "content/public/browser/browser_context.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {
const void* const kBrowserContextCanvasNoiseTokenKey =
    &kBrowserContextCanvasNoiseTokenKey;
}  // namespace

// static
uint64_t CanvasNoiseTokenData::GetToken(BrowserContext* context) {
  CHECK(base::FeatureList::IsEnabled(
      fingerprinting_protection_interventions::features::kCanvasNoise));

  CanvasNoiseTokenData* data = static_cast<CanvasNoiseTokenData*>(
      context->GetUserData(&kBrowserContextCanvasNoiseTokenKey));
  if (data != nullptr) {
    return data->session_token_;
  }
  return CanvasNoiseTokenData::SetNewToken(context);
}

// static
uint64_t CanvasNoiseTokenData::SetNewToken(BrowserContext* context) {
  CHECK(base::FeatureList::IsEnabled(
      fingerprinting_protection_interventions::features::kCanvasNoise));

  auto new_data = std::make_unique<CanvasNoiseTokenData>();
  uint64_t token = new_data->session_token_;
  context->SetUserData(&kBrowserContextCanvasNoiseTokenKey,
                       std::move(new_data));
  return token;
}

}  // namespace content
