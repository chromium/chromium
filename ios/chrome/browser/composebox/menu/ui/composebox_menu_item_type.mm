// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item_type.h"

ComposeboxMenuItemType MenuItemTypeForTool(ComposeboxMode mode) {
  switch (mode) {
    case ComposeboxMode::kAIM:
      return ComposeboxMenuItemType::kAIM;
    case ComposeboxMode::kImageGeneration:
      return ComposeboxMenuItemType::kCreateImage;
    case ComposeboxMode::kDeepSearch:
      return ComposeboxMenuItemType::kDeepSearch;
    case ComposeboxMode::kCanvas:
      return ComposeboxMenuItemType::kCanvas;
    case ComposeboxMode::kRegularSearch:
      return ComposeboxMenuItemType::kUnknown;
  }
}

bool IsToolType(ComposeboxMenuItemType type) {
  switch (type) {
    case ComposeboxMenuItemType::kAIM:
    case ComposeboxMenuItemType::kCreateImage:
    case ComposeboxMenuItemType::kDeepSearch:
    case ComposeboxMenuItemType::kCanvas:
      return true;
    default:
      return false;
  }
}
