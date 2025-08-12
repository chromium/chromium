// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fingerprinting_protection/canvas_noise_token_data.h"

#include <cstdint>
#include <memory>
#include <string_view>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "content/public/browser/browser_context.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

namespace content {

namespace {
const void* const kBrowserContextCanvasNoiseTokenKey =
    &kBrowserContextCanvasNoiseTokenKey;

// FNV constants
// https://datatracker.ietf.org/doc/html/draft-eastlake-fnv#name-fnv-constants
constexpr uint64_t kFnvPrime = 0x00000100000001b3;
constexpr uint64_t kFnvOffset = 0xcbf29ce484222325;

uint64_t DeriveInitialNoiseHash(uint64_t token, std::string_view domain) {
  uint64_t token_hash = kFnvOffset;
  auto hasher = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  hasher->Update(base::U64ToLittleEndian(token));
  hasher->Update(base::as_byte_span(domain));
  std::array<uint8_t, crypto::kSHA256Length> digest;
  hasher->Finish(digest);
  token_hash ^= base::U64FromLittleEndian(base::span(digest).first<8>());
  token_hash *= kFnvPrime;
  return token_hash;
}
}  // namespace

// static
uint64_t CanvasNoiseTokenData::GetBrowserToken(BrowserContext* context) {
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
uint64_t CanvasNoiseTokenData::GetToken(BrowserContext* context,
                                        const url::Origin& origin) {
  if (!origin.opaque()) {
    return DeriveInitialNoiseHash(GetBrowserToken(context), origin.Serialize());
  }
  return DeriveInitialNoiseHash(GetBrowserToken(context),
                                base::UnguessableToken::Create().ToString());
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
