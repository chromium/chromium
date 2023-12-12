// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_SEARCH_ENGINE_CHOICE_TABLE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_SEARCH_ENGINE_CHOICE_TABLE_MEDIATOR_H_

#import <UIKit/UIKit.h>

class FaviconLoader;
class PrefService;
class TemplateURLService;
@protocol SearchEngineChoiceFaviconUpdateConsumer;
@protocol SearchEngineChoiceTableConsumer;

@interface SearchEngineChoiceTableMediator : NSObject

- (instancetype)initWithTemplateURLService:
                    (TemplateURLService*)templateURLService
                               prefService:(PrefService*)prefService
                             faviconLoader:(FaviconLoader*)faviconLoader
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The delegate object that manages interactions with the Search Engine Choice
// table view.
@property(nonatomic, weak) id<SearchEngineChoiceTableConsumer> consumer;

// The delegate object that manages interactions with the Search Engine Choice
// view.
@property(nonatomic, weak) id<SearchEngineChoiceFaviconUpdateConsumer>
    faviconUpdateConsumer;

// Index of the row tapped by the user.
@property(nonatomic, assign) NSInteger selectedRow;

// Save the selected search engine as default.
- (void)saveDefaultSearchEngine;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_SEARCH_ENGINE_CHOICE_TABLE_MEDIATOR_H_
