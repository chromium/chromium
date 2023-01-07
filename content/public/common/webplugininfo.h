// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_WEBPLUGININFO_H_
#define CONTENT_PUBLIC_COMMON_WEBPLUGININFO_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace base {
class Version;
}

namespace content {

struct CONTENT_EXPORT WebPluginMimeType {
  WebPluginMimeType();
  // A constructor for the common case of a single file extension and an ASCII
  // description.
  WebPluginMimeType(const std::string& m,
                    const std::string& f,
                    const std::string& d);
  WebPluginMimeType(const WebPluginMimeType& other);
  ~WebPluginMimeType();

  // The name of the mime type (e.g., "application/x-shockwave-flash").
  std::string mime_type;

  // A list of all the file extensions for this mime type.
  std::vector<std::string> file_extensions;

  // Description of the mime type.
  std::u16string description;

  // Extra parameters to include when instantiating the plugin.
  struct Param {
    Param() = default;
    Param(std::u16string n, std::u16string v)
        : name(std::move(n)), value(std::move(v)) {}
    std::u16string name;
    std::u16string value;
  };
  std::vector<Param> additional_params;
};

// Describes an available Pepper plugin.
struct CONTENT_EXPORT WebPluginInfo {
  enum PluginType {
    PLUGIN_TYPE_PEPPER_IN_PROCESS,
    PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS,
    PLUGIN_TYPE_BROWSER_PLUGIN
  };

  static constexpr SkColor kDefaultBackgroundColor = SkColorSetRGB(38, 38, 38);

  WebPluginInfo();
  WebPluginInfo(const WebPluginInfo& rhs);
  ~WebPluginInfo();
  WebPluginInfo& operator=(const WebPluginInfo& rhs);

  // Special constructor only used during unit testing:
  WebPluginInfo(const std::u16string& fake_name,
                const base::FilePath& fake_path,
                const std::u16string& fake_version,
                const std::u16string& fake_desc);

  bool is_pepper_plugin() const {
    return ((type == PLUGIN_TYPE_PEPPER_IN_PROCESS ) ||
          (type == PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS));
  }

  // Parse a version string as used by a plugin. This method is more lenient
  // in accepting weird version strings than base::Version::GetFromString()
  static void CreateVersionFromString(const std::u16string& version_string,
                                      base::Version* parsed_version);

  // The name of the plugin (i.e. Flash).
  std::u16string name;

  // The path to the plugin file (DLL/bundle/library).
  base::FilePath path;

  // The version number of the plugin file (may be OS-specific)
  std::u16string version;

  // A description of the plugin that we get from its version info.
  std::u16string desc;

  // A list of all the mime types that this plugin supports.
  std::vector<WebPluginMimeType> mime_types;

  // Plugin type. See the PluginType enum.
  int type;

  // When type is PLUGIN_TYPE_PEPPER_* this indicates the permission bits.
  int32_t pepper_permissions;

  // The color to use as the background before the plugin loads.
  SkColor background_color = kDefaultBackgroundColor;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_WEBPLUGININFO_H_
