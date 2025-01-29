// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_H_

#import <Foundation/Foundation.h>

class AutocompleteResult;
@protocol OmniboxPopupControllerDelegate;

/// Controller for the omnibox popup.
@interface OmniboxPopupController : NSObject

/// Delegate of the omnibox popup controller.
@property(nonatomic, weak) id<OmniboxPopupControllerDelegate> delegate;

/// Updates the omnibox popup with `result`.
- (void)updateWithResults:(const AutocompleteResult&)results;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_H_
