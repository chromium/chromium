// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_FILTER_H_
#define CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_FILTER_H_

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
struct WebPluginInfo;

// Callback class to let the client filter the list of all installed plugins
// and block them from being loaded.
// This class is called on the UI thread.
class PluginServiceFilter {
 public:
  virtual ~PluginServiceFilter() = default;

  // Whether `plugin` is available. The client can return false to hide the
  // plugin. The result may be cached, and should be consistent between calls.
  virtual bool IsPluginAvailable(content::BrowserContext* browser_context,
                                 const WebPluginInfo& plugin) = 0;

  // Whether the renderer has permission to load available `plugin`.
  virtual bool CanLoadPlugin(int render_process_id,
                             const base::FilePath& path) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_FILTER_H_
