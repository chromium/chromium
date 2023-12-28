// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/trial_tokens_handler.h"

#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

// The maximum number of tokens which will be processed
// This value should be sufficiently large to avoid any issues in practice,
// but small enough to bound resource consumption to something reasonable
const size_t kMaxTokenCount = 100;
// The maximum length of a single token
// Keep this value in sync with the value of same name in
// third_party/blink/public/common/origin_trials/trial_token.cc
const size_t kMaxTokenSize = 6144;

const TrialTokens* GetTokens(const Extension& extension) {
  return static_cast<const TrialTokens*>(
      extension.GetManifestData(manifest_keys::kTrialTokens));
}

}  // namespace

TrialTokens::TrialTokens() = default;
TrialTokens::TrialTokens(TrialTokens&& other) = default;
TrialTokens::~TrialTokens() = default;

// static
const std::set<std::string>* TrialTokens::GetTrialTokens(
    const Extension& extension) {
  const auto* tokens = GetTokens(extension);
  if (!tokens) {
    return nullptr;
  }
  return &tokens->tokens;
}

// static
bool TrialTokens::HasTrialTokens(const Extension& extension) {
  return GetTokens(extension);
}

TrialTokensHandler::TrialTokensHandler() = default;
TrialTokensHandler::~TrialTokensHandler() = default;

bool TrialTokensHandler::Parse(Extension* extension, std::u16string* error) {
  const base::Value* trial_tokens = nullptr;
  if (!extension->manifest()->GetList(manifest_keys::kTrialTokens,
                                      &trial_tokens) ||
      trial_tokens->GetList().empty()) {
    *error = manifest_errors::kInvalidTrialTokensNonEmptyList;
    return false;
  }

  size_t processedTokenCount = 0;
  auto tokens = std::make_unique<TrialTokens>();
  for (const auto& token : trial_tokens->GetList()) {
    // Avoid processing an arbitrarily large number of trial tokens.
    if (++processedTokenCount > kMaxTokenCount) {
      extension->AddInstallWarning(extensions::InstallWarning(
          base::StringPrintf(manifest_errors::kInvalidTrialTokensTooManyTokens,
                             kMaxTokenCount),
          manifest_keys::kTrialTokens));
      break;
    }

    // Error out on non-string token or empty string
    if (!token.is_string() || token.GetString().empty()) {
      *error = manifest_errors::kInvalidTrialTokensValue;
      return false;
    }
    // Add a warning for a long token and skip it
    if (token.GetString().length() > kMaxTokenSize) {
      extension->AddInstallWarning(extensions::InstallWarning(
          base::StringPrintf(manifest_errors::kInvalidTrialTokensValueTooLong,
                             kMaxTokenSize),
          manifest_keys::kTrialTokens));
      continue;
    }

    tokens->tokens.insert(token.GetString());
  }

  extension->SetManifestData(manifest_keys::kTrialTokens, std::move(tokens));
  return true;
}

base::span<const char* const> TrialTokensHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kTrialTokens};
  return kKeys;
}

}  // namespace extensions
