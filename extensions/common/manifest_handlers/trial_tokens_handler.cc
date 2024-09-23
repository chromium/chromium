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

TrialTokens::TrialTokens(std::set<std::string> tokens)
    : tokens_(std::move(tokens)) {}

TrialTokens::TrialTokens(TrialTokens&& other) = default;
TrialTokens::~TrialTokens() = default;

// static
const std::set<std::string>* TrialTokens::GetTrialTokens(
    const Extension& extension) {
  const auto* tokens = GetTokens(extension);
  if (!tokens) {
    return nullptr;
  }
  DCHECK(!tokens->tokens_.empty());
  return &tokens->tokens_;
}

// static
bool TrialTokens::HasTrialTokens(const Extension& extension) {
  return GetTokens(extension);
}

TrialTokensHandler::TrialTokensHandler() = default;
TrialTokensHandler::~TrialTokensHandler() = default;

bool TrialTokensHandler::Parse(Extension* extension, std::u16string* error) {
  const base::Value* trial_tokens = nullptr;
  // 'trial_tokens' must be a non-empty list. Otherwise, log a benign warning
  if (!extension->manifest()->GetList(manifest_keys::kTrialTokens,
                                      &trial_tokens) ||
      trial_tokens->GetList().empty()) {
    extension->AddInstallWarning(
        InstallWarning(manifest_errors::kInvalidTrialTokensNonEmptyList,
                       manifest_keys::kTrialTokens));
    return true;
  }

  std::set<std::string> tokens;
  for (const auto& token : trial_tokens->GetList()) {
    // Avoid processing an arbitrarily large number of trial tokens
    if (tokens.size() >= kMaxTokenCount) {
      extension->AddInstallWarning(extensions::InstallWarning(
          base::StringPrintf(manifest_errors::kInvalidTrialTokensTooManyTokens,
                             kMaxTokenCount),
          manifest_keys::kTrialTokens));
      break;
    }

    // Skip non-string token or empty string and add a benign warning
    if (!token.is_string() || token.GetString().empty()) {
      extension->AddInstallWarning(
          InstallWarning(manifest_errors::kInvalidTrialTokensValue,
                         manifest_keys::kTrialTokens));
      continue;
    }

    // Skip excessively long long token and add a benign warning
    if (token.GetString().length() > kMaxTokenSize) {
      extension->AddInstallWarning(extensions::InstallWarning(
          base::StringPrintf(manifest_errors::kInvalidTrialTokensValueTooLong,
                             kMaxTokenSize),
          manifest_keys::kTrialTokens));
      continue;
    }

    // Warn about duplicate tokens
    if (tokens.contains(token.GetString())) {
      extension->AddInstallWarning(extensions::InstallWarning(
          base::StringPrintf(manifest_errors::kInvalidTrialTokensValueDuplicate,
                             token.GetString().c_str()),
          manifest_keys::kTrialTokens));
      continue;
    }

    // TODO(crbug.com/40282364): Add validation of trial token contents, log an
    // InstallWarning and skip the token here

    tokens.insert(token.GetString());
  }

  if (tokens.empty()) {
    // If we did not find a single valid token, do not save anything
    return true;
  }

  extension->SetManifestData(manifest_keys::kTrialTokens,
                             std::make_unique<TrialTokens>(std::move(tokens)));
  return true;
}

base::span<const char* const> TrialTokensHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kTrialTokens};
  return kKeys;
}

}  // namespace extensions
