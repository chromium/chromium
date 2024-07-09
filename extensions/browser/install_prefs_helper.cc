// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/install_prefs_helper.h"

#include "extensions/browser/extension_prefs.h"

namespace extensions {

namespace {

// An installation parameter bundled with an extension.
constexpr PrefMap kInstallParamPrefMap = {
    "install_parameter", PrefType::kString, PrefScope::kExtensionSpecific};

}  // namespace

std::string GetInstallParam(const ExtensionPrefs* prefs,
                            const ExtensionId& extension_id) {
  std::string value;
  // If this fails because the pref isn't set, we return an empty string.
  prefs->ReadPrefAsString(extension_id, kInstallParamPrefMap, &value);
  return value;
}

void SetInstallParam(ExtensionPrefs* prefs,
                     const ExtensionId& extension_id,
                     std::string value) {
  prefs->SetStringPref(extension_id, kInstallParamPrefMap, std::move(value));
}

}  // namespace extensions
