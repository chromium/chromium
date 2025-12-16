// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/omnibox_container_view.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/omnibox/model/omnibox_metrics_recorder.h"
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

/// Size of the leading image when presented in composebox.
const CGFloat kLeadingImageSizeAIM = 22;
// The leading image margins when presented in composebox.
const CGFloat kLeadingImageLeadingMarginAIM = 7;
const CGFloat kLeadingImageTrailingMarginAIM = 11;

/// Space between the clear button and the edge of the omnibox.
const CGFloat kTextInputViewClearButtonTrailingOffset = 4;

/// Clear button inset on all sides.
const CGFloat kClearButtonInset = 4.0f;
/// Clear button image size.
const CGFloat kClearButtonImageSize = 17.0f;
const CGFloat kComposeboxClearButtonImageSize = 15.0f;
const CGFloat kClearButtonSize = 28.0f;

/// Whether the omnibox is using the text view instead of the text field.
bool UseTextView(OmniboxPresentationContext presentation_context) {
  if (presentation_context == OmniboxPresentationContext::kLocationBar) {
    return IsMultilineBrowserOmniboxEnabled();
  } else if (presentation_context == OmniboxPresentationContext::kComposebox) {
    return YES;
  }
  return NO;
}

/// The maxium number of lines for the multiline omnibox before it starts
/// scrolling in the presentation context.
int MaxNumberOfLines(OmniboxPresentationContext presentation_context) {
  if (presentation_context == OmniboxPresentationContext::kComposebox) {
    return 5;
  }
  // Lower as the other presentation context don't cap the height of the text
  // view.
  return 3;
}

/// Creates and configures the leading image view.
UIImageView* CreateLeadingImageView(
    UIColor* icon_tint,
    OmniboxPresentationContext presentation_context) {
  UIImageView* leading_image_view = [[UIImageView alloc] init];
  leading_image_view.translatesAutoresizingMaskIntoConstraints = NO;
  leading_image_view.tintColor = icon_tint;
  if (presentation_context == OmniboxPresentationContext::kComposebox) {
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
UIButton* CreateClearButton(OmniboxPresentationContext presentationContext) {
  UIButtonConfiguration* conf =
      [UIButtonConfiguration plainButtonConfiguration];
  CGFloat imageSize =
      presentationContext == OmniboxPresentationContext::kComposebox
          ? kComposeboxClearButtonImageSize
          : kClearButtonImageSize;
  conf.image = DefaultSymbolWithPointSize(kXMarkCircleFillSymbol, imageSize);
  conf.contentInsets =
      NSDirectionalEdgeInsetsMake(kClearButtonInset, kClearButtonInset,
                                  kClearButtonInset, kClearButtonInset);
  UIButton* clear_button = [UIButton buttonWithType:UIButtonTypeSystem];
  clear_button.translatesAutoresizingMaskIntoConstraints = NO;
  clear_button.configuration = conf;
  if (presentationContext == OmniboxPresentationContext::kComposebox) {
    clear_button.tintColor = [UIColor colorNamed:kTextTertiaryColor];
  } else {
    clear_button.tintColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
  }

  SetA11yLabelAndUiAutomationName(clear_button, IDS_IOS_ACCNAME_CLEAR_TEXT,
                                  kOmniboxClearButtonAccessibilityIdentifier);
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

  // The constraint for the textfield's leading anchor.
  NSLayoutConstraint* _textInputViewLeadingConstraint;
  // The constraint for the textfield's leading anchor when the thumbnail is
  // hidden.
  NSLayoutConstraint* _textInputViewLeadingToIconConstraint;
  // Text input height constraint.
  NSLayoutConstraint* _textInputHeightConstraint;
  // The last known width of the text view, used to avoid redundant height
  // calculations.
  CGFloat _lastKnownTextViewWidth;
  // The last computed ideal height of the text view, before being constrained
  // by the container's bounds.
  CGFloat _lastComputedIdealHeight;
  // The last computed intrinsic height, used by the `intrinsicContentSize`
  // property.
  CGFloat _currentIntrinsicHeight;
  // Constraint determining whether the text input is constraint to the close
  // button.
  NSLayoutConstraint* _textInputToCloseButton;
}

@synthesize heightDelegate = _heightDelegate;
@synthesize leadingImageHidden = _leadingImageHidden;

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
    self.clearButton = CreateClearButton(presentationContext);
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
               OmniboxPresentationContext::kComposebox) {
      leadingImageLeadingOffset = kLeadingImageLeadingMarginAIM;
    }

    _textInputToCloseButton = [_textInputView.trailingAnchor
        constraintEqualToAnchor:self.clearButton.leadingAnchor];
    _textInputToCloseButton.active = NO;

    NSLayoutConstraint* textInputToContainerTrailing =
        [_textInputView.trailingAnchor
            constraintEqualToAnchor:self.trailingAnchor];
    textInputToContainerTrailing.priority = UILayoutPriorityRequired - 1;

    CGFloat clearTrailingOffset =
        _presentationContext == OmniboxPresentationContext::kComposebox
            ? 0
            : kTextInputViewClearButtonTrailingOffset;

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
                         constant:-clearTrailingOffset],
      textInputToContainerTrailing
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
    }

    [self updateLeadingConstraint];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  if (_textView.bounds.size.width != _lastKnownTextViewWidth) {
    _lastKnownTextViewWidth = _textView.bounds.size.width;
    [self updateTextViewHeight];
  }
  [self updateTextViewLayout];
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
  [self updateLeadingConstraint];
}

