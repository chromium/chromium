// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_plugin_service_filter.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/common/webplugininfo.h"

namespace content {

ShellPluginServiceFilter::ShellPluginServiceFilter() {}

ShellPluginServiceFilter::~ShellPluginServiceFilter() {}

bool ShellPluginServiceFilter::IsPluginAvailable(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    const url::Origin& main_frame_origin,
    WebPluginInfo* plugin) {
  return plugin->name == base::ASCIIToUTF16("Blink Test Plugin") ||
         plugin->name == base::ASCIIToUTF16("Blink Deprecated Test Plugin") ||
         plugin->name == base::ASCIIToUTF16("WebKit Test PlugIn");
}

bool ShellPluginServiceFilter::CanLoadPlugin(int render_process_id,
                                             const base::FilePath& path) {
  return true;
}

}  // namespace content
