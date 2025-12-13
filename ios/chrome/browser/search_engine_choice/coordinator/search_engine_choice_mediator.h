// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_COORDINATOR_SEARCH_ENGINE_CHOICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_COORDINATOR_SEARCH_ENGINE_CHOICE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_mutator.h"

class TemplateURLService;
@protocol SearchEngineChoiceConsumer;

namespace regional_capabilities {
class RegionalCapabilitiesService;
}  // namespace regional_capabilities
namespace search_engines {
class SearchEngineChoiceService;
}  // namespace search_engines

@interface SearchEngineChoiceMediator : NSObject <SearchEngineChoiceMutator>

// The delegate object that manages interactions with the Search Engine Choice
// table view.
@property(nonatomic, weak) id<SearchEngineChoiceConsumer> consumer;

- (instancetype)
     initWithTemplateURLService:(TemplateURLService*)templateURLService
      searchEngineChoiceService:
          (search_engines::SearchEngineChoiceService*)searchEngineChoiceService
    regionalCapabilitiesService:
        (regional_capabilities::RegionalCapabilitiesService*)
            regionalCapabilitiesService NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Save the selected search engine as default.
- (void)saveDefaultSearchEngine;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_COORDINATOR_SEARCH_ENGINE_CHOICE_MEDIATOR_H_
