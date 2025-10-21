// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/omnibox_container_view.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/omnibox/public/omnibox_constants.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_ios.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_view_ios.h"
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

/// Size of the leading image when presented in AIM Prototype.
const CGFloat kLeadingImageSizeAIM = 22;
// The leading image margins when presented in AIM Prototype.
const CGFloat kLeadingImageLeadingMarginAIM = 7;
const CGFloat kLeadingImageTrailingMarginAIM = 11;

/// Space between the clear button and the edge of the omnibox.
const CGFloat kTextInputViewClearButtonTrailingOffset = 4;
/// The maximum number of lines for the text view before it starts scrolling.
const int kMaxLines = 3;

/// Clear button inset on all sides.
const CGFloat kClearButtonInset = 4.0f;
/// Clear button image size.
const CGFloat kClearButtonImageSize = 17.0f;
const CGFloat kClearButtonSize = 28.0f;

/// Whether the omnibox is using the text view instead of the text field.
bool UseTextView(OmniboxPresentationContext presentation_context) {
  if (presentation_context == OmniboxPresentationContext::kLocationBar) {
    return IsMultilineBrowserOmniboxEnabled();
  } else if (presentation_context ==
             OmniboxPresentationContext::kAIMPrototype) {
    return base::FeatureList::IsEnabled(kIOSOmniboxUseTextView);
  }
  return NO;
}

/// Creates and configures the leading image view.
UIImageView* CreateLeadingImageView(
    UIColor* icon_tint,
    OmniboxPresentationContext presentation_context) {
  UIImageView* leading_image_view = [[UIImageView alloc] init];
  leading_image_view.translatesAutoresizingMaskIntoConstraints = NO;
  leading_image_view.tintColor = icon_tint;
  if (presentation_context == OmniboxPresentationContext::kAIMPrototype) {
    leading_image_view.contentMode = UIViewContentModeScaleAspectFit;
    [NSLayoutConstraint activateConstraints:@[
      [leading_image_view.widthAnchor
          constraintEqualToConstant:kLeadingImageSizeAIM],
      [leading_image_view.heightAnchor
          constraintEqualToConstant:kLeadingImageSizeAIM],
    ]];
  } else {
    leading_image_view.contentMode = UIViewContentModeCenter;
    [NSLayoutConstraint activateConstraints:@[
      [leading_image_view.widthAnchor
          constraintEqualToConstant:kOmniboxLeadingImageSize],
      [leading_image_view.heightAnchor
          constraintEqualToConstant:kOmniboxLeadingImageSize],
    ]];
  }

  return leading_image_view;
}

/// Creates and configures the thumbnail button.
OmniboxThumbnailButton* CreateThumbnailButton() {
  OmniboxThumbnailButton* thumbnail_button =
      [[OmniboxThumbnailButton alloc] init];
  thumbnail_button.translatesAutoresizingMaskIntoConstraints = NO;
  thumbnail_button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_REMOVE_THUMBNAIL_LABEL);
  thumbnail_button.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_REMOVE_THUMBNAIL_HINT);
  thumbnail_button.hidden = YES;
  [NSLayoutConstraint activateConstraints:@[
    [thumbnail_button.widthAnchor constraintEqualToConstant:kThumbnailWidth],
    [thumbnail_button.heightAnchor constraintEqualToConstant:kThumbnailHeight],
  ]];
  return thumbnail_button;
}

/// Creates and configures the clear button.
UIButton* CreateClearButton() {
  UIButtonConfiguration* conf =
      [UIButtonConfiguration plainButtonConfiguration];
  conf.image =
      DefaultSymbolWithPointSize(kXMarkCircleFillSymbol, kClearButtonImageSize);
  conf.contentInsets =
      NSDirectionalEdgeInsetsMake(kClearButtonInset, kClearButtonInset,
                                  kClearButtonInset, kClearButtonInset);
  UIButton* clear_button = [UIButton buttonWithType:UIButtonTypeSystem];
  clear_button.translatesAutoresizingMaskIntoConstraints = NO;
  clear_button.configuration = conf;
  clear_button.tintColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
  SetA11yLabelAndUiAutomationName(clear_button, IDS_IOS_ACCNAME_CLEAR_TEXT,
                                  @"Clear Text");
  clear_button.pointerInteractionEnabled = YES;
  clear_button.pointerStyleProvider =
      CreateLiftEffectCirclePointerStyleProvider();
  [NSLayoutConstraint activateConstraints:@[
    [clear_button.widthAnchor constraintEqualToConstant:kClearButtonSize],
    [clear_button.heightAnchor constraintEqualToConstant:kClearButtonSize],
  ]];
  return clear_button;
}

}  // namespace

