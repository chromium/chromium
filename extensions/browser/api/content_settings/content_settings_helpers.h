// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_HELPERS_H_
#define EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_HELPERS_H_

#include <string>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace extensions {
namespace content_settings_helpers {

// Parses an extension match pattern and returns a corresponding
// content settings pattern object.
// If |pattern_str| is invalid or can't be converted to a content settings
// pattern, |error| is set to the parsing error and an invalid pattern
// is returned.
ContentSettingsPattern ParseExtensionPattern(const std::string& pattern_str,
                                             std::string* error);

// Converts a content settings type string to the corresponding
// ContentSettingsType. Returns ContentSettingsType::DEFAULT if the string
// didn't specify a valid content settings type.
ContentSettingsType StringToContentSettingsType(
    const std::string& content_type);
// Returns a string representation of a ContentSettingsType.
std::string ContentSettingsTypeToString(ContentSettingsType type);

}  // namespace content_settings_helpers
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_HELPERS_H_
