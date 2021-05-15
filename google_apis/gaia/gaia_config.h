// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_CONFIG_H_
#define GOOGLE_APIS_GAIA_GAIA_CONFIG_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "google_apis/google_api_keys.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

// Class representing a configuration for Gaia URLs and Google API keys.
// Parses a JSON config file specified by |switches::kGaiaConfig| and provides
// convenient getters for reading this config.
//
// The config is represented by a JSON object with the following structure:
// {
//   "urls": {
//     "gaia_url": {
//       "url": "https://accounts.example.com"
//     },
//     ...
//   }
//   "api_keys": {
//     "GOOGLE_CLIENT_ID_MAIN": "example_key",
//     ...
//   }
// }
class GaiaConfig {
 public:
  // Returns a global instance of GaiaConfig.
  // This may return nullptr if the config file was not specified by a command
  // line parameter or couldn't be parsed correctly.
  static GaiaConfig* GetInstance();

  // Constructs a new GaiaConfig from a parsed JSON dictionary.
  // Prefer GetInstance() over this constructor.
  explicit GaiaConfig(base::Value parsed_config);
  ~GaiaConfig();

  // Searches for a URL by |key|.
  // Returns true if |key| exists and contains a valid URL. |out_url| will be
  // set to that URL.
  // Otherwise, returns false. |out_url| will be unmodified.
  bool GetURLIfExists(base::StringPiece key, GURL* out_url);

  // Searches for an API key, OAuth2 client ID or secret by |key|.
  // Returns true if |key| exists and contains a valid string.
  // |out_api_key| will be set to that string.
  // Otherwise, returns false. |out_api_key| will be unmodified.
  bool GetAPIKeyIfExists(base::StringPiece key, std::string* out_api_key);

 private:
  friend class GaiaUrlsTest;
  FRIEND_TEST_ALL_PREFIXES(GoogleAPIKeysTest, OverrideAllKeysUsingConfig);

  // Re-reads the config from disk and resets the global instance of GaiaConfig.
  static void ResetInstanceForTesting();

  base::Value parsed_config_;
};

#endif  // GOOGLE_APIS_GAIA_GAIA_CONFIG_H_
