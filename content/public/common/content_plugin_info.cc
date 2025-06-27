// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_plugin_info.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/webplugininfo.h"

namespace content {

ContentPluginInfo::ContentPluginInfo() = default;
ContentPluginInfo::ContentPluginInfo(const ContentPluginInfo& other) = default;
ContentPluginInfo::ContentPluginInfo(ContentPluginInfo&& other) noexcept =
    default;
ContentPluginInfo::~ContentPluginInfo() = default;

WebPluginInfo ContentPluginInfo::ToWebPluginInfo() const {
  WebPluginInfo info;

  info.type = is_internal ? WebPluginInfo::PLUGIN_TYPE_BROWSER_INTERNAL_PLUGIN
                          : WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN;

  info.name = name.empty() ? path.BaseName().LossyDisplayName()
                           : base::UTF8ToUTF16(name);
  info.path = path;
  info.version = base::ASCIIToUTF16(version);
  info.desc = base::ASCIIToUTF16(description);
  info.mime_types = mime_types;

  return info;
}

}  // namespace content
