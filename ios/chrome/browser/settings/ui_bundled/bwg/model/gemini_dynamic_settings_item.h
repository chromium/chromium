// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_DYNAMIC_SETTINGS_ITEM_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_DYNAMIC_SETTINGS_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"

@class GeminiSettingsAction;
@class GeminiSettingsMetadata;

// An item representing a Gemini dynamic setting.
@interface GeminiDynamicSettingsItem : TableViewDetailTextItem

// The action to perform when this item is selected.
@property(nonatomic, strong, readonly) GeminiSettingsAction* action;

// Gemini settings metadata for this setting.
@property(nonatomic, strong, readonly) GeminiSettingsMetadata* metadata;

// Designated initializer.
- (instancetype)initWithType:(NSInteger)type
                    metadata:(GeminiSettingsMetadata*)metadata
                      action:(GeminiSettingsAction*)action
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_MODEL_GEMINI_DYNAMIC_SETTINGS_ITEM_H_
