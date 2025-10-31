// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/omnibox/ui/text_field_view_containing.h"

@class LayoutGuideCenter;
@protocol OmniboxTextInput;
@class OmniboxMetricsRecorder;

/// The omnibox container view is the view that is shown in the location bar's
/// edit state. It contains the omnibox textfield and the buttons on the left
/// and right of it.
@interface OmniboxContainerView : UIView <TextFieldViewContaining>

/// The contained omnibox textfield.
@property(nonatomic, strong, readonly) id<OmniboxTextInput> textInput;

/// The metrics recorder.
@property(nonatomic, weak) OmniboxMetricsRecorder* metricsRecorder;

/// The contained clear button. Hide with `setClearButtonHidden`.
@property(nonatomic, strong, readonly) UIButton* clearButton;

/// The contained thumbnail button.
@property(nonatomic, strong, readonly) UIButton* thumbnailButton;

/// The layout guide center to use to refer to the omnibox leading image.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

/// Sets the thumbnail image used for image search. Set to`nil` to hide the
/// thumbnail.
@property(nonatomic, strong) UIImage* thumbnailImage;

/// Initialize the container view with the given frame, text color, tint
/// color and presentation context for the omnibox.
- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                textInputTint:(UIColor*)textInputTint
                     iconTint:(UIColor*)iconTint
          presentationContext:(OmniboxPresentationContext)presentationContext
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

/// Sets the leading button's image and sets its accessibility identifier.
- (void)setLeadingImage:(UIImage*)image
    withAccessibilityIdentifier:(NSString*)accessibilityIdentifier;

/// Sets the scale of the leading image view.
- (void)setLeadingImageScale:(CGFloat)scaleValue;

/// Hides or shows the clear button.
- (void)setClearButtonHidden:(BOOL)isHidden;

/// Updates the height of the text view.
- (void)updateTextViewHeight;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_CONTAINER_VIEW_H_
