// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_button.h"

#import <QuartzCore/QuartzCore.h>

#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The corner radius of the button.
const CGFloat kCornerRadius = 10;
// The margin for top and bottom placements.
const CGFloat kVerticalMargin = 8;
// The margin for leading and trailing placements.
const CGFloat kHorizontalMargin = 16;
// The max count to show. Afterwards, it shows "`kMaxCount`+".
const NSUInteger kMaxCount = 99;
}  // namespace

@interface InactiveTabsButton ()
@property(nonatomic, strong) UILabel* countLabel;
@end

@implementation InactiveTabsButton

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = UIColor.darkGrayColor;
    self.layer.cornerRadius = kCornerRadius;
    self.layer.masksToBounds = YES;

    // TODO(crbug.com/1413187): Handle Dynamic Type for title, subtitle, count,
    // and disclosure.
    UILabel* titleLabel = [[UILabel alloc] init];
    titleLabel.text =
        l10n_util::GetNSString(IDS_IOS_INACTIVE_TABS_BUTTON_TITLE);
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:titleLabel];

    UILabel* subtitleLabel = [[UILabel alloc] init];
    subtitleLabel.text =
        l10n_util::GetNSStringF(IDS_IOS_INACTIVE_TABS_BUTTON_SUBTITLE,
                                TabInactivityThresholdDisplayString());
    subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:subtitleLabel];

    UIImage* icon = DefaultSymbolTemplateWithPointSize(
        kChevronForwardSymbol, kSymbolAccessoryPointSize);
    UIImageView* disclosure = [[UIImageView alloc] initWithImage:icon];
    disclosure.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:disclosure];

    // TODO(crbug.com/1413187): Handle different sets of constraints for the
    // count label based on the Dynamic Type setting.
    [NSLayoutConstraint activateConstraints:@[
      [titleLabel.topAnchor constraintEqualToAnchor:self.topAnchor
                                           constant:kVerticalMargin],
      [titleLabel.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                               constant:kHorizontalMargin],
      [subtitleLabel.topAnchor constraintEqualToAnchor:titleLabel.bottomAnchor],
      [subtitleLabel.leadingAnchor
          constraintEqualToAnchor:titleLabel.leadingAnchor],
      [subtitleLabel.bottomAnchor constraintEqualToAnchor:self.bottomAnchor
                                                 constant:-kVerticalMargin],
      [disclosure.heightAnchor constraintEqualToConstant:22],
      [disclosure.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
      [disclosure.trailingAnchor constraintEqualToAnchor:self.trailingAnchor
                                                constant:-kHorizontalMargin],
      [disclosure.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                      constant:kVerticalMargin],
      [disclosure.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.bottomAnchor
                                   constant:-kVerticalMargin],
    ]];

    if (IsShowInactiveTabsCountEnabled()) {
      _countLabel = [[UILabel alloc] init];
      _countLabel.translatesAutoresizingMaskIntoConstraints = NO;
      [self addSubview:_countLabel];

      [NSLayoutConstraint activateConstraints:@[
        [_countLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [_countLabel.trailingAnchor
            constraintEqualToAnchor:disclosure.leadingAnchor
                           constant:-kHorizontalMargin],
        [_countLabel.topAnchor
            constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                        constant:kVerticalMargin],
        [_countLabel.bottomAnchor
            constraintLessThanOrEqualToAnchor:self.bottomAnchor
                                     constant:-kVerticalMargin],
      ]];
    }
  }
  return self;
}

- (void)setCount:(NSUInteger)count {
  _count = count;
  self.countLabel.text = count > kMaxCount
                             ? [NSString stringWithFormat:@"%@+", @(kMaxCount)]
                             : [NSString stringWithFormat:@"%@", @(count)];
}

@end
