// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class OmniboxPopupController;
@protocol AutocompleteSuggestionGroup;

/// Delegate for events in omnibox popup controller.
@protocol OmniboxPopupControllerDelegate <NSObject>

/// Notifies the delegate of the updated suggestions groups.
- (void)popupController:(OmniboxPopupController*)popupController
    didUpdateSuggestionsGroups:
        (NSArray<id<AutocompleteSuggestionGroup>>*)suggestionGroups;

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

/// Notifies the delegate when pedals are invalidated.
- (void)popupController:(OmniboxPopupController*)popupController
    didInvalidatePedals:
        (NSArray<id<AutocompleteSuggestionGroup>>*)suggestionGroups;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_DELEGATE_H_
