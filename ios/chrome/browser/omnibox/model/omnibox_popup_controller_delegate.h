// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

class AutocompleteResult;
@class OmniboxPopupController;

/// Delegate for events in omnibox popup controller.
@protocol OmniboxPopupControllerDelegate <NSObject>

/// Notifies the delegate the new suggestions are available.
- (void)popupControllerDidUpdateSuggestions:
            (OmniboxPopupController*)popupController
                             hasSuggestions:(BOOL)hasSuggestions
                                 isFocusing:(BOOL)isFocusing;

/// Notifies the delegate of the new sorted suggestions.
/// TODO(crbug.com/390410111): Change to AutocompleteSuggestion after moving the
/// wrapping to the controller.
- (void)popupController:(OmniboxPopupController*)popupController
         didSortResults:(const AutocompleteResult&)results;

/// Notifies the delegate of text alignment change.
- (void)popupController:(OmniboxPopupController*)popupController
    didUpdateTextAlignment:(NSTextAlignment)alignment;

/// Notifies the delegate of semantic content attribute change
- (void)popupController:(OmniboxPopupController*)popupController
    didUpdateSemanticContentAttribute:
        (UISemanticContentAttribute)semanticContentAttribute;

/// Notifies the delegate of thumbnail update.
- (void)popupController:(OmniboxPopupController*)popupController
    didUpdateHasThumbnail:(BOOL)hasThumbnail;
@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_DELEGATE_H_
