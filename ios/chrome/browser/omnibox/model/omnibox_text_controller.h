// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <string>

@protocol AutocompleteSuggestion;
@class OmniboxAutocompleteController;
class OmniboxControllerIOS;
@protocol OmniboxFocusDelegate;
@protocol OmniboxTextControllerDelegate;
@class OmniboxTextFieldIOS;
class OmniboxViewIOS;

/// Controller of the omnibox text.
@interface OmniboxTextController : NSObject

/// Delegate of the omnibox text controller.
@property(nonatomic, weak) id<OmniboxTextControllerDelegate> delegate;

/// Omnibox focus delegate.
@property(nonatomic, weak) id<OmniboxFocusDelegate> focusDelegate;

/// Controller of autocomplete.
@property(nonatomic, weak)
    OmniboxAutocompleteController* omniboxAutocompleteController;

/// Omnibox textfield.
@property(nonatomic, weak) OmniboxTextFieldIOS* textField;

/// Temporary initializer, used during the refactoring. crbug.com/390409559
- (instancetype)initWithOmniboxController:
                    (OmniboxControllerIOS*)omniboxController
                           omniboxViewIOS:(OmniboxViewIOS*)omniboxViewIOS
                            inLensOverlay:(BOOL)inLensOverlay
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Removes all C++ references.
- (void)disconnect;

/// Updates the omnibox text based on its current client state.
- (void)updateAppearance;

/// Returns whether the omnibox is first responder.
- (BOOL)isOmniboxFirstResponder;

/// Focuses the omnibox.
- (void)focusOmnibox;

/// Ends omnibox editing / defocus the omnibox.
- (void)endEditing;

/// Inserts text into the omnibox without triggering autocomplete.
- (void)insertTextToOmnibox:(NSString*)text;

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

/// Clears the Omnibox text.
- (void)clearText;

/// Accepts the current input / default suggestion.
- (void)acceptInput;

/// Prepares the omnibox for scribble.
- (void)prepareForScribble;

/// Cleans up the omnibox after scribble.
- (void)cleanupAfterScribble;

/// Called when the text input mode changed.
- (void)onTextInputModeChange;

/// Called when the omnibox text field starts editing.
- (void)onDidBeginEditing;

/// Called before the omnibox text field changes. `newText` will replace the
/// text currently in `range`.
- (BOOL)shouldChangeCharactersInRange:(NSRange)range
                    replacementString:(NSString*)newText;

/// Called after the omnibox text field changes.
/// `processingUserEvent`: Whether the change is user initiated.
- (void)textDidChangeWithUserEvent:(BOOL)isProcessingUserEvent;

/// Called when autocomplete text is accepted. (e.g. tap on autocomplete text,
/// tap on left/right arrow key).
- (void)onAcceptAutocomplete;

/// Called when the Omnibox text field should copy.
- (void)onCopy;

/// Called when the Omnibox text field should paste.
- (void)willPaste;

/// Called when the backspace button is pressed in the Omnibox text field.
- (void)onDeleteBackward;

#pragma mark - Omnibox popup event

/// Sets the currently previewed autocomplete suggestion.
- (void)previewSuggestion:(id<AutocompleteSuggestion>)suggestion
            isFirstUpdate:(BOOL)isFirstUpdate;

/// Notifies of scroll event.
- (void)onScroll;

/// Hides the keyboard.
- (void)hideKeyboard;

/// Refines omnibox content with `text`.
- (void)refineWithText:(const std::u16string&)text;

#pragma mark - Private event
// Events that are private. Removed from header after refactoring
// (crbug.com/390409559). Since these methods should be private, comments are in
// the implementation file.

- (void)setCaretPos:(NSUInteger)caretPos;

- (void)startAutocompleteAfterEdit;

- (void)setWindowText:(const std::u16string&)text
             caretPos:(size_t)caretPos
    startAutocomplete:(BOOL)startAutocomplete
    notifyTextChanged:(BOOL)notifyTextChanged;

- (void)updateAutocompleteIfTextChanged:(const std::u16string&)userText
                         autocompletion:
                             (const std::u16string&)inlineAutocomplete;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_CONTROLLER_H_
