// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item_type.h"

ComposeboxMenuItemType MenuItemTypeForModel(ComposeboxModelOption option) {
  switch (option) {
    case ComposeboxModelOption::kRegular:
      return ComposeboxMenuItemType::kModelRegular;
    case ComposeboxModelOption::kAuto:
      return ComposeboxMenuItemType::kModelAuto;
    case ComposeboxModelOption::kThinking:
      return ComposeboxMenuItemType::kModelThinking;
    case ComposeboxModelOption::kThinkingNoGenUI:
      return ComposeboxMenuItemType::kModelThinkingNoGenUI;
    case ComposeboxModelOption::kFlash:
      return ComposeboxMenuItemType::kModelFlash;
    case ComposeboxModelOption::kNone:
      return ComposeboxMenuItemType::kUnknown;
  }
}
