// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/model/autocomplete_result_wrapper_delegate.h"
#import "ui/base/window_open_disposition.h"

struct AutocompleteMatch;
class AutocompleteResult;
@class AutocompleteResultWrapper;
@class OmniboxAutocompleteController;
@protocol OmniboxPopupControllerDelegate;

/// Controller for the omnibox popup.
@interface OmniboxPopupController : NSObject <AutocompleteResultWrapperDelegate>

/// Delegate of the omnibox popup controller.
@property(nonatomic, weak) id<OmniboxPopupControllerDelegate> delegate;

/// Autcomplete result wrapper.
@property(nonatomic, strong)
    AutocompleteResultWrapper* autocompleteResultWrapper;

/// Controller of autocomplete.
@property(nonatomic, weak)
    OmniboxAutocompleteController* omniboxAutocompleteController;

// Disconnects the popup controller.
- (void)disconnect;

#pragma mark - OmniboxAutocomplete event

/// Updates the omnibox popup with sorted`result`.
- (void)updateWithSortedResults:(const AutocompleteResult&)results;

// OmniboxText events should only contain events that don't impact autocomplete.
#pragma mark - OmniboxText events

/// Updates the popup text alignment.
- (void)setTextAlignment:(NSTextAlignment)alignment;

/// Updates the popup semantic content attribute.
- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute;

/// Notifies thumbnail update.
- (void)setHasThumbnail:(BOOL)hasThumbnail;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_H_
