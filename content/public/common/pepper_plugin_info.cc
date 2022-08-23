// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/pepper_plugin_info.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/webplugininfo.h"
#include "ppapi/buildflags/buildflags.h"

namespace content {

#if BUILDFLAG(ENABLE_PPAPI)
PepperPluginInfo::EntryPoints::EntryPoints() = default;
#endif  // BUILDFLAG(ENABLE_PPAPI)

PepperPluginInfo::PepperPluginInfo() = default;
PepperPluginInfo::PepperPluginInfo(const PepperPluginInfo& other) = default;
PepperPluginInfo::PepperPluginInfo(PepperPluginInfo&& other) noexcept = default;
PepperPluginInfo::~PepperPluginInfo() = default;

WebPluginInfo PepperPluginInfo::ToWebPluginInfo() const {
  WebPluginInfo info;

  info.type = is_out_of_process ?
      WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS :
      WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;

  info.name = name.empty() ?
      path.BaseName().LossyDisplayName() : base::UTF8ToUTF16(name);
  info.path = path;
  info.version = base::ASCIIToUTF16(version);
  info.desc = base::ASCIIToUTF16(description);
  info.mime_types = mime_types;
  info.pepper_permissions = permissions;

  return info;
}

}  // namespace content
