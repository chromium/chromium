// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/omnibox_container_view.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/omnibox/public/omnibox_constants.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_ios.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_thumbnail_button.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/animation_util.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "skia/ext/skia_utils_ios.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/color_palette.h"
#import "ui/gfx/image/image.h"

namespace {

/// Width of the thumbnail.
const CGFloat kThumbnailWidth = 48;
/// Height of the thumbnail.
const CGFloat kThumbnailHeight = 40;
/// Space between the thumbnail image and the omnibox text.
const CGFloat kThumbnailImageTrailingMargin = 10;
/// Space between the leading icon and the thumbnail image.
const CGFloat kThumbnailImageLeadingMargin = 9;

// The leading image margins when presented in the lens overlay.
const CGFloat kLeadingImageLeadingMarginLensOverlay = 12;
const CGFloat kLeadingImageTrailingMarginLensOverlay = 9;

/// Space between the clear button and the edge of the omnibox.
const CGFloat kTextFieldClearButtonTrailingOffset = 4;

/// Clear button inset on all sides.
const CGFloat kClearButtonInset = 4.0f;
/// Clear button image size.
const CGFloat kClearButtonImageSize = 17.0f;

}  // namespace

#pragma mark - OmniboxContainerView

@interface OmniboxContainerView ()

// Redefined as readwrite.
@property(nonatomic, strong) OmniboxTextFieldIOS* textField;
// Redefined as readwrite.
@property(nonatomic, strong) UIButton* clearButton;

@end

@implementation OmniboxContainerView {
  /// The leading image view. Used for autocomplete icons.
  UIImageView* _leadingImageView;
  /// Horizontal stack view containing the `_leadingImageView`,
  /// `_thumbnailImageView`, `textField`.
  UIStackView* _stackView;
  // The image thumnail button.
  OmniboxThumbnailButton* _thumbnailButton;

  /// Whether the view is presented in the lens overlay.
  BOOL _isLensOverlay;
}

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                textFieldTint:(UIColor*)textFieldTint
                     iconTint:(UIColor*)iconTint
                isLensOverlay:(BOOL)isLensOverlay {
  self = [super initWithFrame:frame];
  if (self) {
    _isLensOverlay = isLensOverlay;
    _textField = [[OmniboxTextFieldIOS alloc] initWithFrame:frame
                                                  textColor:textColor
                                                  tintColor:textFieldTint
                                              isLensOverlay:isLensOverlay];
    _textField.translatesAutoresizingMaskIntoConstraints = NO;

    // Leading image view.
    _leadingImageView = [[UIImageView alloc] init];
    _leadingImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _leadingImageView.contentMode = UIViewContentModeCenter;
    _leadingImageView.tintColor = iconTint;
    [NSLayoutConstraint activateConstraints:@[
      [_leadingImageView.widthAnchor
          constraintEqualToConstant:kOmniboxLeadingImageSize],
      [_leadingImageView.heightAnchor
          constraintEqualToConstant:kOmniboxLeadingImageSize],
    ]];

    // Stack view.
    _stackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[ _leadingImageView ]];
    _stackView.translatesAutoresizingMaskIntoConstraints = NO;
    _stackView.axis = UILayoutConstraintAxisHorizontal;
    _stackView.alignment = UIStackViewAlignmentCenter;
    _stackView.spacing = 0;
    _stackView.distribution = UIStackViewDistributionFill;
    [self addSubview:_stackView];
    AddSameConstraintsWithInsets(
        _stackView, self,
        NSDirectionalEdgeInsetsMake(0,
                                    _isLensOverlay
                                        ? kLeadingImageLeadingMarginLensOverlay
                                        : kOmniboxLeadingImageViewEdgeOffset,
                                    0, kTextFieldClearButtonTrailingOffset));

    // Thumbnail image view.
    if (base::FeatureList::IsEnabled(kEnableLensOverlay)) {
      // Button to delete the thumbnail.
      _thumbnailButton = [[OmniboxThumbnailButton alloc] init];
      _thumbnailButton.translatesAutoresizingMaskIntoConstraints = NO;
      _thumbnailButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_OMNIBOX_REMOVE_THUMBNAIL_LABEL);
      _thumbnailButton.accessibilityHint =
          l10n_util::GetNSString(IDS_IOS_OMNIBOX_REMOVE_THUMBNAIL_HINT);
      _thumbnailButton.hidden = YES;
      [NSLayoutConstraint activateConstraints:@[
        [_thumbnailButton.widthAnchor
            constraintEqualToConstant:kThumbnailWidth],
        [_thumbnailButton.heightAnchor
            constraintEqualToConstant:kThumbnailHeight],
      ]];
      [_stackView addArrangedSubview:_thumbnailButton];
      // Spacing between thumbnail and text field.
      [_stackView setCustomSpacing:kThumbnailImageTrailingMargin
                         afterView:_thumbnailButton];
    }

    // Textfield.
    [_stackView addArrangedSubview:_textField];

    // Clear button.
    UIButtonConfiguration* conf =
        [UIButtonConfiguration plainButtonConfiguration];
    conf.image = DefaultSymbolWithPointSize(kXMarkCircleFillSymbol,
                                            kClearButtonImageSize);
    conf.contentInsets =
        NSDirectionalEdgeInsetsMake(kClearButtonInset, kClearButtonInset,
                                    kClearButtonInset, kClearButtonInset);
    _clearButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _clearButton.configuration = conf;
    _clearButton.tintColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
    SetA11yLabelAndUiAutomationName(_clearButton, IDS_IOS_ACCNAME_CLEAR_TEXT,
                                    @"Clear Text");
    _clearButton.pointerInteractionEnabled = YES;
    _clearButton.pointerStyleProvider =
        CreateLiftEffectCirclePointerStyleProvider();
    // Do not use the system clear button. Use a custom view instead.
    _textField.clearButtonMode = UITextFieldViewModeNever;
    // Note that `rightView` is an incorrect name, it's really a trailing
    // view.
    _textField.rightViewMode = UITextFieldViewModeAlways;
    _textField.rightView = _clearButton;

    // Spacing between image and text field.
    [_stackView
        setCustomSpacing:_isLensOverlay ? kLeadingImageTrailingMarginLensOverlay
                                        : kOmniboxTextFieldLeadingOffsetImage
               afterView:_leadingImageView];

    // Constraints.
    AddSameConstraintsToSides(_textField, _stackView,
                              LayoutSides::kTop | LayoutSides::kBottom);
    [_textField
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
  }
  return self;
}

