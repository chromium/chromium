// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_list.h"

#include <stddef.h>

#include <string_view>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "net/base/mime_util.h"
#include "url/gurl.h"

namespace content {

namespace {

base::LazyInstance<PluginList>::DestructorAtExit g_singleton =
    LAZY_INSTANCE_INITIALIZER;

// Returns true if the plugin supports |mime_type|. |mime_type| should be all
// lower case.
bool SupportsType(const WebPluginInfo& plugin,
                  const std::string& mime_type,
                  bool allow_wildcard) {
  // Webkit will ask for a plugin to handle empty mime types.
  if (mime_type.empty())
    return false;

  for (const WebPluginMimeType& mime_info : plugin.mime_types) {
    if (net::MatchesMimeType(mime_info.mime_type, mime_type)) {
      if (allow_wildcard || mime_info.mime_type != "*")
        return true;
    }
  }
  return false;
}

// Returns true if the given plugin supports a given file extension.
// |extension| should be all lower case. |actual_mime_type| will be set to the
// MIME type if found. The MIME type which corresponds to the extension is
// optionally returned back.
bool SupportsExtension(const WebPluginInfo& plugin,
                       const std::string& extension,
                       std::string* actual_mime_type) {
  for (const WebPluginMimeType& mime_type : plugin.mime_types) {
    for (const std::string& file_extension : mime_type.file_extensions) {
      if (file_extension == extension) {
        *actual_mime_type = mime_type.mime_type;
        return true;
      }
    }
  }
  return false;
}

}  // namespace

// static
PluginList* PluginList::Singleton() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_singleton.Pointer();
}

void PluginList::RefreshPlugins() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  loading_state_ = LOADING_STATE_NEEDS_REFRESH;
}

void PluginList::RegisterInternalPlugin(const WebPluginInfo& info,
                                        bool add_at_beginning) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  internal_plugins_.push_back(info);
  if (add_at_beginning) {
    // Newer registrations go earlier in the list so they can override the MIME
    // types of older registrations.
    extra_plugin_paths_.insert(extra_plugin_paths_.begin(), info.path);
  } else {
    extra_plugin_paths_.push_back(info.path);
  }
}

void PluginList::UnregisterInternalPlugin(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool found = false;
  for (size_t i = 0; i < internal_plugins_.size(); i++) {
    if (internal_plugins_[i].path == path) {
      internal_plugins_.erase(internal_plugins_.begin() + i);
      found = true;
      break;
    }
  }
  DCHECK(found);
  RemoveExtraPluginPath(path);
}

void PluginList::GetInternalPlugins(
    std::vector<WebPluginInfo>* internal_plugins) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& plugin : internal_plugins_)
    internal_plugins->push_back(plugin);
}

bool PluginList::ReadPluginInfo(const base::FilePath& filename,
                                WebPluginInfo* info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& plugin : internal_plugins_) {
    if (filename == plugin.path) {
      *info = plugin;
      return true;
    }
  }
  return false;
}

PluginList::PluginList() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

bool PluginList::PrepareForPluginLoading() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (loading_state_ == LOADING_STATE_UP_TO_DATE)
    return false;

  loading_state_ = LOADING_STATE_REFRESHING;
  return true;
}

void PluginList::LoadPlugins() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!PrepareForPluginLoading())
    return;

  std::vector<WebPluginInfo> new_plugins;
  std::vector<base::FilePath> plugin_paths;
  GetPluginPathsToLoad(&plugin_paths);

  for (const base::FilePath& path : plugin_paths) {
    WebPluginInfo plugin_info;
    LoadPluginIntoPluginList(path, &new_plugins, &plugin_info);
  }

  SetPlugins(new_plugins);
}

bool PluginList::LoadPluginIntoPluginList(const base::FilePath& path,
                                          std::vector<WebPluginInfo>* plugins,
                                          WebPluginInfo* plugin_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ReadPluginInfo(path, plugin_info))
    return false;

  // TODO(piman): Do we still need this after NPAPI removal?
  for (const content::WebPluginMimeType& mime_type : plugin_info->mime_types) {
    // TODO: don't load global handlers for now.
    // WebKit hands to the Plugin before it tries
    // to handle mimeTypes on its own.
    if (mime_type.mime_type == "*")
      return false;
  }
  plugins->push_back(*plugin_info);
  return true;
}

void PluginList::GetPluginPathsToLoad(
    std::vector<base::FilePath>* plugin_paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const base::FilePath& path : extra_plugin_paths_) {
    if (base::Contains(*plugin_paths, path))
      continue;
    plugin_paths->push_back(path);
  }
}

void PluginList::SetPlugins(const std::vector<WebPluginInfo>& plugins) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If we haven't been invalidated in the mean time, mark the plugin list as
  // up to date.
  if (loading_state_ != LOADING_STATE_NEEDS_REFRESH)
    loading_state_ = LOADING_STATE_UP_TO_DATE;

  plugins_list_ = plugins;
}

void PluginList::GetPlugins(std::vector<WebPluginInfo>* plugins) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LoadPlugins();
  plugins->insert(plugins->end(), plugins_list_.begin(), plugins_list_.end());
}

bool PluginList::GetPluginsNoRefresh(std::vector<WebPluginInfo>* plugins) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  plugins->insert(plugins->end(), plugins_list_.begin(), plugins_list_.end());

  return loading_state_ == LOADING_STATE_UP_TO_DATE;
}

bool PluginList::GetPluginInfoArray(
    const GURL& url,
    const std::string& mime_type,
    bool allow_wildcard,
    std::vector<WebPluginInfo>* info,
    std::vector<std::string>* actual_mime_types) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(mime_type == base::ToLowerASCII(mime_type));
  DCHECK(info);

  bool is_stale = loading_state_ != LOADING_STATE_UP_TO_DATE;
  info->clear();
  if (actual_mime_types)
    actual_mime_types->clear();

  std::set<base::FilePath> visited_plugins;

  // Add in plugins by mime type.
  for (const WebPluginInfo& plugin : plugins_list_) {
    if (SupportsType(plugin, mime_type, allow_wildcard)) {
      const base::FilePath& path = plugin.path;
      if (visited_plugins.insert(path).second) {
        info->push_back(plugin);
        if (actual_mime_types)
          actual_mime_types->push_back(mime_type);
      }
    }
  }

  // Add in plugins by url.
  // We do not permit URL-sniff based plugin MIME type overrides aside from
  // the case where the "type" was initially missing.
  // We collected stats to determine this approach isn't a major compat issue,
  // and we defend against content confusion attacks in various cases, such
  // as when the user doesn't have the Flash plugin enabled.
  std::string path = url.path();
  std::string::size_type last_dot = path.rfind('.');
  if (last_dot == std::string::npos || !mime_type.empty())
    return is_stale;

  std::string extension =
      base::ToLowerASCII(std::string_view(path).substr(last_dot + 1));
  std::string actual_mime_type;
  for (const WebPluginInfo& plugin : plugins_list_) {
    if (SupportsExtension(plugin, extension, &actual_mime_type)) {
      base::FilePath plugin_path = plugin.path;
      if (visited_plugins.insert(plugin_path).second) {
        info->push_back(plugin);
        if (actual_mime_types)
          actual_mime_types->push_back(actual_mime_type);
      }
    }
  }
  return is_stale;
}

void PluginList::RemoveExtraPluginPath(const base::FilePath& plugin_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<base::FilePath>::iterator it =
      base::ranges::find(extra_plugin_paths_, plugin_path);
  if (it != extra_plugin_paths_.end())
    extra_plugin_paths_.erase(it);
}

PluginList::~PluginList() = default;

}  // namespace content
