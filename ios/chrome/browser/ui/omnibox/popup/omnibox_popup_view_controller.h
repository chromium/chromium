// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/commands/omnibox_suggestion_commands.h"
#import "ios/chrome/browser/ui/omnibox/autocomplete_result_consumer.h"
#import "ios/chrome/browser/ui/omnibox/image_retriever.h"

@protocol ImageRetriever;

// View controller used to display a list of omnibox autocomplete matches in the
// omnibox popup.
// It implements up/down arrow handling to highlight autocomplete results.
// Ideally, that should be implemented as key commands in this view controller,
// but UITextField has standard handlers for up/down arrows, so when the omnibox
// is the first responder, this view controller cannot receive these events.
// Hence the delegation.
@interface OmniboxPopupViewController
    : UIViewController<AutocompleteResultConsumer, OmniboxSuggestionCommands>

// When enabled, this view controller will display shortcuts when no suggestions
// are available. When enabling this, |shortcutsViewController| must be set.
// This can be toggled at runtime, for example to only show shortcuts on regular
// pages and not show them on NTP.
@property(nonatomic, assign) BOOL shortcutsEnabled;
// The view controller to display when no suggestions is available. See also:
// |shortcutsEnabled|.
@property(nonatomic, weak) UIViewController* shortcutsViewController;

@property(nonatomic, assign) BOOL incognito;
@property(nonatomic, weak) id<AutocompleteResultConsumerDelegate> delegate;
@property(nonatomic, weak) id<ImageRetriever> imageRetriever;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_CONTROLLER_H_
