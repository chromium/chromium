// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_MEDIATOR_H_

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_mutator.h"

#import <UIKit/UIKit.h>

class PrefService;
class TemplateURLService;
@protocol SearchEngineChoiceConsumer;

@interface SearchEngineChoiceMediator : NSObject<SearchEngineChoiceMutator>

// The delegate object that manages interactions with the Search Engine Choice
// table view.
@property(nonatomic, weak) id<SearchEngineChoiceConsumer> consumer;

- (instancetype)initWithTemplateURLService:
                    (TemplateURLService*)templateURLService
                               prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Save the selected search engine as default.
- (void)saveDefaultSearchEngine;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_MEDIATOR_H_
