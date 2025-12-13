// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_TEXT_INPUT_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_TEXT_INPUT_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/ui/omnibox_keyboard_delegate.h"

@protocol OmniboxTextInputDelegate;

/// A text input with a pre-edit state, inline autocomplete and additional text.
/// - Pre-edit: the state when the text is "selected" and will erase upon
/// typing. Unlike a normal iOS selection, no selection handles are displayed.
/// - Inline autocomplete: optional autocomplete text following the caret.
/// - Additional text: optional text after user and autocomplete text.
@protocol OmniboxTextInput <UITextInput,
                            OmniboxKeyboardDelegate,
                            UITextPasteConfigurationSupporting>

/// The delegate for this text input.
@property(nonatomic, weak) id<OmniboxTextInputDelegate>
    omniboxTextInputDelegate;

/// Object that handles arrow keys behavior in Omnibox dispatching to multiple
/// OmniboxKeyboardDelegates.
@property(nonatomic, weak) id<OmniboxKeyboardDelegate> omniboxKeyboardDelegate;

/// Text displayed when in pre-edit state.
@property(nonatomic) BOOL clearingPreEditText;

/// Whether the return key is enabled with an empty text.
@property(nonatomic, assign) BOOL allowsReturnKeyWithEmptyText;

/// The text displayed in the text input.
@property(nonatomic, copy) NSString* text;

/// Whether the text input is editing.
@property(nonatomic, readonly, getter=isEditing) BOOL editing;

/// Input accessory view.
@property(nonatomic, strong) UIView* inputAccessoryView;

/// A string that represents the current value of the accessibility element.
@property(nonatomic, readonly) NSString* accessibilityValue;

/// Returns the underlying view of the text input.
- (UIView*)view;

/// Returns the view used for aligning the icon, thumbnail and clear button.
- (UIView*)viewForVerticalAlignment;

/// Returns the text input responder for Scribble.
- (UIResponder<UITextInput>*)scribbleInput;

/// Use to make the view or any subview that is the first responder resign
/// (optionally force)
- (BOOL)endEditing:(BOOL)force;

/// Sets the field's text to `text`.  If `userTextLength` is less than the
/// length of `text`, the excess is displayed as inline autocompleted text. When
/// the field is not in editing mode, it will respect the text attributes set on
/// `text`.
- (void)setText:(NSAttributedString*)text userTextLength:(size_t)userTextLength;

/// Optional text displayed after user and autocomplete text.
- (void)setAdditionalText:(NSString*)additionalText;

/// Inserts the given `text`. The text replaces the current
/// selection, if there is one; otherwise the text is inserted at the current
/// cursor position.  This method should only be called while editing.
- (void)insertTextWhileEditing:(NSString*)text;

/// Returns the text that is displayed in the field, including any inline
/// autocomplete text that may be present. This does not include the additional
/// text.
- (NSString*)displayedText;

/// Returns self.text typed by the user. This doesn't include inline
/// autocomplete nor additional text.
- (NSString*)userText;

/// Returns just the portion of `-displayedText` that is inline autocompleted.
- (NSString*)autocompleteText;

/// Returns YES if this field is currently displaying any inline autocompleted
/// text.
- (BOOL)hasAutocompleteText;

/// Removes any inline autocompleted text and additional text that might be
/// present.
- (void)clearAutocompleteText;

/// Returns any marked text in the field.  Marked text is text that
/// is part of a pending IME composition.
- (NSString*)markedText;

/// Returns the current selected text range as an NSRange.
- (NSRange)selectedNSRange;

/// Returns the most likely text alignment, Left or Right, based on the best
/// language match for `self.text`.
- (NSTextAlignment)bestTextAlignment;

/// Returns the most likely semantic content attribute -- right to left, left to
/// right or unspecified -- based on the first character of `self.text` and the
/// device's current locale.
- (UISemanticContentAttribute)bestSemanticContentAttribute;

/// Checks if direction of the omnibox text changed, and updates the
/// text alignment if necessary.
- (void)updateTextDirection;

/// Returns an x offset for a given `string`. If no such `string` is found,
/// returns some default offset. Used for focus/defocus animation.
- (CGFloat)offsetForString:(NSString*)string;

/// Initial touch on the Omnibox triggers a "pre-edit" state. The current
/// URL is shown without any insertion point. First character typed replaces
/// the URL. A second touch turns on the insertion point.
- (void)enterPreEditState;
- (void)exitPreEditState;
- (BOOL)isPreEditing;

/// Current omnibox font.
- (UIFont*)currentFont;

// Force disable the return key.
- (void)forceDisableReturnKey:(BOOL)forceDisable;

// The default string that displays when there is no other text in the text
// field. Can be locally overridden by `-(void)setCustomPlaceholderText:`.
- (void)setDefaultPlaceholderText:(NSString*)defaultPlaceholderText;

// Sets a custom placeholder text, overriding the default one already set.
// If the custom placeholder is `nil` the system will fallback to the default
// placeholder.
- (void)setCustomPlaceholderText:(NSString*)customPlaceholderText;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_TEXT_INPUT_H_
