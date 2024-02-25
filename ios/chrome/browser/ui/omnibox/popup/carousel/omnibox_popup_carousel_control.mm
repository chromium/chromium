// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/carousel/omnibox_popup_carousel_control.h"

#import "ios/chrome/browser/ui/omnibox/popup/carousel/carousel_item.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel/carousel_item_menu_provider.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel/omnibox_popup_carousel_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

/// Size of the view behind the icon.
const CGFloat kBackgroundViewSize = 56.0f;
/// Top, leading and trailing margins of `ImageView`.
const CGFloat kBackgroundViewMargin = 7.0f;
/// Padding between the icon and the label.
const CGFloat kBackgroundLabelSpacing = 10.0f;
/// Size of the view containing the favicon.
const CGFloat kFaviconSize = 32.0f;
/// Size of the favicon placeholder font.
const CGFloat kFaviconPlaceholderFontSize = 22.0f;
/// Leading and trailing margin of the label.
const CGFloat kLabelMargin = 1.0f;
/// Maximum number of lines for the label.
const NSInteger kLabelNumLines = 2;

/// Corner radius of the context menu preview.
const CGFloat kPreviewCornerRadius = 13.0f;
/// Corner radius of the icon's `backgroundView`.
const CGFloat kBackgroundCornerRadius = 13.0f;

/// UILabel displaying text at the bottom of carousel item.
UILabel* CarouselItemLabel() {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.numberOfLines = kLabelNumLines;
  label.textAlignment = NSTextAlignmentCenter;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  label.accessibilityIdentifier =
      kOmniboxCarouselControlLabelAccessibilityIdentifier;
  return label;
}

/// UIView for the squircle background of carousel item.
UIView* CarouselItemBackgroundView() {
  UIImageView* imageView = [[UIImageView alloc] init];
  UIImage* backgroundImage = [[UIImage imageNamed:@"ntp_most_visited_tile"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  imageView.image = backgroundImage;
  imageView.tintColor = [UIColor colorNamed:kGrey100Color];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [imageView.widthAnchor constraintEqualToConstant:kBackgroundViewSize],
    [imageView.heightAnchor constraintEqualToConstant:kBackgroundViewSize]
  ]];
  return imageView;
}

/// FaviconView containing the icon of carousel item.
FaviconView* CarouselItemFaviconView() {
  FaviconView* faviconView = [[FaviconView alloc] init];
  [faviconView setFont:[UIFont systemFontOfSize:kFaviconPlaceholderFontSize]];
  faviconView.userInteractionEnabled = NO;
  faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [faviconView.widthAnchor constraintEqualToConstant:kFaviconSize],
    [faviconView.heightAnchor constraintEqualToConstant:kFaviconSize]
  ]];
  return faviconView;
}

/// CAGradientLayer for `backgroundView` when selected.
CAGradientLayer* CarouselBackgroundGradientLayer() {
  CAGradientLayer* gradientLayer = [[CAGradientLayer alloc] init];
  gradientLayer.startPoint = CGPointMake(0, 0.5);
  gradientLayer.endPoint = CGPointMake(1, 0.5);
  UIColor* gradientStartColor =
      [[UIColor colorNamed:@"omnibox_suggestion_row_highlight_color"]
          colorWithAlphaComponent:0.85];
  UIColor* gradientEndColor =
      [UIColor colorNamed:@"omnibox_suggestion_row_highlight_color"];
  gradientLayer.colors =
      @[ (id)[gradientStartColor CGColor], (id)[gradientEndColor CGColor] ];
  gradientLayer.cornerRadius = kBackgroundCornerRadius;
  gradientLayer.cornerCurve = kCACornerCurveContinuous;
  return gradientLayer;
}

}  // namespace

const CGFloat kOmniboxPopupCarouselControlWidth =
    kBackgroundViewSize + 2 * kBackgroundViewMargin;

@interface OmniboxPopupCarouselControl ()

/// View containing the background.
@property(nonatomic, strong) UIView* backgroundView;
/// View containing the icon.
@property(nonatomic, strong) FaviconView* faviconView;
/// UILabel containing the text.
@property(nonatomic, strong) UILabel* label;
/// Gradient layer for `backgroundView` when selected.
@property(nonatomic, strong) CAGradientLayer* gradientLayer;

@end

@implementation OmniboxPopupCarouselControl

- (instancetype)init {
  self = [super init];
  if (self) {
    _label = CarouselItemLabel();
    _backgroundView = CarouselItemBackgroundView();
    _faviconView = CarouselItemFaviconView();
    _carouselItem = nil;
    _gradientLayer = CarouselBackgroundGradientLayer();
  }
  return self;
}

