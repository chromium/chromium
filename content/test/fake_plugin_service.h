// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_PLUGIN_SERVICE_H_
#define CONTENT_TEST_FAKE_PLUGIN_SERVICE_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/browser/plugin_service.h"

namespace url {
class Origin;
}

namespace content {

class FakePluginService : public PluginService {
 public:
  FakePluginService();
  ~FakePluginService() override;
  // PluginService implementation:
  void Init() override;
  bool GetPluginInfoArray(const GURL& url,
                          const std::string& mime_type,
                          bool allow_wildcard,
                          std::vector<WebPluginInfo>* info,
                          std::vector<std::string>* actual_mime_types) override;
  bool GetPluginInfo(int render_process_id,
                     int render_frame_id,
                     const GURL& url,
                     const url::Origin& main_frame_origin,
                     const std::string& mime_type,
                     bool allow_wildcard,
                     bool* is_stale,
                     WebPluginInfo* info,
                     std::string* actual_mime_type) override;
  bool GetPluginInfoByPath(const base::FilePath& plugin_path,
                           WebPluginInfo* info) override;
  base::string16 GetPluginDisplayNameByPath(
      const base::FilePath& path) override;
  void GetPlugins(GetPluginsCallback callback) override;
  const PepperPluginInfo* GetRegisteredPpapiPluginInfo(
      const base::FilePath& plugin_path) override;
  void SetFilter(PluginServiceFilter* filter) override;
  PluginServiceFilter* GetFilter() override;
  bool IsPluginUnstable(const base::FilePath& plugin_path) override;
  void RefreshPlugins() override;
  void RegisterInternalPlugin(const WebPluginInfo& info,
                              bool add_at_beginning) override;
  void UnregisterInternalPlugin(const base::FilePath& path) override;
  void GetInternalPlugins(std::vector<WebPluginInfo>* plugins) override;
  bool PpapiDevChannelSupported(BrowserContext* browser_context,
                                const GURL& document_url) override;
  int CountPpapiPluginProcessesForProfile(
      const base::FilePath& plugin_path,
      const base::FilePath& profile_data_directory) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePluginService);
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_PLUGIN_SERVICE_H_
