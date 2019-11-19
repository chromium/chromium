// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_FAKE_APPLICATION_CONFIG_MANAGER_H_
#define FUCHSIA_RUNNERS_CAST_FAKE_APPLICATION_CONFIG_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "url/gurl.h"

// Test cast.ApplicationConfigManager implementation which maps a test Cast
// AppId to a URL.
class FakeApplicationConfigManager
    : public chromium::cast::ApplicationConfigManager {
 public:
  FakeApplicationConfigManager();
  ~FakeApplicationConfigManager() override;

  // Associates a Cast application |id| with a url, to be served from the
  // EmbeddedTestServer.
  void AddAppMapping(const std::string& id,
                     const GURL& url,
                     bool enable_remote_debugging);

  // Associates a Cast application |id| with a url and a set of content
  // directories, to be served from the EmbeddedTestServer.
  void AddAppMappingWithContentDirectories(
      const std::string& id,
      const GURL& url,
      std::vector<fuchsia::web::ContentDirectoryProvider> directories);

  // chromium::cast::ApplicationConfigManager interface.
  void GetConfig(std::string id, GetConfigCallback config_callback) override;

 private:
  std::map<std::string, chromium::cast::ApplicationConfig> id_to_config_;

  DISALLOW_COPY_AND_ASSIGN(FakeApplicationConfigManager);
};

#endif  // FUCHSIA_RUNNERS_CAST_FAKE_APPLICATION_CONFIG_MANAGER_H_
