// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <string>

@class OmniboxAutocompleteController;
class OmniboxController;
@protocol OmniboxTextControllerDelegate;
@class OmniboxTextFieldIOS;
class OmniboxViewIOS;

/// Controller of the omnibox text.
@interface OmniboxTextController : NSObject

/// Delegate of the omnibox text controller.
@property(nonatomic, weak) id<OmniboxTextControllerDelegate> delegate;

/// Controller of autocomplete.
@property(nonatomic, weak)
    OmniboxAutocompleteController* omniboxAutocompleteController;

/// Omnibox textfield.
@property(nonatomic, weak) OmniboxTextFieldIOS* textField;

/// Temporary initializer, used during the refactoring. crbug.com/390409559
- (instancetype)initWithOmniboxController:(OmniboxController*)omniboxController
                           omniboxViewIOS:(OmniboxViewIOS*)omniboxViewIOS
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Removes all C++ references.
- (void)disconnect;

#pragma mark - Autocomplete event

/// Sets the additional text.
- (void)setAdditionalText:(const std::u16string&)text;

#pragma mark - Omnibox text event

/// Called when the user removes the additional text.
- (void)onUserRemoveAdditionalText;

/// Called when a thumbnail is set.
- (void)onThumbnailSet:(BOOL)hasThumbnail;

/// Called when the thumbnail has been removed during omnibox edit.
- (void)onUserRemoveThumbnail;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_CONTROLLER_H_
