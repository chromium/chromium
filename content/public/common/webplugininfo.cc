// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/webplugininfo.h"

#include <stddef.h>

#include <algorithm>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"

namespace content {

WebPluginMimeType::WebPluginMimeType() {}

WebPluginMimeType::WebPluginMimeType(const std::string& m,
                                     const std::string& f,
                                     const std::string& d)
    : mime_type(m),
      file_extensions(),
      description(base::ASCIIToUTF16(d)) {
  file_extensions.push_back(f);
}

WebPluginMimeType::WebPluginMimeType(const WebPluginMimeType& other) = default;

WebPluginMimeType::~WebPluginMimeType() {}

WebPluginInfo::WebPluginInfo() = default;

WebPluginInfo::WebPluginInfo(const WebPluginInfo& rhs) = default;

WebPluginInfo::~WebPluginInfo() {}

WebPluginInfo& WebPluginInfo::operator=(const WebPluginInfo& rhs) = default;

WebPluginInfo::WebPluginInfo(const std::u16string& fake_name,
                             const base::FilePath& fake_path,
                             const std::u16string& fake_version,
                             const std::u16string& fake_desc)
    : name(fake_name),
      path(fake_path),
      version(fake_version),
      desc(fake_desc),
      mime_types() {}

}  // namespace content
