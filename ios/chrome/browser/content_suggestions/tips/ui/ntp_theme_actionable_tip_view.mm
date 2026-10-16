// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tips/ui/ntp_theme_actionable_tip_view.h"

#import "ios/chrome/browser/content_suggestions/tips/ui/ntp_theme_actionable_tip_config.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_audience.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

constexpr CGFloat kSwatchSize = 52.0;
constexpr CGFloat kSelectedBorderWidth = 3.0;
constexpr CGFloat kInnerInset = 5.0;
constexpr CGFloat kMoreSymbolSize = 20.0;
constexpr NSInteger kMoreButtonTag = 4;

}  // namespace

@implementation NTPThemeActionableTipView {
  NTPThemeActionableTipConfig* _config;
  UIStackView* _stackView;
}

- (instancetype)initWithConfig:(NTPThemeActionableTipConfig*)config {
  if ((self = [super initWithFrame:CGRectZero])) {
    CHECK(config);
    _config = config;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    [self createSubviews];
  }
  return self;
}

#pragma mark - Private

- (void)createSubviews {
  NSMutableArray<UIControl*>* buttons = [NSMutableArray array];

  for (NSUInteger i = 0; i < _config.colorPalettes.count; ++i) {
    BOOL selected = (static_cast<NSInteger>(i) == _config.selectedIndex);
    [buttons
        addObject:[self createColorSwatchWithPalette:_config.colorPalettes[i]
                                                 tag:i
                                            selected:selected]];
  }

  for (NSUInteger i = 0; i < _config.backgroundImages.count; ++i) {
    NSInteger tag = _config.colorPalettes.count + i;
    BOOL selected = (tag == _config.selectedIndex);
    [buttons
        addObject:[self createImageSwatchWithImage:_config.backgroundImages[i]
                                               tag:tag
                                          selected:selected]];
  }

  [buttons addObject:[self createMoreSwatchButton]];

  _stackView = [[UIStackView alloc] initWithArrangedSubviews:buttons];
  _stackView.axis = UILayoutConstraintAxisHorizontal;
  _stackView.distribution = UIStackViewDistributionEqualSpacing;
  _stackView.alignment = UIStackViewAlignmentCenter;
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_stackView];
  AddSameConstraints(_stackView, self);
}

- (UIControl*)createColorSwatchWithPalette:(NewTabPageColorPalette*)palette
                                       tag:(NSInteger)tag
                                  selected:(BOOL)selected {
  UIControl* wrapper = [[UIControl alloc] init];
  wrapper.translatesAutoresizingMaskIntoConstraints = NO;
  wrapper.tag = tag;
  wrapper.layer.cornerRadius = kSwatchSize / 2.0;
  wrapper.layer.borderColor = [UIColor colorNamed:kStaticBlueColor].CGColor;
  wrapper.layer.borderWidth = selected ? kSelectedBorderWidth : 0.0;
  [wrapper addTarget:self
                action:@selector(handleSwatchTapped:)
      forControlEvents:UIControlEventTouchUpInside];

  UIView* innerCircle = [[UIView alloc] init];
  innerCircle.translatesAutoresizingMaskIntoConstraints = NO;
  innerCircle.userInteractionEnabled = NO;
  innerCircle.layer.cornerRadius = (kSwatchSize - 2 * kInnerInset) / 2.0;
  innerCircle.clipsToBounds = YES;
  [wrapper addSubview:innerCircle];
  AddSameConstraintsWithInset(innerCircle, wrapper, kInnerInset);

  UIView* topHalf = [[UIView alloc] init];
  topHalf.translatesAutoresizingMaskIntoConstraints = NO;
  topHalf.backgroundColor =
      palette.lightColor ?: [UIColor colorNamed:kGrey100Color];

  UIView* bottomLeft = [[UIView alloc] init];
  bottomLeft.translatesAutoresizingMaskIntoConstraints = NO;
  bottomLeft.backgroundColor =
      palette.darkColor ?: [UIColor colorNamed:kBlueColor];

  UIView* bottomRight = [[UIView alloc] init];
  bottomRight.translatesAutoresizingMaskIntoConstraints = NO;
  bottomRight.backgroundColor =
      palette.mediumColor ?: [UIColor colorNamed:kGrey200Color];

  [innerCircle addSubview:topHalf];
  [innerCircle addSubview:bottomLeft];
  [innerCircle addSubview:bottomRight];

  [NSLayoutConstraint activateConstraints:@[
    [wrapper.widthAnchor constraintEqualToConstant:kSwatchSize],
    [wrapper.heightAnchor constraintEqualToConstant:kSwatchSize],
    [topHalf.topAnchor constraintEqualToAnchor:innerCircle.topAnchor],
    [topHalf.leadingAnchor constraintEqualToAnchor:innerCircle.leadingAnchor],
    [topHalf.trailingAnchor constraintEqualToAnchor:innerCircle.trailingAnchor],
    [topHalf.heightAnchor constraintEqualToAnchor:innerCircle.heightAnchor
                                       multiplier:0.5],
    [bottomLeft.bottomAnchor constraintEqualToAnchor:innerCircle.bottomAnchor],
    [bottomLeft.leadingAnchor
        constraintEqualToAnchor:innerCircle.leadingAnchor],
    [bottomLeft.widthAnchor constraintEqualToAnchor:innerCircle.widthAnchor
                                         multiplier:0.5],
    [bottomLeft.heightAnchor constraintEqualToAnchor:innerCircle.heightAnchor
                                          multiplier:0.5],
    [bottomRight.bottomAnchor constraintEqualToAnchor:innerCircle.bottomAnchor],
    [bottomRight.trailingAnchor
        constraintEqualToAnchor:innerCircle.trailingAnchor],
    [bottomRight.widthAnchor constraintEqualToAnchor:innerCircle.widthAnchor
                                          multiplier:0.5],
    [bottomRight.heightAnchor constraintEqualToAnchor:innerCircle.heightAnchor
                                           multiplier:0.5],
  ]];

  return wrapper;
}

