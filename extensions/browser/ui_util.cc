// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/ui_util.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"

namespace extensions {
namespace ui_util {

bool ShouldDisplayInExtensionSettings(Manifest::Type type,
                                      mojom::ManifestLocation location) {
  // Don't show for themes since the settings UI isn't really useful for them.
  if (type == Manifest::Type::kTheme) {
    return false;
  }

  // Hide component extensions because they are only extensions as an
  // implementation detail of Chrome.
  if (Manifest::IsComponentLocation(location) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowComponentExtensionOptions)) {
    return false;
  }

  // Unless they are unpacked, never show hosted apps. Note: We intentionally
  // show packaged apps and platform apps because there are some pieces of
  // functionality that are only available in chrome://extensions/ but which
  // are needed for packaged and platform apps. For example, inspecting
  // background pages. See http://crbug.com/40162419.
  if (!Manifest::IsUnpackedLocation(location) &&
      type == Manifest::TYPE_HOSTED_APP) {
    return false;
  }

  return true;
}

bool ShouldDisplayInExtensionSettings(const Extension& extension) {
  return ShouldDisplayInExtensionSettings(extension.GetType(),
                                          extension.location());
}

std::u16string GetFixupExtensionNameForUIDisplay(
    const std::u16string& extension_name) {
  const size_t extension_name_char_limit =
      75;  // Extension name char limit on CWS
  gfx::BreakType break_type = gfx::BreakType::CHARACTER_BREAK;
  std::u16string fixup_extension_name = gfx::TruncateString(
      extension_name, extension_name_char_limit, break_type);
  return fixup_extension_name;
}

std::u16string GetFixupExtensionNameForUIDisplay(
    const std::string& extension_name) {
  return GetFixupExtensionNameForUIDisplay(base::UTF8ToUTF16(extension_name));
}

}  // namespace ui_util
}  // namespace extensions
