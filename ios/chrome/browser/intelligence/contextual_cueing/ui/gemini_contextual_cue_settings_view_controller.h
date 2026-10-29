// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_UI_GEMINI_CONTEXTUAL_CUE_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_UI_GEMINI_CONTEXTUAL_CUE_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/intelligence/contextual_cueing/ui/gemini_contextual_cue_settings_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class GeminiContextualCueSettingsViewController;
@protocol GeminiContextualCueSettingsMutator;

// Delegate for presentation events related to
// `GeminiContextualCueSettingsViewController`.
@protocol
    GeminiContextualCueSettingsViewControllerPresentationDelegate <NSObject>

// Called when the user taps the Done button.
- (void)geminiContextualCueSettingsViewControllerDidTapDone:
    (GeminiContextualCueSettingsViewController*)controller;

@end

// View controller for the Gemini contextual cue settings bottom sheet.
@interface GeminiContextualCueSettingsViewController
    : ChromeTableViewController <GeminiContextualCueSettingsConsumer>

#pragma mark - Public

// Presentation delegate.
@property(nonatomic, weak)
    id<GeminiContextualCueSettingsViewControllerPresentationDelegate>
        presentationDelegate;

// Mutator for handling user actions in the settings sheet.
@property(nonatomic, weak) id<GeminiContextualCueSettingsMutator> mutator;

// Initializes the view controller.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_UI_GEMINI_CONTEXTUAL_CUE_SETTINGS_VIEW_CONTROLLER_H_
