// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol AutocompleteSuggestion;
@class OmniboxTextController;

/// Delegate for events in omniboxTextController.
@protocol OmniboxTextControllerDelegate <NSObject>

/// Informs the delegate that an autocomplete suggestion is being previewed.
- (void)omniboxTextController:(OmniboxTextController*)omniboxTextController
         didPreviewSuggestion:(id<AutocompleteSuggestion>)suggestion
                isFirstUpdate:(BOOL)isFirstUpdate;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_CONTROLLER_DELEGATE_H_
