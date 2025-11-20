// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/buildflags.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

class GURL;

namespace base {
class FilePath;
}

namespace content {

class BrowserContext;
class PluginServiceFilter;
struct WebPluginInfo;

// This class lives on the UI thread.
class CONTENT_EXPORT PluginService {
 public:
  // Returns the PluginService singleton.
  static PluginService* GetInstance();

  // Tells all the renderer processes associated with the given browser context
  // to throw away their cache of the plugin list, and optionally also reload
  // all the pages with plugins. If `browser_context` is nullptr, purges the
  // cache in all renderers.
  static void PurgePluginListCache(BrowserContext* browser_context);

  virtual ~PluginService() {}

  // Must be called on the instance to finish initialization.
  virtual void Init() = 0;

  // Gets the plugin in the list of plugins that matches the given URL and mime
  // type.
  virtual void GetPluginInfoArray(
      const GURL& url,
      const std::string& mime_type,
      std::vector<WebPluginInfo>* info,
      std::vector<std::string>* actual_mime_types) = 0;

  // Filters the plugins list using `browser_context` and returns if there
  // exists a plugin that matches the given URL and mime type.
  virtual bool HasPlugin(content::BrowserContext* browser_context,
                         const GURL& url,
                         const std::string& mime_type) = 0;

  // Gets plugin info by plugin path (including disabled plugins). This will use
  // cached data in the plugin list.
  virtual std::optional<WebPluginInfo> GetPluginInfoByPathForTesting(
      const base::FilePath& plugin_path) = 0;

  // Synchronously loads plugins if necessary and returns the list of plugin
  // infos. This does not block and is safe to call on the UI thread.
  // Since this refreshes the list of plugins, callers can ignore the result if
  // they just want to refresh the list.
  virtual const std::vector<WebPluginInfo>& GetPlugins() = 0;

  virtual void SetFilter(PluginServiceFilter* filter) = 0;
  virtual PluginServiceFilter* GetFilter() = 0;

  // Register an internal plugin with the specified plugin information.
  // An internal plugin must be registered before it can
  // be loaded using PluginList::LoadPlugin().
  // New plugins get added earlier in the list so that they can override the
  // MIME types of older registrations.
  virtual void RegisterInternalPlugin(const WebPluginInfo& info) = 0;

  // Removes a specified internal plugin from the list. The search will match
  // on the path from the version info previously registered.
  virtual void UnregisterInternalPlugin(const base::FilePath& path) = 0;

  // Gets a list of all the registered internal plugins.
  virtual std::vector<WebPluginInfo> GetInternalPluginsForTesting() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_H_
