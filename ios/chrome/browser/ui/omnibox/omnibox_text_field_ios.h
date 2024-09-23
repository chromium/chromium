// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_IOS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/omnibox_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_delegate.h"

/// UITextField subclass to allow for adjusting borders.
/// A textfield with a pre-edit state, inline autocomplete and additional text.
/// - Pre-edit: the state when the text is "selected" and will erase upon
/// typing. Unlike a normal iOS selection, no selection handles are displayed.
/// - Inline autocomplete: optional autocomplete text following the caret.
/// - Additional text: optional text after user and autocomplete text.
@interface OmniboxTextFieldIOS : UITextField <OmniboxKeyboardDelegate>

/// The delegate for this textfield.  Overridden to use OmniboxTextFieldDelegate
/// instead of UITextFieldDelegate.
@property(nonatomic, weak) id<OmniboxTextFieldDelegate> delegate;
/// Object that handles arrow keys behavior in Omnibox dispatching to multiple
/// OmniboxKeyboardDelegates.
@property(nonatomic, weak) id<OmniboxKeyboardDelegate> omniboxKeyboardDelegate;

/// Text displayed when in pre-edit state.
@property(nonatomic) BOOL clearingPreEditText;
/// Optional text displayed after user and autocomplete text.
@property(nonatomic, strong) NSAttributedString* additionalText;

/// Whether the omnibox has a rich inline default suggestion. Only used when
/// `RichAutocompletion` is enabled with no additional text.
@property(nonatomic, assign) BOOL omniboxHasRichInline;

/// Whether the return key is enabled with an empty textfield.
@property(nonatomic, assign) BOOL allowsReturnKeyWithEmptyText;

/// Initialize the omnibox with the given `frame`, `textColor`, and `tintColor`.
- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                    tintColor:(UIColor*)tintColor;

/// Sets the field's text to `text`.  If `userTextLength` is less than the
/// length of `text`, the excess is displayed as inline autocompleted text. When
/// the field is not in editing mode, it will respect the text attributes set on
/// `text`.
- (void)setText:(NSAttributedString*)text userTextLength:(size_t)userTextLength;

/// Inserts the given `text` into the text field. The text replaces the current
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
/// UITextField. alignment if necessary.
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

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_IOS_H_
