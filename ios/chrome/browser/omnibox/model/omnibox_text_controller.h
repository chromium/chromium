// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <string>

#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"
#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"

@protocol AutocompleteSuggestion;
@class OmniboxAutocompleteController;
class OmniboxClient;
@protocol OmniboxFocusDelegate;
@protocol OmniboxTextControllerDelegate;
@protocol OmniboxTextInput;
@class OmniboxTextFieldIOS;

/// Controller of the omnibox text.
@interface OmniboxTextController : NSObject

/// Delegate of the omnibox text controller.
@property(nonatomic, weak) id<OmniboxTextControllerDelegate> delegate;

/// Omnibox focus delegate.
@property(nonatomic, weak) id<OmniboxFocusDelegate> focusDelegate;

/// Controller of autocomplete.
@property(nonatomic, weak)
    OmniboxAutocompleteController* omniboxAutocompleteController;

/// Omnibox text input.
@property(nonatomic, weak) id<OmniboxTextInput> textInput;

/// Returns the current selection range.
@property(nonatomic, assign, readonly) NSRange currentSelection;

- (instancetype)initWithOmniboxClient:(OmniboxClient*)omniboxClient
                     omniboxTextModel:(OmniboxTextModel*)omniboxTextModel
                  presentationContext:
                      (OmniboxPresentationContext)presentationContext
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

/// Notifies the client about input changes.
- (void)notifyClientOnUserInputInProgressChange:(BOOL)changedToUserInProgress;

/// Retrieves the current text input selection bounds.
- (void)getSelectionBounds:(size_t*)start end:(size_t*)end;

/// Reverts the edit and popup back to their unedited state (permanent text
/// showing, popup closed, no user input in progress).
- (void)revertAll;

/// Returns the current text field displayed text.
- (std::u16string)displayedText;

/// Updates the text model input_in_progress state.
- (void)setInputInProgress:(BOOL)inProgress;

/// Reverts the text model back to its unedited state (permanent text showing,
/// no user input in progress).
- (void)revertState;

/// Copies a match corresponding to the current text into `match`, and
/// populates `alternate_nav_url` as well if it's not nullptr. If the popup
/// is closed, the match is generated from the autocomplete classifier.
- (void)getInfoForCurrentText:(AutocompleteMatch*)match
       alternateNavigationURL:(GURL*)alternateNavigationURL;

/// Sets the user_text_ to `text`. Also enters user-input-in-progress mode.
/// Virtual for testing.
- (void)setUserText:(const std::u16string&)text;

/// Returns the match for the current text. If the user has not edited the text
/// this is the match corresponding to the permanent text. Returns the
/// alternate nav URL, if `alternateNavURL` is non-NULL and there is such a
/// URL.
- (AutocompleteMatch)currentMatch:(GURL*)alternateNavURL;

/// Invoked any time the text may have changed in the edit.
- (void)onTextChanged;

/// Called when any relevant data changes.  This rolls together several
/// separate pieces of data into one call so we can update all the UI
/// efficiently. Specifically, it's invoked for autocompletion.
///   `inline_autocompletion` is the autocompletion.
///   `additional_text` is additional omnibox text to be displayed adjacent to
///     the omnibox view.
///   `new_match` is the selected match when the user is changing selection,
///    the default match if the user is typing, or an empty match when
///    selecting a header.
- (void)onPopupDataChanged:(const std::u16string&)inlineAutocompletion
            additionalText:(const std::u16string&)additionalText
                  newMatch:(const AutocompleteMatch&)newMatch;

/// Resets the permanent display texts `url_for_editing` to those provided by
/// the controller. Returns true if the display text shave changed and the
/// change should be immediately user-visible, because either the user is not
/// editing or the edit does not have focus.
- (bool)resetDisplayTexts;

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
