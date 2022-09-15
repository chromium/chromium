// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_plugin_service_filter.h"

#include "content/public/common/webplugininfo.h"

namespace content {

ShellPluginServiceFilter::ShellPluginServiceFilter() = default;

ShellPluginServiceFilter::~ShellPluginServiceFilter() = default;

bool ShellPluginServiceFilter::IsPluginAvailable(
    content::BrowserContext* browser_context,
    const WebPluginInfo& plugin) {
  return plugin.name == u"Blink Test Plugin" ||
         plugin.name == u"Blink Deprecated Test Plugin" ||
         plugin.name == u"WebKit Test PlugIn";
}

bool ShellPluginServiceFilter::CanLoadPlugin(int render_process_id,
                                             const base::FilePath& path) {
  return true;
}

}  // namespace content
