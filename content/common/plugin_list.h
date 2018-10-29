// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PLUGIN_LIST_H_
#define CONTENT_COMMON_PLUGIN_LIST_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/webplugininfo.h"
#include "ppapi/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

class GURL;

namespace content {

// The PluginList is responsible for loading our NPAPI based plugins. It does
// so in whatever manner is appropriate for the platform. On Windows, it loads
// plugins from a known directory by looking for DLLs which start with "NP",
// and checking to see if they are valid NPAPI libraries. On the Mac, it walks
// the machine-wide and user plugin directories and loads anything that has
// the correct types. On Linux, it walks the plugin directories as well
// (e.g. /usr/lib/browser-plugins/).
// This object is thread safe.
class CONTENT_EXPORT PluginList {
 public:
  // Gets the one instance of the PluginList.
  static PluginList* Singleton();

  // Cause the plugin list to refresh next time they are accessed, regardless
  // of whether they are already loaded.
  void RefreshPlugins();

  // Register an internal plugin with the specified plugin information.
  // An internal plugin must be registered before it can
  // be loaded using PluginList::LoadPlugin().
  // If |add_at_beginning| is true the plugin will be added earlier in
  // the list so that it can override the MIME types of older registrations.
  void RegisterInternalPlugin(const WebPluginInfo& info,
                              bool add_at_beginning);

  // Removes a specified internal plugin from the list. The search will match
  // on the path from the version info previously registered.
  void UnregisterInternalPlugin(const base::FilePath& path);

  // Gets a list of all the registered internal plugins.
  void GetInternalPlugins(std::vector<WebPluginInfo>* plugins);

  // Get all the plugins synchronously, loading them if necessary.
  void GetPlugins(std::vector<WebPluginInfo>* plugins);

  // Copies the list of plugins into |plugins| without loading them.
  // Returns true if the list of plugins is up to date.
  bool GetPluginsNoRefresh(std::vector<WebPluginInfo>* plugins);

  // Returns a list in |info| containing plugins that are found for
  // the given url and mime type (including disabled plugins, for
  // which |info->enabled| is false).  The mime type which corresponds
  // to the URL is optionally returned back in |actual_mime_types| (if
  // it is non-NULL), one for each of the plugin info objects found.
  // The |allow_wildcard| parameter controls whether this function
  // returns plugins which support wildcard mime types (* as the mime
  // type).  The |info| parameter is required to be non-NULL.  The
  // list is in order of "most desirable" to "least desirable".
  // If |use_stale| is NULL, this will load the plugin list if necessary.
  // If it is not NULL, the plugin list will not be loaded, and |*use_stale|
  // will be true iff the plugin list was stale.
  void GetPluginInfoArray(const GURL& url,
                          const std::string& mime_type,
                          bool allow_wildcard,
                          bool* use_stale,
                          std::vector<WebPluginInfo>* info,
                          std::vector<std::string>* actual_mime_types);

  void set_will_load_plugins_callback(const base::Closure& callback);

 private:
  enum LoadingState {
    LOADING_STATE_NEEDS_REFRESH,
    LOADING_STATE_REFRESHING,
    LOADING_STATE_UP_TO_DATE,
  };

  friend class PluginListTest;
  friend struct base::LazyInstanceTraitsBase<PluginList>;

  PluginList();
  ~PluginList();

  // The following functions are used to support probing for WebPluginInfo
  // using a different instance of this class.

  // Computes a list of all plugins to potentially load from all sources.
  void GetPluginPathsToLoad(std::vector<base::FilePath>* plugin_paths);

  // Signals that plugin loading will start. This method should be called before
  // loading plugins with a different instance of this class. Returns false if
  // the plugin list is up to date.
  // When loading has finished, SetPlugins() should be called with the list of
  // plugins.
  bool PrepareForPluginLoading();

  // Clears the internal list of Plugins and copies them from the vector.
  void SetPlugins(const std::vector<WebPluginInfo>& plugins);

  // Load all plugins from the default plugins directory.
  void LoadPlugins();

  // Removes |plugin_path| from the list of extra plugin paths. Should only be
  // called while holding |lock_|.
  void RemoveExtraPluginPathLocked(const base::FilePath& plugin_path)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Creates a WebPluginInfo structure given a plugin's path.  On success
  // returns true, with the information being put into "info".
  // Returns false if the library couldn't be found, or if it's not a plugin.
  bool ReadPluginInfo(const base::FilePath& filename, WebPluginInfo* info);

  // Load a specific plugin with full path. Return true iff loading the plugin
  // was successful.
  bool LoadPluginIntoPluginList(const base::FilePath& filename,
                                std::vector<WebPluginInfo>* plugins,
                                WebPluginInfo* plugin_info);

  //
  // Internals
  //

  // States whether we will load the plugin list the next time we try to access
  // it, whether we are currently in the process of loading it, or whether we
  // consider it up to date.
  LoadingState loading_state_ GUARDED_BY(lock_);

  // Extra plugin paths that we want to search when loading.
  std::vector<base::FilePath> extra_plugin_paths_ GUARDED_BY(lock_);

  // Holds information about internal plugins.
  std::vector<WebPluginInfo> internal_plugins_ GUARDED_BY(lock_);

  // A list holding all plugins.
  std::vector<WebPluginInfo> plugins_list_ GUARDED_BY(lock_);

  // Callback that is invoked whenever the PluginList will reload the plugins.
  base::Closure will_load_plugins_callback_ GUARDED_BY(lock_);

  // Need synchronization for the above members since this object can be
  // accessed on multiple threads.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PluginList);
};

}  // namespace content

#endif  // CONTENT_COMMON_PLUGIN_LIST_H_
