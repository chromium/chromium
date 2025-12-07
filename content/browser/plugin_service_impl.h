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
struct WebPluginInfo;

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
  void GetPluginInfoArray(const GURL& url,
                          const std::string& mime_type,
                          std::vector<WebPluginInfo>* info,
                          std::vector<std::string>* actual_mime_types) override;
  bool HasPlugin(content::BrowserContext* browser_context,
                 const GURL& url,
                 const std::string& mime_type) override;
  std::optional<WebPluginInfo> GetPluginInfoByPathForTesting(
      const base::FilePath& plugin_path) override;
  const std::vector<WebPluginInfo>& GetPlugins() override;
  void SetFilter(PluginServiceFilter* filter) override;
  PluginServiceFilter* GetFilter() override;
  void RegisterInternalPlugin(const WebPluginInfo& info) override;
  void UnregisterInternalPlugin(const base::FilePath& path) override;
  std::vector<WebPluginInfo> GetInternalPluginsForTesting() override;

 private:
  friend struct base::DefaultSingletonTraits<PluginServiceImpl>;

  // Creates the PluginServiceImpl object, but doesn't actually build the plugin
  // list yet.  It's generated lazily.
  PluginServiceImpl();
  ~PluginServiceImpl() override;

  void RegisterPlugins();

  std::vector<WebPluginInfo> plugins_;

  // Weak pointer; set during the startup and must outlive us.
  raw_ptr<PluginServiceFilter, DanglingUntriaged> filter_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_
