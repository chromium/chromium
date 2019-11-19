// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/fake_application_config_manager.h"

#include <string>
#include <utility>

#include "base/logging.h"

FakeApplicationConfigManager::FakeApplicationConfigManager() = default;

FakeApplicationConfigManager::~FakeApplicationConfigManager() = default;

void FakeApplicationConfigManager::GetConfig(std::string id,
                                             GetConfigCallback callback) {
  if (id_to_config_.find(id) == id_to_config_.end()) {
    LOG(ERROR) << "Unknown Cast App ID: " << id;
    callback(chromium::cast::ApplicationConfig());
    return;
  }

  callback(std::move(std::move(id_to_config_[id])));
  id_to_config_.erase(id);
}

void FakeApplicationConfigManager::AddAppMapping(const std::string& id,
                                                 const GURL& url,
                                                 bool enable_remote_debugging) {
  chromium::cast::ApplicationConfig app_config;
  app_config.set_id(id);
  app_config.set_display_name("Dummy test app");
  app_config.set_web_url(url.spec());
  app_config.set_enable_remote_debugging(enable_remote_debugging);
  id_to_config_[id] = std::move(app_config);
}

void FakeApplicationConfigManager::AddAppMappingWithContentDirectories(
    const std::string& id,
    const GURL& url,
    std::vector<fuchsia::web::ContentDirectoryProvider> directories) {
  chromium::cast::ApplicationConfig app_config;
  app_config.set_id(id);
  app_config.set_display_name("Dummy test app");
  app_config.set_web_url(url.spec());
  if (!directories.empty()) {
    app_config.set_content_directories_for_isolated_application(
        std::move(directories));
  }

  id_to_config_[id] = std::move(app_config);
}
