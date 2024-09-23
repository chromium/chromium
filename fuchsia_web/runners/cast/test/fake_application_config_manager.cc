// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/test/fake_application_config_manager.h"

#include <fuchsia/web/cpp/fidl.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"

constexpr char FakeApplicationConfigManager::kFakeAgentUrl[] =
    "fuchsia-pkg://fuchsia.com/fake_agent#meta/fake_agent.cmx";

// static
chromium::cast::ApplicationConfig FakeApplicationConfigManager::CreateConfig(
    std::string_view id,
    const GURL& url) {
  chromium::cast::ApplicationConfig app_config;
  app_config.set_id(std::string(id));
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

void FakeApplicationConfigManager::AddApp(std::string_view id,
                                          const GURL& url) {
  AddAppConfig(CreateConfig(id, url));
}

void FakeApplicationConfigManager::GetConfig(std::string id,
                                             GetConfigCallback callback) {
  auto it = id_to_config_.find(id);
  if (it == id_to_config_.end()) {
    LOG(ERROR) << "Unknown Cast App ID: " << id;
    callback(chromium::cast::ApplicationConfig());
    return;
  }

  // ContextDirectoryProviders contain move-only fuchsia.io.Directory resources,
  // so if those are present then remove them, manually clone them, then
  // put them back.
  std::optional<std::vector<fuchsia::web::ContentDirectoryProvider>>
      content_directories;
  chromium::cast::ApplicationConfig& config = it->second;
  if (config.has_content_directories_for_isolated_application()) {
    content_directories.emplace(std::move(
        *config.mutable_content_directories_for_isolated_application()));
  }

  // Now it should be safe to Clone() the configuration.
  chromium::cast::ApplicationConfig result;
  zx_status_t status = config.Clone(&result);
  ZX_CHECK(status == ZX_OK, status) << "Clone result";

  // Manually clone across the content directories, if necessary.
  if (content_directories) {
    for (auto& content_directory : *content_directories) {
      fuchsia::web::ContentDirectoryProvider entry;
      entry.set_name(content_directory.name());

      // Borrow the directory handle to clone it.
      fuchsia::io::DirectorySyncPtr sync_ptr;
      sync_ptr.Bind(std::move(*content_directory.mutable_directory()));
      fidl::InterfaceHandle<fuchsia::io::Node> node;
      status = sync_ptr->Clone(fuchsia::io::OpenFlags::CLONE_SAME_RIGHTS,
                               node.NewRequest());
      ZX_CHECK(status == ZX_OK, status) << "Clone content directory";
      entry.set_directory(
          fidl::InterfaceHandle<fuchsia::io::Directory>(node.TakeChannel()));
      *content_directory.mutable_directory() = sync_ptr.Unbind();

      result.mutable_content_directories_for_isolated_application()->push_back(
          std::move(entry));
    }

    // Restore the content directories into the stored configuration.
    config.set_content_directories_for_isolated_application(
        std::move(*content_directories));
  }

  callback(std::move(result));
}
