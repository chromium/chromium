// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UI_UTIL_H_
#define EXTENSIONS_BROWSER_UI_UTIL_H_

#include "extensions/common/manifest.h"

namespace extensions {
class Extension;

namespace ui_util {

// Returns true if an extension with the given `type` and `location` should be
// displayed in the extension settings page (e.g. chrome://extensions).
bool ShouldDisplayInExtensionSettings(Manifest::Type type,
                                      mojom::ManifestLocation location);
// Convenience method of the above taking an Extension object.
bool ShouldDisplayInExtensionSettings(const Extension& extension);

// Returns the extension name to be used in UI surfaces. Name will be truncated
// if its very long, preventing extension name to spoof or break UI surfaces
// (see crbug.com/40063885).
std::u16string GetFixupExtensionNameForUIDisplay(
    const std::u16string& extension_name);
std::u16string GetFixupExtensionNameForUIDisplay(
    const std::string& extension_name);

}  // namespace ui_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UI_UTIL_H_
