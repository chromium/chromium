// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_plugin_info.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/webplugininfo.h"
#include "ppapi/buildflags/buildflags.h"

namespace content {

#if BUILDFLAG(ENABLE_PPAPI)
ContentPluginInfo::EntryPoints::EntryPoints() = default;
#endif  // BUILDFLAG(ENABLE_PPAPI)

ContentPluginInfo::ContentPluginInfo() = default;
ContentPluginInfo::ContentPluginInfo(const ContentPluginInfo& other) = default;
ContentPluginInfo::ContentPluginInfo(ContentPluginInfo&& other) noexcept =
    default;
ContentPluginInfo::~ContentPluginInfo() = default;

WebPluginInfo ContentPluginInfo::ToWebPluginInfo() const {
  WebPluginInfo info;

  info.type = is_out_of_process
                  ? WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS
                  : WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;

  info.name = name.empty() ? path.BaseName().LossyDisplayName()
                           : base::UTF8ToUTF16(name);
  info.path = path;
  info.version = base::ASCIIToUTF16(version);
  info.desc = base::ASCIIToUTF16(description);
  info.mime_types = mime_types;
  info.pepper_permissions = permissions;

  return info;
}

}  // namespace content
