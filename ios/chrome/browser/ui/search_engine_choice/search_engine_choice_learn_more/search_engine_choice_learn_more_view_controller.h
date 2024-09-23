// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_LEARN_MORE_SEARCH_ENGINE_CHOICE_LEARN_MORE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_LEARN_MORE_SEARCH_ENGINE_CHOICE_LEARN_MORE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@class SearchEngineChoiceLearnMoreViewController;

// Delegate protocol for SearchEngineChoiceLearnMoreViewController.
@protocol SearchEngineChoiceLearnMoreDelegate <NSObject>

// Called when the user taps the dismiss button.
- (void)learnMoreDone:
    (SearchEngineChoiceLearnMoreViewController*)viewController;

@end

// A base view controller for showing an informational screen.
@interface SearchEngineChoiceLearnMoreViewController : UIViewController

// View controller delegate.
@property(nonatomic, weak) id<SearchEngineChoiceLearnMoreDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_LEARN_MORE_SEARCH_ENGINE_CHOICE_LEARN_MORE_VIEW_CONTROLLER_H_
