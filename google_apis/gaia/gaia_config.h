// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_CONFIG_H_
#define GOOGLE_APIS_GAIA_GAIA_CONFIG_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "google_apis/google_api_keys.h"

class GURL;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

// Class representing a configuration for Gaia URLs and Google API keys.
// Parses a JSON config file specified by |switches::kGaiaConfigPath| or
// |switches::kGaiaConfigContents| and provides convenient getters for reading
// this config.
//
// The config is represented by a JSON object with the following structure:
// {
//   "urls": {
//     "gaia_url": {
//       "url": "https://accounts.example.com"
//     },
//     ...
//   },
//   "api_keys": {
//     "GOOGLE_CLIENT_ID_MAIN": "example_key",
//     ...
//   }
// }
class COMPONENT_EXPORT(GOOGLE_APIS) GaiaConfig {
 public:
  // Returns a global instance of GaiaConfig.
  // This may return nullptr if the config file was not specified by a command
  // line parameter.
  static GaiaConfig* GetInstance();

  // Constructs a new GaiaConfig from a parsed JSON dictionary.
  // Prefer GetInstance() over this constructor.
  explicit GaiaConfig(base::Value::Dict parsed_config);
  GaiaConfig(const GaiaConfig&) = delete;
  GaiaConfig& operator=(const GaiaConfig&) = delete;
  ~GaiaConfig();

  // Searches for a URL by |key|.
  // Returns true if |key| exists and contains a valid URL. |out_url| will be
  // set to that URL.
  // Otherwise, returns false. |out_url| will be unmodified.
  bool GetURLIfExists(std::string_view key, GURL* out_url);

  // Searches for an API key, OAuth2 client ID or secret by |key|.
  // Returns true if |key| exists and contains a valid string.
  // |out_api_key| will be set to that string.
  // Otherwise, returns false. |out_api_key| will be unmodified.
  bool GetAPIKeyIfExists(std::string_view key, std::string* out_api_key);

  // Serializes the state of |this| into |command_line|, in a way that
  // GaiaConfig::GetInstance() would honor. Internally, it uses switch
  // |kGaiaConfigContents| for this purpose, which is appended to
  // |*command_line|.
  void SerializeContentsToCommandLineSwitch(
      base::CommandLine* command_line) const;

  // Instantiates this object given |base::CommandLine|, used in tests.
  static std::unique_ptr<GaiaConfig> CreateFromCommandLineForTesting(
      const base::CommandLine* command_line);

 private:
  friend class GaiaUrlsOverriderForTesting;
  friend class GaiaUrlsTest;
  FRIEND_TEST_ALL_PREFIXES(GoogleAPIKeysTest, OverrideAllKeysUsingConfig);

  static std::unique_ptr<GaiaConfig>* GetGlobalConfig();

  static std::unique_ptr<GaiaConfig> ReadConfigFromString(
      const std::string& config_contents);
  static std::unique_ptr<GaiaConfig> ReadConfigFromDisk(
      const base::FilePath& config_path);
  static std::unique_ptr<GaiaConfig> ReadConfigFromCommandLineSwitches(
      const base::CommandLine* command_line);

  // Re-reads the config from disk and resets the global instance of GaiaConfig.
  static void ResetInstanceForTesting();

  base::Value::Dict parsed_config_;
};

#endif  // GOOGLE_APIS_GAIA_GAIA_CONFIG_H_
