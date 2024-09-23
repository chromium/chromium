// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/pref_names.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "extensions/common/api/types.h"

namespace extensions {
namespace pref_names {

using extensions::api::types::ChromeSettingScope;

bool ScopeToPrefName(ChromeSettingScope scope, std::string* result) {
  switch (scope) {
    case ChromeSettingScope::kRegular:
      *result = kPrefPreferences;
      return true;
    case ChromeSettingScope::kRegularOnly:
      *result = kPrefRegularOnlyPreferences;
      return true;
    case ChromeSettingScope::kIncognitoPersistent:
      *result = kPrefIncognitoPreferences;
      return true;
    case ChromeSettingScope::kIncognitoSessionOnly:
      return false;
    case ChromeSettingScope::kNone:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

const char kPrefPreferences[] = "preferences";
const char kPrefIncognitoPreferences[] = "incognito_preferences";
const char kPrefRegularOnlyPreferences[] = "regular_only_preferences";
const char kPrefContentSettings[] = "content_settings";
const char kPrefIncognitoContentSettings[] = "incognito_content_settings";

}  // namespace pref_names
}  // namespace extensions
