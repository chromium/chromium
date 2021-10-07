// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/config_reader.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace cr_fuchsia {

namespace {

bool HaveConflicts(const base::Value& dict1, const base::Value& dict2) {
  for (auto item : dict1.DictItems()) {
    const base::Value* value = dict2.FindKey(item.first);
    if (!value)
      continue;
    if (!value->is_dict())
      return true;
    if (HaveConflicts(item.second, *value))
      return true;
  }

  return false;
}

base::Value ReadConfigFile(const base::FilePath& path) {
  std::string file_content;
  bool loaded = base::ReadFileToString(path, &file_content);
  CHECK(loaded) << "Couldn't read config file: " << path;

  base::JSONReader::ValueWithError parsed =
      base::JSONReader::ReadAndReturnValueWithError(file_content);
  CHECK(parsed.value) << "Failed to parse " << path << ": "
                      << parsed.error_message;
  CHECK(parsed.value->is_dict()) << "Config is not a JSON dictionary: " << path;

  return std::move(*parsed.value);
}

absl::optional<base::Value> ReadConfigsFromDir(const base::FilePath& dir) {
  base::FileEnumerator configs(dir, false, base::FileEnumerator::FILES,
                               "*.json");
  absl::optional<base::Value> config;
  for (base::FilePath path; !(path = configs.Next()).empty();) {
    base::Value path_config = ReadConfigFile(path);
    if (config) {
      CHECK(!HaveConflicts(*config, path_config));
      config->MergeDictionary(&path_config);
    } else {
      config = std::move(path_config);
    }
  }

  return config;
}

}  // namespace

const absl::optional<base::Value>& LoadPackageConfig() {
  // Package configurations do not change at run-time, so read the configuration
  // on the first call and cache the result.
  static base::NoDestructor<absl::optional<base::Value>> config(
      ReadConfigsFromDir(base::FilePath("/config/data")));

  return *config;
}

absl::optional<base::Value> LoadConfigFromDirForTest(  // IN-TEST
    const base::FilePath& dir) {
  return ReadConfigsFromDir(dir);
}

}  // namespace cr_fuchsia
