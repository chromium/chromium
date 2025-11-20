// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_list.h"

#include <stddef.h>

#include <algorithm>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/lazy_instance.h"
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
bool SupportsType(const WebPluginInfo& plugin, const std::string& mime_type) {
  // Webkit will ask for a plugin to handle empty mime types.
  if (mime_type.empty())
    return false;

  for (const WebPluginMimeType& mime_info : plugin.mime_types) {
    if (net::MatchesMimeType(mime_info.mime_type, mime_type)) {
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

void PluginList::RegisterInternalPlugin(const WebPluginInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  internal_plugins_.insert(internal_plugins_.begin(), info);
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
}

std::vector<WebPluginInfo> PluginList::GetInternalPluginsForTesting() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return internal_plugins_;
}

PluginList::PluginList() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PluginList::LoadPlugins() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<base::FilePath> seen_plugin_paths;
  plugins_list_.clear();
  for (const WebPluginInfo& plugin_info : internal_plugins_) {
    if (base::Contains(seen_plugin_paths, plugin_info.path)) {
      continue;
    }
    seen_plugin_paths.push_back(plugin_info.path);

    for (const content::WebPluginMimeType& mime_type : plugin_info.mime_types) {
      // These should only be set by internal extensions. Sanity check there are
      // no global handlers.
      CHECK_NE(mime_type.mime_type, "*");
    }
    plugins_list_.push_back(plugin_info);
  }
}

const std::vector<WebPluginInfo>& PluginList::GetPlugins() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LoadPlugins();
  return plugins_list_;
}

const std::vector<WebPluginInfo>& PluginList::GetPluginsForTesting() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return plugins_list_;
}

void PluginList::GetPluginInfoArray(
    const GURL& url,
    const std::string& mime_type,
    std::vector<WebPluginInfo>* info,
    std::vector<std::string>* actual_mime_types) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(mime_type == base::ToLowerASCII(mime_type));
  DCHECK(info);

  info->clear();
  if (actual_mime_types) {
    actual_mime_types->clear();
  }

  std::set<base::FilePath> visited_plugins;

  // Add in plugins by mime type.
  for (const WebPluginInfo& plugin : plugins_list_) {
    if (SupportsType(plugin, mime_type)) {
      const base::FilePath& path = plugin.path;
      if (visited_plugins.insert(path).second) {
        info->push_back(plugin);
        if (actual_mime_types) {
          actual_mime_types->push_back(mime_type);
        }
      }
    }
  }

  // Add in plugins by url.
  // We do not permit URL-sniff based plugin MIME type overrides aside from
  // the case where the "type" was initially missing.
  // We collected stats to determine this approach isn't a major compat issue,
  // and we defend against content confusion attacks in various cases, such
  // as when the user doesn't have the Flash plugin enabled.
  std::string path = url.GetPath();
  std::string::size_type last_dot = path.rfind('.');
  if (last_dot == std::string::npos || !mime_type.empty()) {
    return;
  }

  std::string extension =
      base::ToLowerASCII(std::string_view(path).substr(last_dot + 1));
  std::string actual_mime_type;
  for (const WebPluginInfo& plugin : plugins_list_) {
    if (SupportsExtension(plugin, extension, &actual_mime_type)) {
      base::FilePath plugin_path = plugin.path;
      if (visited_plugins.insert(plugin_path).second) {
        info->push_back(plugin);
        if (actual_mime_types) {
          actual_mime_types->push_back(actual_mime_type);
        }
      }
    }
  }
}

PluginList::~PluginList() = default;

}  // namespace content
