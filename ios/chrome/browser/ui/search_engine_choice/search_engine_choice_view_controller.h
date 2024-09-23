// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_consumer.h"

#import <UIKit/UIKit.h>

@protocol SearchEngineChoiceMutator;

// Delegate protocol for SearchEngineViewController.
@protocol SearchEngineChoiceActionDelegate <NSObject>

// Called when the user taps the primary button.
- (void)didTapPrimaryButton;
// Called when the user taps the "Learn More" hyperlink.
- (void)showLearnMore;

@end

// A base view controller for showing a choice screen.
@interface SearchEngineChoiceViewController
    : UIViewController <SearchEngineChoiceConsumer>

// Delegate for all the user actions.
@property(nonatomic, weak) id<SearchEngineChoiceActionDelegate> actionDelegate;
// Delegate to update the selected search engine.
@property(nonatomic, weak) id<SearchEngineChoiceMutator> mutator;

- (instancetype)initWithFirstRunMode:(BOOL)isForFRE
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_VIEW_CONTROLLER_H_
