// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/text_field_view_containing.h"

@class LayoutGuideCenter;
@class OmniboxTextFieldIOS;

/// The omnibox container view is the view that is shown in the location bar's
/// edit state. It contains the omnibox textfield and the buttons on the left
/// and right of it.
@interface OmniboxContainerView : UIView <TextFieldViewContaining>

/// The contained omnibox textfield.
@property(nonatomic, strong, readonly) OmniboxTextFieldIOS* textField;

/// The contained clear button. Hide with `setClearButtonHidden`.
@property(nonatomic, strong, readonly) UIButton* clearButton;

/// The contained thumbnail button.
@property(nonatomic, strong, readonly) UIButton* thumbnailButton;

/// The layout guide center to use to refer to the omnibox leading image.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

/// Sets the thumbnail image used for image search. Set to`nil` to hide the
/// thumbnail.
@property(nonatomic, strong) UIImage* thumbnailImage;

/// Initialize the container view with the given frame, text color, and tint
/// color for omnibox.
- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                textFieldTint:(UIColor*)textFieldTint
                     iconTint:(UIColor*)iconTint
                isLensOverlay:(BOOL)isLensOverlay NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

/// Sets the leading button's image and sets its accessibility identifier.
- (void)setLeadingImage:(UIImage*)image
    withAccessibilityIdentifier:(NSString*)accessibilityIdentifier;

/// Sets the scale of the leading image view.
- (void)setLeadingImageScale:(CGFloat)scaleValue;

/// Hides or shows the clear button. TODO(b/325035406): cleanup with
/// kRichAutocompletion.
- (void)setClearButtonHidden:(BOOL)isHidden;

/// Notifies the consumer to update the additional text. Set to nil to remove
/// additional text.
- (void)updateAdditionalText:(NSString*)additionalText;

/// Notifies the consumer whether the omnibox has a rich inline default
/// suggestion. Only used when `RichAutocompletion` is enabled without
/// additional text.
- (void)setOmniboxHasRichInline:(BOOL)omniboxHasRichInline;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTAINER_VIEW_H_