- (void)setLeadingImage:(UIImage*)image
    withAccessibilityIdentifier:(NSString*)accessibilityIdentifier {
  _leadingImageView.image = image;
  _leadingImageView.accessibilityIdentifier = accessibilityIdentifier;
}

- (void)setLeadingImageScale:(CGFloat)scaleValue {
  _leadingImageView.transform =
      CGAffineTransformMakeScale(scaleValue, scaleValue);
}

- (UIButton*)thumbnailButton {
  return _thumbnailButton;
}

- (UIImage*)thumbnailImage {
  return _thumbnailButton.thumbnailImage;
}

- (void)setThumbnailImage:(UIImage*)image {
  [_thumbnailButton setThumbnailImage:image];
  [_thumbnailButton setHidden:!image];
  if (image) {
    [_stackView setCustomSpacing:kThumbnailImageLeadingMargin
                       afterView:_leadingImageView];
  } else {
    [_stackView
        setCustomSpacing:_isLensOverlay ? kLeadingImageTrailingMarginLensOverlay
                                        : kOmniboxTextFieldLeadingOffsetImage
               afterView:_leadingImageView];
  }
}

- (void)setLayoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter {
  _layoutGuideCenter = layoutGuideCenter;
  [_layoutGuideCenter referenceView:_leadingImageView
                          underName:kOmniboxLeadingImageGuide];
  [_layoutGuideCenter referenceView:_textField
                          underName:kOmniboxTextFieldGuide];
}

- (void)setClearButtonHidden:(BOOL)isHidden {
  self.textField.rightViewMode =
      isHidden ? UITextFieldViewModeNever : UITextFieldViewModeAlways;
}

- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  [super setSemanticContentAttribute:semanticContentAttribute];
  _stackView.semanticContentAttribute = semanticContentAttribute;
}

#pragma mark - TextFieldViewContaining

- (UIView*)textFieldView {
  return self.textField;
}

@end