- (void)setLayoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter {
  _layoutGuideCenter = layoutGuideCenter;
  [_layoutGuideCenter referenceView:_leadingImageView
                          underName:kOmniboxLeadingImageGuide];
  [_layoutGuideCenter referenceView:_textInputView
                          underName:kOmniboxTextFieldGuide];
}

- (void)setLeadingImageHidden:(BOOL)leadingImageHidden {
  _leadingImageHidden = leadingImageHidden;
  _leadingImageView.hidden = leadingImageHidden;
  [self updateLeadingConstraint];
}

- (void)setLeadingImageAlpha:(BOOL)alpha {
  _leadingImageView.alpha = alpha;
}

- (void)forceDisableReturnKey:(BOOL)forceDisable {
  [_textInputView forceDisableReturnKey:forceDisable];
}

- (void)setAllowsReturnKeyWithEmptyText:(BOOL)allowsReturnKeyWithEmptyText {
  _textInputView.allowsReturnKeyWithEmptyText = allowsReturnKeyWithEmptyText;
}

- (void)setCustomPlaceholderText:(NSString*)customPlaceholderText {
  [_textInputView setCustomPlaceholderText:[customPlaceholderText copy]];
}

- (void)updateLeadingConstraint {
  _textInputViewLeadingConstraint.active = NO;

  BOOL thumbnailVisible = !_thumbnailButton.hidden &&
                          base::FeatureList::IsEnabled(kEnableLensOverlay);
  if (self.leadingImageHidden) {
    _textInputViewLeadingConstraint = [_textInputView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor];
  } else if (thumbnailVisible) {
    // The textInputView can be anchored to the thumbnail (if visible) or the
    // leading icon (if thumbnail is hidden).
    _textInputViewLeadingConstraint = [_textInputView.leadingAnchor
        constraintEqualToAnchor:_thumbnailButton.trailingAnchor
                       constant:kThumbnailImageTrailingMargin];
  } else {
    CGFloat textInputViewLeadingOffset = kOmniboxTextFieldLeadingOffsetImage;
    if (_presentationContext == OmniboxPresentationContext::kLensOverlay) {
      textInputViewLeadingOffset = kLeadingImageTrailingMarginLensOverlay;
    } else if (_presentationContext ==
               OmniboxPresentationContext::kComposebox) {
      textInputViewLeadingOffset = kLeadingImageTrailingMarginAIM;
    }

    // By default, the thumbnail is hidden, so the text field is anchored to the
    // leading icon.
    _textInputViewLeadingConstraint = [_textInputView.leadingAnchor
        constraintEqualToAnchor:_leadingImageView.trailingAnchor
                       constant:textInputViewLeadingOffset];
  }

  _textInputViewLeadingConstraint.active = YES;
}

- (void)setClearButtonHidden:(BOOL)isHidden {
  self.clearButton.hidden = isHidden;
  _textInputToCloseButton.active = !isHidden;
}

- (id<OmniboxTextInput>)textInput {
  return _textInputView;
}

#pragma mark - Private

