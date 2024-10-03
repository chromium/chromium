// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_ranking_model.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_ranking_model_delegate.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class Browser;
@protocol MagicStackConsumer;
@class MagicStackRankingModel;
@class MostVisitedTilesMediator;
@class SetUpListMediator;
@class ShortcutsMediator;

// Mediator for ContentSuggestions.
@interface ContentSuggestionsMediator
    : NSObject <MagicStackRankingModelDelegate>

// Registers the feature preferences.
+ (void)registerProfilePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Ranking Model for the Magic Stack.
@property(nonatomic, weak) MagicStackRankingModel* magicStackRankingModel;

// The consumer that will be notified when the data change.
@property(nonatomic, weak) id<ContentSuggestionsConsumer> consumer;

// The consumer that will be notified when the underlying Magic Stack data
// changes.
@property(nonatomic, weak) id<MagicStackConsumer> magicStackConsumer;

@property(nonatomic, weak) MostVisitedTilesMediator* mostVisitedTilesMediator;
@property(nonatomic, weak) SetUpListMediator* setUpListMediator;
@property(nonatomic, weak) ShortcutsMediator* shortcutsMediator;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_
