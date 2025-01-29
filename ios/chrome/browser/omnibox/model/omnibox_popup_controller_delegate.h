// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

class AutocompleteResult;
@class OmniboxPopupController;

/// Delegate for events in omnibox popup controller.
@protocol OmniboxPopupControllerDelegate <NSObject>

/// Notifies the delegate the new suggestions are available.
/// TODO(crbug.com/390410111): Change to AutocompleteSuggestion after moving the
/// wrapping to the controller.
- (void)popupController:(OmniboxPopupController*)popupController
       didUpdateResults:(const AutocompleteResult&)results;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_DELEGATE_H_
