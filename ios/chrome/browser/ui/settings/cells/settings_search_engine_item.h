// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SEARCH_ENGINE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SEARCH_ENGINE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@class FaviconAttributes;
@class FaviconView;

// This protocol is implemented by SettingsSearchEngineItem and
// LegacySettingsSearchEngineItem. This can be removed once
// LegacySettingsSearchEngineItem is removed.
@protocol SettingsSearchEngineItem <NSObject>

// The enabled/disabled state. If disabled, user interaction will be forbidden
// and cell's alpha will be reduced.
@property(nonatomic, assign) BOOL enabled;
// The text for the title.
@property(nonatomic, readwrite, copy) NSString* text;
// The text for the subtitle.
@property(nonatomic, readwrite, copy) NSString* detailText;

@end

// SettingsSearchEngineItem contains the model data for a TableViewURLCell.
@interface SettingsSearchEngineItem : TableViewItem <SettingsSearchEngineItem>

// Sets the favicon.
@property(nonatomic, strong) FaviconAttributes* faviconAttributes;

@end

@interface SettingsSearchEngineCell : TableViewCell

// Cell image.
@property(nonatomic, strong, readonly) FaviconView* faviconView;
// Cell title.
@property(nonatomic, strong, readonly) UILabel* textLabel;
// Cell subtitle.
@property(nonatomic, strong, readonly) UILabel* detailTextLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SEARCH_ENGINE_ITEM_H_
