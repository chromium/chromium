// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_TEXT_FIELD_VIEW_CONTAINING_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_TEXT_FIELD_VIEW_CONTAINING_H_

@protocol TextFieldViewContaining;

// Delegate for height change notification.
@protocol TextFieldViewContainingHeightDelegate <NSObject>

// Notifies that the text input view requires a new `height`.
- (void)textFieldViewContaining:(UIView<TextFieldViewContaining>*)sender
                didChangeHeight:(CGFloat)height;

@end

// A protocol that defines an object that contains a text field view.
@protocol TextFieldViewContaining

// A text field view.
@property(nonatomic, readonly) UIView* textFieldView;

// Delegate to notify of height changes.
@property(nonatomic, weak) id<TextFieldViewContainingHeightDelegate>
    heightDelegate;

// The number of lines in the text field.
@property(nonatomic, readonly) NSUInteger numberOfLines;

// Hide the leading image in the omnibox.
- (void)hideLeadingImage:(BOOL)hideLeadingImage;

// Sets the alpha value for the leading image.
- (void)setLeadingImageAlpha:(BOOL)alpha;

// Whether to force disable the return key on the keyboard.
- (void)forceDisableReturnKey:(BOOL)forceDisable;

/// Whether the return key is enabled with an empty text.
- (void)setAllowsReturnKeyWithEmptyText:(BOOL)allowsReturnKeyWithEmptyText;

// Sets a custom placeholder text, overriding the default one already set.
// If the custom placeholder is `nil` the system will fallback to the default
// placeholder.
- (void)setCustomPlaceholderText:(NSString*)customPlaceholderText;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_TEXT_FIELD_VIEW_CONTAINING_H_