/// Updates the text view intrinsic content height and clip autocomplete text.
- (void)updateTextViewHeight {
  if (!_textView) {
    return;
  }

  // Computes user text height.
  CGFloat verticalPadding =
      _textView.textContainerInset.top + _textView.textContainerInset.bottom;
  CGFloat singleLineHeight = [_textView singleLineHeight];

  // Calculate the height of the user text.
  NSAttributedString* userText = _textView.attributedUserText;
  if (_textView.isPreEditing) {
    userText = [[NSAttributedString alloc] initWithString:@""];
  }

  // Calculate the precise drawing width.
  UIEdgeInsets textContainerInsets = _textView.textContainerInset;
  CGFloat lineFragmentPadding = _textView.textContainer.lineFragmentPadding;
  CGFloat drawingWidth = _textView.bounds.size.width -
                         textContainerInsets.left - textContainerInsets.right -
                         lineFragmentPadding * 2.0;
  if (drawingWidth < 0) {
    drawingWidth = 0;
  }
  CGFloat userTextHeight = 0;
  if (userText.length > 0) {
    userTextHeight = [self heightForAttributedText:userText
                                  withDrawingWidth:drawingWidth];
  }
  if (!userTextHeight) {
    userTextHeight = singleLineHeight;
  }

  // Records the number of lines and update it in the text view.
  NSInteger numberOfLines = round(userTextHeight / singleLineHeight);
  [self.metricsRecorder setNumberOfLines:numberOfLines];
  _textView.textContainer.maximumNumberOfLines = numberOfLines;
  NSLineBreakMode defaultLineBreakMode =
      [self lineBreakModeForUserText:userText];
  [self updateLastLineClipping:defaultLineBreakMode];

  // Limit the number of lines and update intrinsic size.
  _lastComputedIdealHeight = ceilf(userTextHeight + verticalPadding);
  CGFloat maxHeightFromLines =
      (singleLineHeight * MaxNumberOfLines(_presentationContext)) +
      verticalPadding;
  CGFloat intrinsicHeight = MIN(_lastComputedIdealHeight, maxHeightFromLines);

  if (!_textInputHeightConstraint) {
    _textInputHeightConstraint =
        [_textView.heightAnchor constraintEqualToConstant:intrinsicHeight];
    // Lower priority as the height is capped by the available height set by the
    // embedder.
    _textInputHeightConstraint.priority = UILayoutPriorityDefaultHigh + 1;
    _textInputHeightConstraint.active = YES;
  } else {
    _textInputHeightConstraint.constant = intrinsicHeight;
  }

  if (_currentIntrinsicHeight != intrinsicHeight) {
    _currentIntrinsicHeight = intrinsicHeight;
    [self.heightDelegate textFieldViewContaining:self
                                 didChangeHeight:intrinsicHeight];
  }
}

/// Updates the text view layout.
- (void)updateTextViewLayout {
  if (!_textView) {
    return;
  }

  _textView.scrollEnabled = _lastComputedIdealHeight > self.bounds.size.height;
}

/// Updates the paragraph style to clip the last line.
- (void)updateLastLineClipping:(NSLineBreakMode)defaultLineBreakMode {
  NSTextStorage* textStorage = _textView.textStorage;
  NSRange fullRange = NSMakeRange(0, textStorage.length);

  [self applyLineBreakMode:defaultLineBreakMode
      toMutableAttributedString:textStorage
                        inRange:fullRange];

  NSLayoutManager* layoutManager = _textView.layoutManager;
  NSUInteger numberOfGlyphs = [layoutManager numberOfGlyphs];
  NSUInteger maxLines = _textView.textContainer.maximumNumberOfLines;

  if (numberOfGlyphs == 0 || maxLines == 0) {
    return;
  }

  // Determine the actual number of lines.
  NSUInteger lineCount = 0;
  NSRange lineRange;
  for (NSUInteger glyphIndex = 0; glyphIndex < numberOfGlyphs; lineCount++) {
    [layoutManager lineFragmentRectForGlyphAtIndex:glyphIndex
                                    effectiveRange:&lineRange];
    glyphIndex = NSMaxRange(lineRange);
  }

  if (lineCount >= maxLines) {
    // Find the glyph index at the start of the line to be clipped.
    NSUInteger clipStartGlyphIndex = 0;
    for (NSUInteger i = 0; i < maxLines - 1; i++) {
      if (clipStartGlyphIndex >= numberOfGlyphs) {
        break;
      }
      [layoutManager lineFragmentRectForGlyphAtIndex:clipStartGlyphIndex
                                      effectiveRange:&lineRange];
      clipStartGlyphIndex = NSMaxRange(lineRange);
    }

    // Convert the glyph index to a character index.
    NSUInteger clipStartCharIndex =
        [layoutManager characterIndexForGlyphAtIndex:clipStartGlyphIndex];

    // Apply clipping to the last line and beyond.
    if (clipStartCharIndex < textStorage.length) {
      NSRange clipRange = NSMakeRange(clipStartCharIndex,
                                      textStorage.length - clipStartCharIndex);
      [self applyLineBreakMode:NSLineBreakByClipping
          toMutableAttributedString:textStorage
                            inRange:clipRange];
    }
  }
}