- (UIControl*)createImageSwatchWithImage:(UIImage*)image
                                     tag:(NSInteger)tag
                                selected:(BOOL)selected {
  UIControl* wrapper = [[UIControl alloc] init];
  wrapper.translatesAutoresizingMaskIntoConstraints = NO;
  wrapper.tag = tag;
  wrapper.layer.cornerRadius = kSwatchSize / 2.0;
  wrapper.layer.borderColor = [UIColor colorNamed:kStaticBlueColor].CGColor;
  wrapper.layer.borderWidth = selected ? kSelectedBorderWidth : 0.0;
  [wrapper addTarget:self
                action:@selector(handleSwatchTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageView.userInteractionEnabled = NO;
  imageView.contentMode = UIViewContentModeScaleAspectFill;
  imageView.backgroundColor = [UIColor colorNamed:kGrey200Color];
  imageView.layer.cornerRadius = (kSwatchSize - 2 * kInnerInset) / 2.0;
  imageView.clipsToBounds = YES;
  [wrapper addSubview:imageView];
  AddSameConstraintsWithInset(imageView, wrapper, kInnerInset);
  [NSLayoutConstraint activateConstraints:@[
    [wrapper.widthAnchor constraintEqualToConstant:kSwatchSize],
    [wrapper.heightAnchor constraintEqualToConstant:kSwatchSize],
  ]];
  return wrapper;
}

- (UIControl*)createMoreSwatchButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.tag = kMoreButtonTag;
  button.backgroundColor = [UIColor colorNamed:kNTPRedesignTileBackgroundColor];
  button.tintColor = [UIColor colorNamed:kSolidBlackColor];
  button.layer.cornerRadius = kSwatchSize / 2.0;
  [button setImage:SymbolWithPointSize(SymbolPhotoBadgePlus, kMoreSymbolSize)
          forState:UIControlStateNormal];
  [button addTarget:self
                action:@selector(handleSwatchTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  [NSLayoutConstraint activateConstraints:@[
    [button.widthAnchor constraintEqualToConstant:kSwatchSize],
    [button.heightAnchor constraintEqualToConstant:kSwatchSize],
  ]];
  return button;
}

- (void)handleSwatchTapped:(UIControl*)sender {
  if (sender.tag == kMoreButtonTag) {
    [_config.audience didSelectNTPThemePhotoPicker];
    return;
  }
  for (UIView* subview in _stackView.arrangedSubviews) {
    if (subview.tag < kMoreButtonTag) {
      subview.layer.borderWidth =
          (subview.tag == sender.tag) ? kSelectedBorderWidth : 0.0;
    }
  }
  [_config.audience didSelectNTPThemeAtIndex:sender.tag];
}

@end
