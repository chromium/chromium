// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_

#import "base/ios/block_types.h"

enum class ContentSuggestionsModuleType;
@class ContentSuggestionsWhatsNewItem;
@class MostVisitedTilesConfig;
@class SafetyCheckState;
@class SetUpListConfig;
enum class SetUpListItemType;
@class SetUpListItemViewData;
@class ShortcutsConfig;
@class ParcelTrackingItem;
@class TabResumptionItem;

// MagicStackOrderChange is used in `updateMagicStackOrder:withStatus:` to
// indicate what module has changed and how it needs to be updated.
struct MagicStackOrderChange {
  enum class Type {
    kInsert = 1,
    kRemove,
    kReplace,
  };
  Type type;
  // New Module. Will be set for kReplace and kInsert.
  ContentSuggestionsModuleType new_module;
  // Old Module. Will be set for kReplace and kRemove.
  ContentSuggestionsModuleType old_module;
  // The index of `newModule`, applies to `oldModule` for kRemove.
  NSInteger index;
};

// Supports adding/removing/updating UI elements to the ContentSuggestions
// UIViewController.
@protocol ContentSuggestionsConsumer

// Indicates to the consumer the current Most Visited tiles to show with
// `config`.
- (void)setMostVisitedTilesConfig:(MostVisitedTilesConfig*)config;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_
