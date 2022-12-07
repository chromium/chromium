// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/view_type_util.h"

#include "base/strings/string_piece.h"
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
    base::StringPiece name;
  } constexpr kTypeMap[] = {
      {mojom::ViewType::kAppWindow, "APP_WINDOW"},
      {mojom::ViewType::kExtensionBackgroundPage, "BACKGROUND"},
      {mojom::ViewType::kExtensionDialog, "EXTENSION_DIALOG"},
      {mojom::ViewType::kExtensionPopup, "POPUP"},
      {mojom::ViewType::kTabContents, "TAB"},
      {mojom::ViewType::kExtensionSidePanel, "SIDE_PANEL"},
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
