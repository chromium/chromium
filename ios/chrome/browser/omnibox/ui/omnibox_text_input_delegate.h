// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_TEXT_INPUT_DELEGATE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_TEXT_INPUT_DELEGATE_H_

@protocol OmniboxTextInput;

// Delegate protocol for `OmniboxTextInput`. It allows the input view
// to report raw user interaction and editing events to its controller, which
// can then forward them to the mediator and model layers.
@protocol OmniboxTextInputDelegate <NSObject>

// Reports that the text is about to change.
- (BOOL)textInput:(id<OmniboxTextInput>)textInput
    shouldChangeTextInRange:(NSRange)range
          replacementString:(NSString*)newText;

// Reports that the text has changed.
- (void)textInputDidChange:(id<OmniboxTextInput>)textInput;

// Reports that the text has changed, only the UI should be updated,
// autocomplete should not be triggered. This is called when the text is changed
// by the user or by the setting it.
- (void)textInputDidUpdateUIForText:(id<OmniboxTextInput>)textInput;

// Reports the return action should be accepted.
- (BOOL)textInputShouldReturn:(id<OmniboxTextInput>)textInput;

// Reports that editing has begun.
- (void)textInputDidBeginEditing:(id<OmniboxTextInput>)textInput;

// Reports that editing has ended.
- (void)textInputDidEndEditing:(id<OmniboxTextInput>)textInput;

// Reports a backspace event.
- (void)textInputDidDeleteBackward:(id<OmniboxTextInput>)textInput;

// Reports that the user has accepted the inline autocomplete suggestion.
- (void)textInputDidAcceptAutocomplete:(id<OmniboxTextInput>)textInput;

// Reports that the user has removed the "additional text".
- (void)textInputDidRemoveAdditionalText:(id<OmniboxTextInput>)textInput;

// Reports that the user has accepted the input (e.g., by pressing Return).
- (void)textInputDidAcceptInput:(id<OmniboxTextInput>)textInput;

// Reports a copy event.
- (void)textInputDidCopy:(id<OmniboxTextInput>)textInput;

// Reports that the view is about to perform a paste.
- (void)textInputWillPaste:(id<OmniboxTextInput>)textInput;

// Called when the UIPasteControl in the omnibox's keyboard accessory is shown.
// Returns whether or not the paste control should be enabled.
- (BOOL)textInput:(id<OmniboxTextInput>)textInput
    canPasteItemProviders:(NSArray<NSItemProvider*>*)itemProviders;

// Called when the UIPasteControl in the omnibox's keyboard accessory is tapped.
- (void)textInput:(id<OmniboxTextInput>)textInput
    pasteItemProviders:(NSArray<NSItemProvider*>*)itemProviders;

- (UIMenu*)textInput:(id<OmniboxTextInput>)textInput
    editMenuForCharactersInRange:(NSRange)range
                suggestedActions:(NSArray<UIMenuElement*>*)suggestedActions;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_TEXT_INPUT_DELEGATE_H_
