// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/config_reader.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace cr_fuchsia {

namespace {

absl::optional<base::Value> ReadPackageConfig() {
  constexpr char kConfigPath[] = "/config/data/config.json";

  base::FilePath path(kConfigPath);
  if (!base::PathExists(path)) {
    LOG(WARNING) << "Config file doesn't exist: " << path.value();
    return absl::nullopt;
  }

  std::string file_content;
  bool loaded = base::ReadFileToString(path, &file_content);
  if (!loaded) {
    LOG(WARNING) << "Couldn't read config file: " << path.value();
    return absl::nullopt;
  }

  base::JSONReader::ValueWithError parsed =
      base::JSONReader::ReadAndReturnValueWithError(file_content);
  CHECK(parsed.value) << "Failed to parse " << path.value() << ": "
                      << parsed.error_message;
  CHECK(parsed.value->is_dict())
      << "Config is not a JSON dictionary: " << path.value();

  return std::move(parsed.value);
}

}  // namespace

const absl::optional<base::Value>& LoadPackageConfig() {
  // Package configurations do not change at run-time, so read the configuration
  // on the first call and cache the result.
  static base::NoDestructor<absl::optional<base::Value>> config(
      ReadPackageConfig());
  return *config;
}

}  // namespace cr_fuchsia
