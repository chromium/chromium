// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_TRIAL_TOKENS_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_TRIAL_TOKENS_HANDLER_H_

#include <set>
#include <string>

#include "base/containers/span.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// A structure to hold the set of tokens provided by this extension.
struct TrialTokens : public Extension::ManifestData {
  TrialTokens(std::set<std::string> tokens);

  TrialTokens(const TrialTokens&) = delete;
  TrialTokens& operator=(const TrialTokens&) = delete;

  TrialTokens(TrialTokens&& other);

  ~TrialTokens() override;

  static const std::set<std::string>* GetTrialTokens(
      const Extension& extension);

  static bool HasTrialTokens(const Extension& extension);

 private:
  // A set of trial tokens used by this extension.
  std::set<std::string> tokens_;
};

// Parses the "trust_tokens" manifest key.
class TrialTokensHandler : public ManifestHandler {
 public:
  TrialTokensHandler();

  TrialTokensHandler(const TrialTokensHandler&) = delete;
  TrialTokensHandler& operator=(const TrialTokensHandler&) = delete;

  ~TrialTokensHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_TRIAL_TOKENS_HANDLER_H_