- (NSUInteger)numberOfLines {
  if (_textInputView != _textView) {
    return 1;
  }

  return _textView.textContainer.maximumNumberOfLines;
}

#pragma mark - TextFieldViewContaining

- (void)setMinimumHeight:(CGFloat)minimumHeight {
  if (UseTextView(_presentationContext)) {
    _textView.minimumHeight = minimumHeight;
  }
}

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

#pragma mark - Private

/// Computes the height needed to layout `attributedText` with `drawingWidth`.
/// The height is computed with `lineBreakModeForUserText`.
- (CGFloat)heightForAttributedText:(NSAttributedString*)attributedText
                  withDrawingWidth:(CGFloat)drawingWidth {
  if (attributedText.length == 0) {
    return 0;
  }
  NSMutableAttributedString* mutableString = [[NSMutableAttributedString alloc]
      initWithAttributedString:attributedText];
  NSLineBreakMode lineBreakMode =
      [self lineBreakModeForUserText:attributedText];
  NSRange fullRange = NSMakeRange(0, mutableString.length);
  [self applyLineBreakMode:lineBreakMode
      toMutableAttributedString:mutableString
                        inRange:fullRange];
  CGSize constraintSize = CGSizeMake(drawingWidth, CGFLOAT_MAX);
  NSStringDrawingOptions options =
      NSStringDrawingUsesLineFragmentOrigin | NSStringDrawingUsesFontLeading;
  CGRect boundingRect = [mutableString boundingRectWithSize:constraintSize
                                                    options:options
                                                    context:nil];
  return ceilf(boundingRect.size.height);
}

/// Returns the line break mode to apply for the text.
- (NSLineBreakMode)lineBreakModeForUserText:(NSAttributedString*)text {
  BOOL containsWhitespace =
      [text.string
          rangeOfCharacterFromSet:[NSCharacterSet
                                      whitespaceAndNewlineCharacterSet]]
          .location != NSNotFound;
  NSLineBreakMode defaultLineBreakMode = containsWhitespace
                                             ? NSLineBreakByWordWrapping
                                             : NSLineBreakByCharWrapping;
  return defaultLineBreakMode;
}

/// Applies a line break mode to an attributed string within a specific range,
/// preserving all other paragraph style properties.
///
/// @param mutableAttributedString The mutable attributed string to modify.
/// @param lineBreakMode The new NSLineBreakMode to apply.
/// @param range The range within the attributed string to apply the line break
/// mode.
- (void)applyLineBreakMode:(NSLineBreakMode)lineBreakMode
    toMutableAttributedString:
        (NSMutableAttributedString*)mutableAttributedString
                      inRange:(NSRange)range {
  [mutableAttributedString
      enumerateAttribute:NSParagraphStyleAttributeName
                 inRange:range
                 options:0
              usingBlock:^(id value, NSRange currentRange, BOOL* stop) {
                NSParagraphStyle* existingStyle = (NSParagraphStyle*)value;

                // If a paragraph style exists, copy it. Otherwise, create a new
                // default one.
                NSMutableParagraphStyle* newParagraphStyle =
                    existingStyle ? [existingStyle mutableCopy]
                                  : [[NSMutableParagraphStyle alloc] init];

                // Set the desired line break mode.
                newParagraphStyle.lineBreakMode = lineBreakMode;

                // Re-apply the modified style to the original range.
                [mutableAttributedString
                    addAttribute:NSParagraphStyleAttributeName
                           value:newParagraphStyle
                           range:currentRange];
              }];
}

@end
