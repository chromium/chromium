// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_view.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_main_content_configuration.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {
const CGFloat kTableViewCustomSeparatorHeight = 0.5;
}  // namespace

@interface LegacyTableViewCell ()
@end

@implementation LegacyTableViewCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _customSeparator = [[UIView alloc] init];
    _customSeparator.translatesAutoresizingMaskIntoConstraints = NO;
    _customSeparator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
    // Defaults to hidden until a custom separator is explicitly set.
    _customSeparator.hidden = YES;

    [self addSubview:_customSeparator];

    NSArray* constraints = @[
      [_customSeparator.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_customSeparator.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [_customSeparator.heightAnchor
          constraintEqualToConstant:AlignValueToPixel(
                                        kTableViewCustomSeparatorHeight)],
      [_customSeparator.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
    ];
    for (NSLayoutConstraint* constraint in constraints) {
      // Have a priority higher than the default high but don't make it required
      // to allow subclass to override it.
      constraint.priority = UILayoutPriorityDefaultHigh + 1;
    }
    [NSLayoutConstraint activateConstraints:constraints];
  }
  return self;
}

- (void)setUseCustomSeparator:(BOOL)useCustomSeparator {
  _useCustomSeparator = useCustomSeparator;
  self.customSeparator.hidden = !useCustomSeparator;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.useCustomSeparator = NO;
  self.selectionStyle = UITableViewCellSelectionStyleDefault;
  self.userInteractionEnabled = YES;
  self.accessibilityLabel = nil;
  self.accessibilityUserInputLabels = nil;
  self.accessibilityValue = nil;
  self.accessibilityHint = nil;
  self.accessoryView = nil;
  self.accessibilityCustomActions = nil;
}

- (void)updateConfigurationUsingState:(UICellConfigurationState*)state {
  if ([self.contentConfiguration
          conformsToProtocol:@protocol(ChromeMainContentConfiguration)]) {
    id<ChromeMainContentConfiguration> configuration =
        static_cast<id<ChromeMainContentConfiguration>>(
            self.contentConfiguration);
    BOOL hasAccessoryView =
        self.accessoryType != UITableViewCellAccessoryNone ||
        self.accessoryView != nil;
    [configuration setHasAccessoryView:hasAccessoryView];
    self.contentConfiguration = configuration;
  }
  [super updateConfigurationUsingState:state];
}

#pragma mark - Accessibility

- (UIAccessibilityTraits)accessibilityTraits {
  UIAccessibilityTraits accessibilityTraits = super.accessibilityTraits;
  if (!self.isUserInteractionEnabled) {
    accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  return accessibilityTraits;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  NSObject* contentConfiguration = self.contentConfiguration;
  if (contentConfiguration.accessibilityUserInputLabels.count > 0) {
    return contentConfiguration.accessibilityUserInputLabels;
  }
  return [super accessibilityUserInputLabels];
}

- (CGPoint)accessibilityActivationPoint {
  if ([self.contentView conformsToProtocol:@protocol(ChromeContentView)]) {
    UIView<ChromeContentView>* chromeContentView =
        static_cast<UIView<ChromeContentView>*>(self.contentView);
    if ([chromeContentView hasCustomAccessibilityActivationPoint]) {
      return chromeContentView.accessibilityActivationPoint;
    }
  }
  return [super accessibilityActivationPoint];
}

@end
