// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class FilePath;
}

namespace url {
class Origin;
}

namespace content {

class BrowserContext;
class PluginServiceFilter;
struct PepperPluginInfo;
struct WebPluginInfo;

// This must be created on the main thread but it's only called on the IO/file
// thread unless otherwise noted. This is an asynchronous wrapper around the
// PluginList interface for querying plugin information. This must be used
// instead of that to avoid doing expensive disk operations on the IO/UI
// threads.
// TODO(http://crbug.com/990013): Only use this on the UI thread.
class CONTENT_EXPORT PluginService {
 public:
  using GetPluginsCallback =
      base::OnceCallback<void(const std::vector<WebPluginInfo>&)>;

  // Returns the PluginService singleton.
  static PluginService* GetInstance();

  // Tells all the renderer processes associated with the given browser context
  // to throw away their cache of the plugin list, and optionally also reload
  // all the pages with plugins. If |browser_context| is nullptr, purges the
  // cache in all renderers.
  // NOTE: can only be called on the UI thread.
  static void PurgePluginListCache(BrowserContext* browser_context,
                                   bool reload_pages);

  virtual ~PluginService() {}

  // Must be called on the instance to finish initialization.
  virtual void Init() = 0;

  // Gets the plugin in the list of plugins that matches the given url and mime
  // type. Returns true if the data is frome a stale plugin list, false if it
  // is up to date. This can be called from any thread.
  virtual bool GetPluginInfoArray(
      const GURL& url,
      const std::string& mime_type,
      bool allow_wildcard,
      std::vector<WebPluginInfo>* info,
      std::vector<std::string>* actual_mime_types) = 0;

  // Gets plugin info for an individual plugin and filters the plugins using
  // the |context| and renderer IDs. This will report whether the data is stale
  // via |is_stale| and returns whether or not the plugin can be found.
  // This must be called from the UI thread.
  virtual bool GetPluginInfo(int render_process_id,
                             int render_frame_id,
                             const GURL& url,
                             const url::Origin& main_frame_origin,
                             const std::string& mime_type,
                             bool allow_wildcard,
                             bool* is_stale,
                             WebPluginInfo* info,
                             std::string* actual_mime_type) = 0;

  // Get plugin info by plugin path (including disabled plugins). Returns true
  // if the plugin is found and WebPluginInfo has been filled in |info|. This
  // will use cached data in the plugin list.
  // This can be called from any thread.
  virtual bool GetPluginInfoByPath(const base::FilePath& plugin_path,
                                   WebPluginInfo* info) = 0;

  // Returns the display name for the plugin identified by the given path. If
  // the path doesn't identify a plugin, or the plugin has no display name,
  // this will attempt to generate a display name from the path.
  // This can be called from any thread.
  virtual base::string16 GetPluginDisplayNameByPath(
      const base::FilePath& plugin_path) = 0;

  // Asynchronously loads plugins if necessary and then calls back to the
  // provided function on the calling sequence on completion.
  // This can be called from any thread.
  virtual void GetPlugins(GetPluginsCallback callback) = 0;

  // Returns information about a pepper plugin if it exists, otherwise nullptr.
  // The caller does not own the pointer, and it's not guaranteed to live past
  // the call stack.
  virtual const PepperPluginInfo* GetRegisteredPpapiPluginInfo(
      const base::FilePath& plugin_path) = 0;

  virtual void SetFilter(PluginServiceFilter* filter) = 0;
  virtual PluginServiceFilter* GetFilter() = 0;

  // Used to monitor plugin stability. An unstable plugin is one that has
  // crashed more than a set number of times in a set time period.
  virtual bool IsPluginUnstable(const base::FilePath& plugin_path) = 0;

  // Cause the plugin list to refresh next time they are accessed, regardless
  // of whether they are already loaded.
  virtual void RefreshPlugins() = 0;

  // Register an internal plugin with the specified plugin information.
  // An internal plugin must be registered before it can
  // be loaded using PluginList::LoadPlugin().
  // If |add_at_beginning| is true the plugin will be added earlier in
  // the list so that it can override the MIME types of older registrations.
  virtual void RegisterInternalPlugin(const WebPluginInfo& info,
                                      bool add_at_beginning) = 0;

  // Removes a specified internal plugin from the list. The search will match
  // on the path from the version info previously registered.
  virtual void UnregisterInternalPlugin(const base::FilePath& path) = 0;

  // Gets a list of all the registered internal plugins.
  virtual void GetInternalPlugins(std::vector<WebPluginInfo>* plugins) = 0;

  // Returns true iff PPAPI "dev channel" methods are supported.
  virtual bool PpapiDevChannelSupported(BrowserContext* browser_context,
                                        const GURL& document_url) = 0;

  // Determine the number of PPAPI processes currently tracked by the service.
  // Exposed primarily for testing purposes.
  virtual int CountPpapiPluginProcessesForProfile(
      const base::FilePath& plugin_path,
      const base::FilePath& profile_data_directory) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_H_
