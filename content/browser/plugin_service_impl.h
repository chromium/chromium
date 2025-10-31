// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/plugin_service.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

namespace content {
class PluginServiceFilter;
struct ContentPluginInfo;

// This class responds to requests from renderers for the list of plugins, and
// also a proxy object for plugin instances. It lives on the UI thread.
class CONTENT_EXPORT PluginServiceImpl : public PluginService {
 public:
  // Returns the PluginServiceImpl singleton.
  static PluginServiceImpl* GetInstance();

  PluginServiceImpl(const PluginServiceImpl&) = delete;
  PluginServiceImpl& operator=(const PluginServiceImpl&) = delete;

  // PluginService implementation:
  void Init() override;
  bool GetPluginInfoArray(const GURL& url,
                          const std::string& mime_type,
                          bool allow_wildcard,
                          std::vector<WebPluginInfo>* info,
                          std::vector<std::string>* actual_mime_types) override;
  bool GetPluginInfo(content::BrowserContext* browser_context,
                     const GURL& url,
                     const std::string& mime_type,
                     bool allow_wildcard,
                     bool* is_stale,
                     WebPluginInfo* info,
                     std::string* actual_mime_type) override;
  bool GetPluginInfoByPath(const base::FilePath& plugin_path,
                           WebPluginInfo* info) override;
  std::u16string GetPluginDisplayNameByPath(
      const base::FilePath& path) override;
  void GetPlugins(GetPluginsCallback callback) override;
  std::vector<WebPluginInfo> GetPluginsSynchronous() override;
  void SetFilter(PluginServiceFilter* filter) override;
  PluginServiceFilter* GetFilter() override;
  void RefreshPlugins() override;
  void RegisterInternalPlugin(const WebPluginInfo& info,
                              bool add_at_beginning) override;
  void UnregisterInternalPlugin(const base::FilePath& path) override;
  std::vector<WebPluginInfo> GetInternalPluginsForTesting() override;

 private:
  friend struct base::DefaultSingletonTraits<PluginServiceImpl>;

  // Creates the PluginServiceImpl object, but doesn't actually build the plugin
  // list yet.  It's generated lazily.
  PluginServiceImpl();
  ~PluginServiceImpl() override;

  void RegisterPlugins();

  std::vector<ContentPluginInfo> plugins_;

  // Weak pointer; set during the startup and must outlive us.
  raw_ptr<PluginServiceFilter, DanglingUntriaged> filter_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_
