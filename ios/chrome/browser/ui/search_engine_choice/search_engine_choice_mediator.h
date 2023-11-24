// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_MEDIATOR_H_

#import <UIKit/UIKit.h>

class FaviconLoader;
@protocol SearchEngineChoiceConsumer;
@class SnippetSearchEngineItem;

// Mediator that handles the selection operations.
@interface SearchEngineChoiceMediator : NSObject

// The delegate object that manages interactions the Search Engine Choice view.
@property(nonatomic, weak) id<SearchEngineChoiceConsumer> consumer;
// The item selected by the user. Set when the user taps on a row of the search
// engines choice table.
@property(nonatomic, weak) SnippetSearchEngineItem* selectedItem;

- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_MEDIATOR_H_
