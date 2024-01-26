// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_CONFIG_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_CONFIG_H_

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"

@protocol ContentSuggestionsViewControllerAudience;
@protocol SetUpListConsumerSource;
@class SetUpListItemViewData;

// Config object for a Set Up List module.
@interface SetUpListConfig : MagicStackModule

// List of Set Up List items to show in the module.
@property(nonatomic, strong) NSArray<SetUpListItemViewData*>* setUpListItems;

// YES if the module should be showing a compact SetUpList layout.
@property(nonatomic, assign) BOOL shouldShowCompactModule;

// Set Up List model.
@property(nonatomic, strong) id<SetUpListConsumerSource>
    setUpListConsumerSource;

// Command handler for Set Up List events.
@property(nonatomic, weak) id<ContentSuggestionsViewControllerAudience>
    commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_CONFIG_H_
