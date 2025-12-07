// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_MUTATOR_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_MUTATOR_H_

#import <UIKit/UIKit.h>

/// Mutator for the omnibox.
@protocol OmniboxMutator <NSObject>

/// Removes the thumbnail.
- (void)removeThumbnail;

/// Removes the additional text.
- (void)removeAdditionalText;

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

#pragma mark - Textfield delegate forwaring

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

#pragma mark - ContextMenu event

/// User tapped on the keyboard accessory's paste button.
- (void)pasteToSearch:(NSArray<NSItemProvider*>*)itemProviders;

/// User tapped on the Search Copied Text from the omnibox menu.
- (void)searchCopiedText;

/// User tapped on the Search Copied Image from the omnibox menu.
- (void)searchCopiedImage;

/// User tapped on the Lens Image from the omnibox menu.
- (void)lensCopiedImage;

/// User tapped on the Visit Copied Link from the omnibox menu.
- (void)visitCopiedLink;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_MUTATOR_H_
