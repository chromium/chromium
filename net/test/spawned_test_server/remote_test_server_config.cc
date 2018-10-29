// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/spawned_test_server/remote_test_server_config.h"

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "url/gurl.h"

#if defined(OS_FUCHSIA)
#include "base/base_paths_fuchsia.h"
#endif

namespace net {

namespace {

base::FilePath GetTestServerConfigFilePath() {
  base::FilePath dir;
#if defined(OS_ANDROID)
  base::PathService::Get(base::DIR_ANDROID_EXTERNAL_STORAGE, &dir);
#elif defined(OS_FUCHSIA)
  dir = base::FilePath("/test-shared");
#else
  base::PathService::Get(base::DIR_TEMP, &dir);
#endif
  return dir.AppendASCII("net-test-server-config");
}

}  // namespace

RemoteTestServerConfig::RemoteTestServerConfig() = default;
RemoteTestServerConfig::~RemoteTestServerConfig() {}

RemoteTestServerConfig::RemoteTestServerConfig(
    const RemoteTestServerConfig& other) = default;
RemoteTestServerConfig& RemoteTestServerConfig::operator=(
    const RemoteTestServerConfig&) = default;

RemoteTestServerConfig RemoteTestServerConfig::Load() {
  base::ScopedAllowBlockingForTesting allow_blocking;

  RemoteTestServerConfig result;

  base::FilePath config_path = GetTestServerConfigFilePath();

  // Use defaults if the file doesn't exists.
  if (!base::PathExists(config_path))
    return result;

  std::string config_json;
  if (!ReadFileToString(config_path, &config_json))
    LOG(FATAL) << "Failed to read " << config_path.value();

  std::unique_ptr<base::DictionaryValue> config =
      base::DictionaryValue::From(base::JSONReader::Read(config_json));
  if (!config)
    LOG(FATAL) << "Failed to parse " << config_path.value();

  std::string address_str;
  if (config->GetString("address", &address_str)) {
    if (!result.address_.AssignFromIPLiteral(address_str)) {
      LOG(FATAL) << "Invalid address specified in test server config: "
                 << address_str;
    }
  } else {
    LOG(WARNING) << "address isn't specified in test server config.";
  }

  if (config->GetString("spawner_url_base", &result.spawner_url_base_)) {
    GURL url(result.spawner_url_base_);
    if (!url.is_valid()) {
      LOG(FATAL) << "Invalid spawner_url_base specified in test server config: "
                 << result.spawner_url_base_;
    }
  }

  return result;
}

GURL RemoteTestServerConfig::GetSpawnerUrl(const std::string& command) const {
  CHECK(!spawner_url_base_.empty())
      << "spawner_url_base is expected, but not set in test server config.";
  std::string url = spawner_url_base_ + "/" + command;
  GURL result = GURL(url);
  CHECK(result.is_valid()) << url;
  return result;
}

}  // namespace net
