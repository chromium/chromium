// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_TTC_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_TTC_API_H_

#include <string>

namespace ios::provider {

// Configuration parameters for TTC live voice interactions.
struct TTCConfig {
  // Master system prompt instructing the model on behavior and persona.
  std::string system_instruction{};

  // Model resource identifier (e.g. "models/sample-live-model").
  std::string model{};

  // Voice persona identifier for audio synthesis (e.g. "SampleVoice").
  std::string voice_name{};

  // Full WebSocket endpoint URL for the bidirectional streaming service.
  std::string endpoint_url{};

  // API key for authenticating with the streaming service.
  std::string api_key{};

  // Returns true if the configuration has an endpoint URL and model specified.
  bool is_valid() const { return !endpoint_url.empty() && !model.empty(); }

  // Returns true if all fields are empty (e.g. unconfigured open-source build).
  bool empty() const {
    return system_instruction.empty() && model.empty() && voice_name.empty() &&
           endpoint_url.empty() && api_key.empty();
  }

  friend bool operator==(const TTCConfig&, const TTCConfig&) = default;
};

// Returns the TTC configuration if compiled with internal providers or test
// mocks; returns empty fields in open-source Chromium builds.
TTCConfig GetTTCConfig();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_TTC_API_H_
