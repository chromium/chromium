// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/fake_application_config_manager.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "fuchsia/runners/cast/cast_component.h"

constexpr char FakeApplicationConfigManager::kFakeAgentUrl[] =
    "fuchsia-pkg://fuchsia.com/fake_agent#meta/fake_agent.cmx";

// static
chromium::cast::ApplicationConfig FakeApplicationConfigManager::CreateConfig(
    const std::string& id,
    const GURL& url) {
  chromium::cast::ApplicationConfig app_config;
  app_config.set_id(id);
  app_config.set_display_name("Dummy test app");
  app_config.set_web_url(url.spec());
  app_config.set_agent_url(kFakeAgentUrl);

  // Add a PROTECTED_MEDIA_IDENTIFIER permission. This is consistent with the
  // real ApplicationConfigManager.
  fuchsia::web::PermissionDescriptor permission;
  permission.set_type(fuchsia::web::PermissionType::PROTECTED_MEDIA_IDENTIFIER);
  app_config.mutable_permissions()->push_back(std::move(permission));

  return app_config;
}

FakeApplicationConfigManager::FakeApplicationConfigManager() = default;
FakeApplicationConfigManager::~FakeApplicationConfigManager() = default;

void FakeApplicationConfigManager::AddAppConfig(
    chromium::cast::ApplicationConfig app_config) {
  id_to_config_[app_config.id()] = std::move(app_config);
}

void FakeApplicationConfigManager::AddApp(const std::string& id,
                                          const GURL& url) {
  AddAppConfig(CreateConfig(id, url));
}

void FakeApplicationConfigManager::GetConfig(std::string id,
                                             GetConfigCallback callback) {
  if (id_to_config_.find(id) == id_to_config_.end()) {
    LOG(ERROR) << "Unknown Cast App ID: " << id;
    callback(chromium::cast::ApplicationConfig());
    return;
  }

  callback(std::move(id_to_config_[id]));
  id_to_config_.erase(id);
}
