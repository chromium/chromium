// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/view_type_util.h"

#include <string_view>

#include "extensions/common/mojom/view_type.mojom.h"

namespace extensions {

bool GetViewTypeFromString(const std::string& view_type,
                           mojom::ViewType* view_type_out) {
  // TODO(devlin): This map doesn't contain the following values:
  // - mojom::ViewType::kBackgroundContents
  // - mojom::ViewType::kComponent
  // - mojom::ViewType::kExtensionGuest
  // Why? Is it just because we don't expose those types to JS?
  static const struct {
    mojom::ViewType type;
    std::string_view name;
  } constexpr kTypeMap[] = {
      {mojom::ViewType::kAppWindow, "APP_WINDOW"},
      {mojom::ViewType::kExtensionBackgroundPage, "BACKGROUND"},
      {mojom::ViewType::kExtensionPopup, "POPUP"},
      {mojom::ViewType::kTabContents, "TAB"},
      {mojom::ViewType::kExtensionSidePanel, "SIDE_PANEL"},
      {mojom::ViewType::kDeveloperTools, "DEVELOPER_TOOLS"},
  };

  for (const auto& entry : kTypeMap) {
    if (entry.name == view_type) {
      *view_type_out = entry.type;
      return true;
    }
  }

  return false;
}

}  // namespace extensions
