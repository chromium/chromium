// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_carousel_control.h"

#import "ios/chrome/browser/ui/omnibox/popup/carousel_item.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel_item_menu_provider.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_carousel_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Size of the view behind the icon.
const CGFloat kBackgroundViewSize = 56.0f;
// Top, leading and trailing margins of `ImageView`.
const CGFloat kBackgroundViewMargin = 8.0f;
// Padding between the icon and the label.
const CGFloat kBackgroundLabelSpacing = 10.0f;
// Size of the view containing the favicon.
const CGFloat kFaviconSize = 32.0f;
// Size of the favicon placeholder font.
const CGFloat kFaviconPlaceholderFontSize = 22.0f;
// Leading and trailing margin of the label.
const CGFloat kLabelMargin = 1.0f;
// Maximum number of lines for the label.
const NSInteger kLabelNumLines = 2;

// Corner radius of the context menu preview.
const CGFloat kPreviewCornerRadius = 13.0f;

// UILabel displaying text at the bottom of carousel item.
UILabel* CarouselItemLabel() {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  label.numberOfLines = kLabelNumLines;
  label.textAlignment = NSTextAlignmentCenter;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  // TODO(crbug.com/1365374): Check color with UX.
  return label;
}

// UIView for the background of carousel item.
UIView* CarouselItemBackgroundView() {
  UIImageView* imageView = [[UIImageView alloc] init];
  UIImage* backgroundImage = [[UIImage imageNamed:@"ntp_most_visited_tile"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  imageView.image = backgroundImage;
  imageView.tintColor = [UIColor colorNamed:kGrey200Color];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [imageView.widthAnchor constraintEqualToConstant:kBackgroundViewSize],
    [imageView.heightAnchor constraintEqualToConstant:kBackgroundViewSize]
  ]];
  return imageView;
}

// FaviconView containing the icon of carousel item.
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

}  // namespace

@interface OmniboxPopupCarouselControl ()

// View containing the background.
@property(nonatomic, strong) UIView* backgroundView;
// View containing the icon.
@property(nonatomic, strong) FaviconView* faviconView;
// UILabel containing the text.
@property(nonatomic, strong) UILabel* label;
@property(nonatomic, strong) CarouselItem* carouselItem;

@end

@implementation OmniboxPopupCarouselControl

- (instancetype)init {
  self = [super init];
  if (self) {
    _label = CarouselItemLabel();
    _backgroundView = CarouselItemBackgroundView();
    _faviconView = CarouselItemFaviconView();
    _carouselItem = nil;
  }
  return self;
}

- (void)setSelected:(BOOL)selected {
  [super setSelected:selected];
  if (selected) {
    self.backgroundColor =
        [UIColor colorNamed:@"omnibox_suggestion_row_highlight_color"];
  } else {
    self.backgroundColor = UIColor.clearColor;
  }
}

- (void)addSubviews {
  // Rounds corners in a Squircle.
  self.layer.cornerCurve = kCACornerCurveContinuous;
  self.layer.cornerRadius = kPreviewCornerRadius;
  // TODO(crbug.com/1365374): Add context Menu.
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
}

#pragma mark - Public methods

- (void)setupWithCarouselItem:(CarouselItem*)carouselItem {
  if (self.subviews.count == 0) {
    [self addSubviews];
  }
  self.carouselItem = carouselItem;
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

@end
