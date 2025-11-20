// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PLUGIN_LIST_H_
#define CONTENT_BROWSER_PLUGIN_LIST_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/buildflags.h"
#include "content/public/common/webplugininfo.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

class GURL;

namespace content {

// Manages the list of plugins. At this point, there are no external plugins.
// This object lives on the UI thread.
class CONTENT_EXPORT PluginList {
 public:
  // Gets the one instance of the PluginList.
  static PluginList* Singleton();

  PluginList(const PluginList&) = delete;
  PluginList& operator=(const PluginList&) = delete;

  // Register an internal plugin with the specified plugin information.
  // An internal plugin must be registered before it can
  // be loaded using PluginList::LoadPlugin().
  // New plugins get added earlier in the list so that they can override the
  // MIME types of older registrations.
  void RegisterInternalPlugin(const WebPluginInfo& info);

  // Removes a specified internal plugin from the list. The search will match
  // on the path from the version info previously registered.
  void UnregisterInternalPlugin(const base::FilePath& path);

  // Gets a list of all the registered internal plugins.
  std::vector<WebPluginInfo> GetInternalPluginsForTesting() const;

  // Get all the plugins synchronously, loading them if necessary.
  const std::vector<WebPluginInfo>& GetPlugins();

  // Returns the list of plugins without loading them.
  const std::vector<WebPluginInfo>& GetPluginsForTesting() const;

  // Returns a list in `info` containing plugins that are found for
  // the given url and mime type (including disabled plugins, for
  // which `info->enabled` is false).  The mime type which corresponds
  // to the URL is optionally returned back in `actual_mime_types` (if
  // it is non-NULL), one for each of the plugin info objects found.
  // The `info` parameter is required to be non-NULL.
  // The list is in order of "most desirable" to "least desirable".
  // This will load the plugin list if necessary.
  void GetPluginInfoArray(const GURL& url,
                          const std::string& mime_type,
                          std::vector<WebPluginInfo>* info,
                          std::vector<std::string>* actual_mime_types);

 private:
  friend class PluginListTest;
  friend struct base::LazyInstanceTraitsBase<PluginList>;

  PluginList();
  ~PluginList();

  // The following functions are used to support probing for WebPluginInfo
  // using a different instance of this class.

  // Load all plugins from the default plugins directory.
  void LoadPlugins();

  //
  // Internals
  //

  // Holds information about internal plugins.
  std::vector<WebPluginInfo> internal_plugins_;

  // A list holding all plugins.
  std::vector<WebPluginInfo> plugins_list_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PLUGIN_LIST_H_
