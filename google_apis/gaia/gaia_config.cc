// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_config.h"

#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/threading/thread_restrictions.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/google_api_keys.h"
#include "url/gurl.h"

// static
GaiaConfig* GaiaConfig::GetInstance() {
  return GetGlobalConfig()->get();
}

GaiaConfig::GaiaConfig(base::Value::Dict parsed_config)
    : parsed_config_(std::move(parsed_config)) {}

GaiaConfig::~GaiaConfig() = default;

bool GaiaConfig::GetURLIfExists(std::string_view key, GURL* out_url) {
  const base::Value::Dict* urls = parsed_config_.FindDict("urls");
  if (!urls)
    return false;

  const base::Value::Dict* url_config = urls->FindDict(key);
  if (!url_config)
    return false;

  const std::string* url_string = url_config->FindString("url");
  if (!url_string) {
    LOG(ERROR) << "Incorrect format of \"" << key
               << "\" gaia config key. A key should contain {\"url\": "
                  "\"https://...\"} dictionary.";
    return false;
  }

  GURL url = GURL(*url_string);
  if (!url.is_valid()) {
    LOG(ERROR) << "Invalid URL at \"" << key << "\" URL key";
    return false;
  }

  *out_url = std::move(url);
  return true;
}

bool GaiaConfig::GetAPIKeyIfExists(std::string_view key,
                                   std::string* out_api_key) {
  const base::Value::Dict* api_keys = parsed_config_.FindDict("api_keys");
  if (!api_keys)
    return false;

  const std::string* api_key = api_keys->FindString(key);
  if (!api_key)
    return false;

  *out_api_key = *api_key;
  return true;
}

void GaiaConfig::SerializeContentsToCommandLineSwitch(
    base::CommandLine* command_line) const {
  std::string config_contents;
  base::JSONWriter::Write(parsed_config_, &config_contents);
  command_line->AppendSwitchASCII(switches::kGaiaConfigContents,
                                  config_contents);
}

// static
std::unique_ptr<GaiaConfig> GaiaConfig::CreateFromCommandLineForTesting(
    const base::CommandLine* command_line) {
  return ReadConfigFromCommandLineSwitches(command_line);
}

// static
void GaiaConfig::ResetInstanceForTesting() {
  *GetGlobalConfig() =
      ReadConfigFromCommandLineSwitches(base::CommandLine::ForCurrentProcess());
}

// static
std::unique_ptr<GaiaConfig>* GaiaConfig::GetGlobalConfig() {
  static base::NoDestructor<std::unique_ptr<GaiaConfig>> config(
      ReadConfigFromCommandLineSwitches(
          base::CommandLine::ForCurrentProcess()));
  return config.get();
}

// static
std::unique_ptr<GaiaConfig> GaiaConfig::ReadConfigFromString(
    const std::string& config_contents) {
  std::optional<base::Value> dict = base::JSONReader::Read(config_contents);
  if (!dict || !dict->is_dict()) {
    LOG(FATAL) << "Couldn't parse Gaia config file";
  }

  return std::make_unique<GaiaConfig>(std::move(dict->GetDict()));
}

// static
std::unique_ptr<GaiaConfig> GaiaConfig::ReadConfigFromDisk(
    const base::FilePath& config_path) {
  // Blocking is okay here because this code is executed only when the
  // --gaia-config command line flag is specified. --gaia-config is only used
  // for development.
  base::ScopedAllowBlocking scoped_allow_blocking;

  std::string config_contents;
  if (!base::ReadFileToString(config_path, &config_contents)) {
    LOG(FATAL) << "Couldn't read Gaia config file " << config_path;
  }
  return ReadConfigFromString(config_contents);
}

// static
std::unique_ptr<GaiaConfig> GaiaConfig::ReadConfigFromCommandLineSwitches(
    const base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kGaiaConfigPath) &&
      command_line->HasSwitch(switches::kGaiaConfigContents)) {
    LOG(FATAL) << "Either a Gaia config file path or a config file contents "
                  "can be provided; "
               << "not both";
  }

  if (command_line->HasSwitch(switches::kGaiaConfigContents)) {
    return ReadConfigFromString(
        command_line->GetSwitchValueASCII(switches::kGaiaConfigContents));
  }

  if (command_line->HasSwitch(switches::kGaiaConfigPath)) {
    return ReadConfigFromDisk(
        command_line->GetSwitchValuePath(switches::kGaiaConfigPath));
  }

  return nullptr;
}