#pragma mark - OmniboxContainerView

@interface OmniboxContainerView () <OmniboxTextViewHeightDelegate>

// Redefined as readwrite.
@property(nonatomic, strong) UIButton* clearButton;

@end

@implementation OmniboxContainerView {
  /// The leading image view. Used for autocomplete icons.
  UIImageView* _leadingImageView;
  // The image thumnail button.
  OmniboxThumbnailButton* _thumbnailButton;
  // The text input view.
  UIView<OmniboxTextInput>* _textInputView;
  // Stores the text view for height adjustment.
  OmniboxTextViewIOS* _textView;

  /// The context in which the omnibox is presented.
  OmniboxPresentationContext _presentationContext;

  // The constraint for the textfield's leading anchor when the thumbnail is
  // visible.
  NSLayoutConstraint* _textInputViewLeadingToThumbnailConstraint;
  // The constraint for the textfield's leading anchor when the thumbnail is
  // hidden.
  NSLayoutConstraint* _textInputViewLeadingToIconConstraint;
  // Text input height constraint.
  NSLayoutConstraint* _textInputHeightConstraint;
}

@synthesize heightDelegate = _heightDelegate;

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                textInputTint:(UIColor*)textInputTint
                     iconTint:(UIColor*)iconTint
          presentationContext:(OmniboxPresentationContext)presentationContext {
  self = [super initWithFrame:frame];
  if (self) {
    _presentationContext = presentationContext;
    _leadingImageView = CreateLeadingImageView(iconTint, presentationContext);
    self.clearButton = CreateClearButton();
    [self createAndAddTextInputViewWithTextColor:textColor
                                   textInputTint:textInputTint];

    [self addSubview:_leadingImageView];
    [self addSubview:self.clearButton];

    // Constraints.
    [_textInputView
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_textInputView setContentHuggingPriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];

    NSLayoutAnchor* referenceCenterYAnchor =
        _textInputView.viewForVerticalAlignment.centerYAnchor;

    CGFloat leadingImageLeadingOffset = kOmniboxLeadingImageViewEdgeOffset;
    if (_presentationContext == OmniboxPresentationContext::kLensOverlay) {
      leadingImageLeadingOffset = kLeadingImageLeadingMarginLensOverlay;
    } else if (_presentationContext ==
               OmniboxPresentationContext::kAIMPrototype) {
      leadingImageLeadingOffset = kLeadingImageLeadingMarginAIM;
    }

    [NSLayoutConstraint activateConstraints:@[
      [_leadingImageView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:leadingImageLeadingOffset],
      [_leadingImageView.centerYAnchor
          constraintEqualToAnchor:referenceCenterYAnchor],
      [_textInputView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_textInputView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [self.clearButton.centerYAnchor
          constraintEqualToAnchor:referenceCenterYAnchor],
      [self.clearButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kTextInputViewClearButtonTrailingOffset],
      [_textInputView.trailingAnchor
          constraintEqualToAnchor:self.clearButton.leadingAnchor],
    ]];

    // Thumbnail image view.
    if (base::FeatureList::IsEnabled(kEnableLensOverlay)) {
      _thumbnailButton = CreateThumbnailButton();
      [self addSubview:_thumbnailButton];
      [NSLayoutConstraint activateConstraints:@[
        [_thumbnailButton.leadingAnchor
            constraintEqualToAnchor:_leadingImageView.trailingAnchor
                           constant:kThumbnailImageLeadingMargin],
        [_thumbnailButton.centerYAnchor
            constraintEqualToAnchor:referenceCenterYAnchor]
      ]];

      // The textInputView can be anchored to the thumbnail (if visible) or the
      // leading icon (if thumbnail is hidden).
      _textInputViewLeadingToThumbnailConstraint = [_textInputView.leadingAnchor
          constraintEqualToAnchor:_thumbnailButton.trailingAnchor
                         constant:kThumbnailImageTrailingMargin];
    }

    CGFloat textInputViewLeadingOffset = kOmniboxTextFieldLeadingOffsetImage;
    if (_presentationContext == OmniboxPresentationContext::kLensOverlay) {
      textInputViewLeadingOffset = kLeadingImageTrailingMarginLensOverlay;
    } else if (_presentationContext ==
               OmniboxPresentationContext::kAIMPrototype) {
      textInputViewLeadingOffset = kLeadingImageTrailingMarginAIM;
    }

    _textInputViewLeadingToIconConstraint = [_textInputView.leadingAnchor
        constraintEqualToAnchor:_leadingImageView.trailingAnchor
                       constant:textInputViewLeadingOffset];
    // By default, the thumbnail is hidden, so the text field is anchored to the
    // leading icon.
    _textInputViewLeadingToIconConstraint.active = YES;
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateTextViewHeight];
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

  BOOL thumbnailVisible = ![_thumbnailButton isHidden];
  if (_textInputViewLeadingToThumbnailConstraint) {
    _textInputViewLeadingToThumbnailConstraint.active = thumbnailVisible;
  }
  _textInputViewLeadingToIconConstraint.active = !thumbnailVisible;
}

- (void)setLayoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter {
  _layoutGuideCenter = layoutGuideCenter;
  [_layoutGuideCenter referenceView:_leadingImageView
                          underName:kOmniboxLeadingImageGuide];
  [_layoutGuideCenter referenceView:_textInputView
                          underName:kOmniboxTextFieldGuide];
}

- (void)setClearButtonHidden:(BOOL)isHidden {
  self.clearButton.hidden = isHidden;
}

- (id<OmniboxTextInput>)textInput {
  return _textInputView;
}

- (void)updateTextViewHeight {
  if (!_textView) {
    return;
  }
  // Recalculate textView height and update it to clip and scroll if necessary.
  CGFloat verticalPadding =
      _textView.textContainerInset.top + _textView.textContainerInset.bottom;
  CGFloat maxHeight = (_textView.font.lineHeight * kMaxLines) + verticalPadding;
  CGSize size = [_textView
      sizeThatFits:CGSizeMake(_textView.frame.size.width, CGFLOAT_MAX)];
  CGFloat newHeight = MIN(size.height, maxHeight);
  if (!_textInputHeightConstraint) {
    _textInputHeightConstraint =
        [_textView.heightAnchor constraintEqualToConstant:newHeight];
    _textInputHeightConstraint.active = YES;
  } else {
    _textInputHeightConstraint.constant = newHeight;
  }
  _textView.scrollEnabled = size.height > maxHeight;
  [self.heightDelegate textFieldViewContaining:self didChangeHeight:newHeight];
}

#pragma mark - TextFieldViewContaining

- (UIView*)textFieldView {
  return _textInputView;
}

#pragma mark - Private

/// Creates the text input view and adds it to the view hierarchy.
- (void)createAndAddTextInputViewWithTextColor:(UIColor*)textColor
                                 textInputTint:(UIColor*)textInputTint {
  if (UseTextView(_presentationContext)) {
    OmniboxTextViewIOS* textView =
        [[OmniboxTextViewIOS alloc] initWithFrame:CGRectZero
                                        textColor:textColor
                                        tintColor:textInputTint
                              presentationContext:_presentationContext];
    textView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:textView];
    // The placeholder must be added as a sibling to the textview. Constraints
    // are handled internally in the text view.
    UILabel* placeholderLabel = [[UILabel alloc] init];
    placeholderLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:placeholderLabel];
    textView.placeholderLabel = placeholderLabel;
    textView.heightDelegate = self;
    _textView = textView;
    _textInputView = textView;
    [self updateTextViewHeight];
  } else {
    OmniboxTextFieldIOS* textField =
        [[OmniboxTextFieldIOS alloc] initWithFrame:CGRectZero
                                         textColor:textColor
                                         tintColor:textInputTint
                               presentationContext:_presentationContext];
    // Do not use the system clear button. Use a custom view instead.
    textField.clearButtonMode = UITextFieldViewModeNever;
    textField.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:textField];
    _textInputView = textField;
  }
}

#pragma mark - OmniboxTextViewHeightDelegate

- (void)textViewContentChanged:(OmniboxTextViewIOS*)textView {
  [self updateTextViewHeight];
}

@end
