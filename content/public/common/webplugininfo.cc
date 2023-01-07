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

WebPluginInfo::WebPluginInfo()
    : type(PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS),
      pepper_permissions(0) {
}

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
      mime_types(),
      type(PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS),
      pepper_permissions(0) {}

void WebPluginInfo::CreateVersionFromString(
    const std::u16string& version_string,
    base::Version* parsed_version) {
  // Remove spaces and ')' from the version string,
  // Replace any instances of 'r', ',' or '(' with a dot.
  std::string version = base::UTF16ToASCII(version_string);
  base::RemoveChars(version, ") ", &version);
  std::replace(version.begin(), version.end(), 'd', '.');
  std::replace(version.begin(), version.end(), 'r', '.');
  std::replace(version.begin(), version.end(), ',', '.');
  std::replace(version.begin(), version.end(), '(', '.');
  std::replace(version.begin(), version.end(), '_', '.');

  // Remove leading zeros from each of the version components.
  std::string no_leading_zeros_version;
  std::vector<std::string> numbers = base::SplitString(
      version, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t i = 0; i < numbers.size(); ++i) {
    size_t n = numbers[i].size();
    size_t j = 0;
    while (j < n && numbers[i][j] == '0') {
      ++j;
    }
    no_leading_zeros_version += (j < n) ? numbers[i].substr(j) : "0";
    if (i != numbers.size() - 1) {
      no_leading_zeros_version += ".";
    }
  }

  *parsed_version = base::Version(no_leading_zeros_version);
}

}  // namespace content
