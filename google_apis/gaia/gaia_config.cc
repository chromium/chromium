// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_config.h"
#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/google_api_keys.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {

std::unique_ptr<GaiaConfig> ReadConfigFromDisk() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kGaiaConfig))
    return nullptr;

  const base::FilePath config_path =
      command_line->GetSwitchValuePath(switches::kGaiaConfig);
  std::string config_contents;
  if (!base::ReadFileToString(config_path, &config_contents)) {
    LOG(ERROR) << "Couldn't read gaia config file " << config_path;
    return nullptr;
  }

  absl::optional<base::Value> dict = base::JSONReader::Read(config_contents);
  if (!dict || !dict->is_dict()) {
    LOG(ERROR) << "Couldn't parse gaia config file " << config_path;
    return nullptr;
  }

  return std::make_unique<GaiaConfig>(std::move(dict.value()));
}

std::unique_ptr<GaiaConfig>* GetGlobalConfig() {
  static base::NoDestructor<std::unique_ptr<GaiaConfig>> config(
      ReadConfigFromDisk());
  return config.get();
}

}  // namespace

// static
GaiaConfig* GaiaConfig::GetInstance() {
  return GetGlobalConfig()->get();
}

GaiaConfig::GaiaConfig(base::Value parsed_config)
    : parsed_config_(std::move(parsed_config)) {}

GaiaConfig::~GaiaConfig() = default;

bool GaiaConfig::GetURLIfExists(base::StringPiece key, GURL* out_url) {
  const base::Value* urls = parsed_config_.FindDictKey("urls");
  if (!urls)
    return false;

  const base::Value* url_config = urls->FindDictKey(key);
  if (!url_config)
    return false;

  const std::string* url_string = url_config->FindStringKey("url");
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

  *out_url = url;
  return true;
}

bool GaiaConfig::GetAPIKeyIfExists(base::StringPiece key,
                                   std::string* out_api_key) {
  const base::Value* api_keys = parsed_config_.FindDictKey("api_keys");
  if (!api_keys)
    return false;

  const std::string* api_key = api_keys->FindStringKey(key);
  if (!api_key)
    return false;

  *out_api_key = *api_key;
  return true;
}

// static
void GaiaConfig::ResetInstanceForTesting() {
  *GetGlobalConfig() = ReadConfigFromDisk();
}
