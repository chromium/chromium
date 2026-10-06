// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ITEM_TYPE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ITEM_TYPE_H_

#import "ios/chrome/browser/composebox/public/composebox_mode.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"

enum class ComposeboxMenuItemType {
  kUnknown = 0,
  kAIM,
  kCreateImage,
  kDeepSearch,
  kCanvas,
  kModelRegular,
  kModelAuto,
  kModelThinking,
  kCurrentTab,
  kModelThinkingNoGenUI,
  kModelFlash,
  kAttachmentTabs,
  kAttachmentCamera,
  kAttachmentGallery,
  kAttachmentFiles,
  kAttachmentDrive,
  kAttachmentSharedTabs,
};

// Maps a model option to its corresponding menu item type.
ComposeboxMenuItemType MenuItemTypeForModel(ComposeboxModelOption option);

// Maps a tool mode to its corresponding menu item type.
ComposeboxMenuItemType MenuItemTypeForTool(ComposeboxMode mode);

// Returns YES if the menu item type represents a tool (which can be toggled).
bool IsToolType(ComposeboxMenuItemType type);

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ITEM_TYPE_H_
