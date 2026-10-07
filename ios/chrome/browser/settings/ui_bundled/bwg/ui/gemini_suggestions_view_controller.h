// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_GEMINI_SUGGESTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_GEMINI_SUGGESTIONS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_suggestions_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@class GeminiSuggestionsViewController;
@protocol GeminiSettingsMutator;

// Delegate for presentation events related to
// `GeminiSuggestionsViewController`.
@protocol GeminiSuggestionsViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)geminiSuggestionsViewControllerWasRemoved:
    (GeminiSuggestionsViewController*)controller;

@end

// View controller related to the Gemini suggestions setting.
@interface GeminiSuggestionsViewController
    : SettingsRootTableViewController <GeminiSuggestionsConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak)
    id<GeminiSuggestionsViewControllerPresentationDelegate>
        presentationDelegate;

// Used for sending model data updates to the mediator.
@property(nonatomic, weak) id<GeminiSettingsMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_GEMINI_SUGGESTIONS_VIEW_CONTROLLER_H_