- (void)setSelected:(BOOL)selected {
  [super setSelected:selected];
  if (selected) {
    [self.delegate carouselControlDidBecomeFocused:self];
    [self.backgroundView.layer addSublayer:self.gradientLayer];
  } else {
    [self.gradientLayer removeFromSuperlayer];
  }
}

- (void)layoutSubviews {
  self.gradientLayer.frame = self.backgroundView.bounds;
  [super layoutSubviews];
}

- (void)addSubviews {
  // Rounds corners in a Squircle.
  self.layer.cornerCurve = kCACornerCurveContinuous;
  self.layer.cornerRadius = kPreviewCornerRadius;
  [self
      addInteraction:[[UIContextMenuInteraction alloc] initWithDelegate:self]];
  self.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_label];
  [self addSubview:_backgroundView];
  [self addSubview:_faviconView];

  AddSameCenterConstraints(_backgroundView, _faviconView);
  [NSLayoutConstraint activateConstraints:@[
    [_backgroundView.topAnchor constraintEqualToAnchor:self.topAnchor
                                              constant:kBackgroundViewMargin],
    [_backgroundView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kBackgroundViewMargin],
    [self.trailingAnchor constraintEqualToAnchor:_backgroundView.trailingAnchor
                                        constant:kBackgroundViewMargin],

    [_label.topAnchor constraintEqualToAnchor:_backgroundView.bottomAnchor
                                     constant:kBackgroundLabelSpacing],
    [_label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                         constant:kLabelMargin],
    [self.trailingAnchor constraintEqualToAnchor:_label.trailingAnchor
                                        constant:kLabelMargin],
    [self.bottomAnchor constraintEqualToAnchor:_label.bottomAnchor
                                      constant:kBackgroundViewMargin]
  ]];
  [self setNeedsLayout];
  [self layoutIfNeeded];
}

#pragma mark - Public methods

- (void)setCarouselItem:(CarouselItem*)carouselItem {
  if (self.subviews.count == 0) {
    [self addSubviews];
  }
  _carouselItem = carouselItem;
  [self.faviconView configureWithAttributes:carouselItem.faviconAttributes];
  self.label.text = carouselItem.title;
  self.accessibilityLabel = carouselItem.title;
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  return [self.menuProvider
      contextMenuConfigurationForCarouselItem:self.carouselItem
                                     fromView:self];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
    willDisplayMenuForConfiguration:(UIContextMenuConfiguration*)configuration
                           animator:
                               (id<UIContextMenuInteractionAnimating>)animator {
  if (self.isSelected) {
    __weak CAGradientLayer* weakHighlightLayer = self.gradientLayer;
    [animator addAnimations:^{
      [weakHighlightLayer removeFromSuperlayer];
    }];
  }
  [super contextMenuInteraction:interaction
      willDisplayMenuForConfiguration:configuration
                             animator:animator];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
       willEndForConfiguration:(UIContextMenuConfiguration*)configuration
                      animator:(id<UIContextMenuInteractionAnimating>)animator {
  if (self.isSelected) {
    __weak __typeof(self) weakSelf = self;
    [animator addAnimations:^{
      [weakSelf.backgroundView.layer addSublayer:weakSelf.gradientLayer];
    }];
  }
  [super contextMenuInteraction:interaction
        willEndForConfiguration:configuration
                       animator:animator];
}

- (UITargetedPreview*)contextMenuInteraction:
                          (UIContextMenuInteraction*)interaction
                               configuration:
                                   (UIContextMenuConfiguration*)configuration
       highlightPreviewForItemWithIdentifier:(id<NSCopying>)identifier {
  UIPreviewParameters* previewParameters = [[UIPreviewParameters alloc] init];
  previewParameters.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  previewParameters.visiblePath =
      [UIBezierPath bezierPathWithRoundedRect:interaction.view.bounds
                                 cornerRadius:kPreviewCornerRadius];
  return [[UITargetedPreview alloc] initWithView:self
                                      parameters:previewParameters];
}

#pragma mark - UIAccessibility

- (void)accessibilityElementDidBecomeFocused {
  // Element is focused by VoiceOver, informs its delegate so it can make it
  // visible, in case it's hidden in the scroll view.
  [self.delegate carouselControlDidBecomeFocused:self];
}

- (UIAccessibilityTraits)accessibilityTraits {
  return UIAccessibilityTraitButton | [super accessibilityTraits];
}

/// Custom actions for a cell configured with this item.
- (NSArray<UIAccessibilityCustomAction*>*)accessibilityCustomActions {
  return
      [self.menuProvider accessibilityActionsForCarouselItem:self.carouselItem
                                                    fromView:self];
}

@end
