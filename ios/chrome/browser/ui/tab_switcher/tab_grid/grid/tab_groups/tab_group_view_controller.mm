// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
constexpr CGFloat kColoredDotSize = 20;
constexpr CGFloat kTitleHorizontalMargin = 16;
constexpr CGFloat kTitleVerticalMargin = 10;
constexpr CGFloat kLeftMargin = 9;
constexpr CGFloat kFullTitleTopMargin = 24;
constexpr CGFloat kDotTitleSeparationMargin = 8;
}  // namespace

@interface TabGroupViewController () <UINavigationBarDelegate>
@end

@implementation TabGroupViewController {
  // The embedded navigation bar.
  UINavigationBar* _navigationBar;
  // Tab Groups handler.
  __weak id<TabGroupsCommands> _handler;
  // Group's title.
  NSString* _groupTitle;
  // Group's color.
  UIColor* _groupColor;
  // Group's creation date.
  base::Time _groupCreationDate;
}

#pragma mark - UIViewController

- (instancetype)initWithHandler:(id<TabGroupsCommands>)handler {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to create a tab group view controller outside "
         "the Tab Groups experiment.";
  if (self = [super init]) {
    _handler = handler;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  if (!UIAccessibilityIsReduceTransparencyEnabled()) {
    self.view.backgroundColor = [UIColor clearColor];
    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    UIVisualEffectView* blurEffectView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    blurEffectView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:blurEffectView];
    AddSameConstraints(self.view, blurEffectView);
  } else {
    self.view.backgroundColor = [UIColor blackColor];
  }

  [self configureNavigationBar];
  [self configurePrimaryTitle];
}

- (void)didTapPlusButton {
  // TODO(crbug.com/1501837): Add the creation of a new tab in the current
  // group.
}

#pragma mark - UINavigationBarDelegate

- (BOOL)navigationBar:(UINavigationBar*)navigationBar
        shouldPopItem:(UINavigationItem*)item {
  [_handler hideTabGroup];
  return NO;
}

#pragma mark - UIBarPositioningDelegate

- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  // Let the background of the navigation bar extend to the top, behind the
  // Dynamic Island or notch.
  return UIBarPositionTopAttached;
}

#pragma mark - TabGroupConsumer

- (void)setGroupTitle:(NSString*)title {
  _groupTitle = title;
}

- (void)setGroupColor:(UIColor*)color {
  _groupColor = color;
}

- (void)setGroupDateCreation:(base::Time)date {
  _groupCreationDate = date;
}

#pragma mark - Private

// Returns the navigation item which contain the back button.
- (UINavigationItem*)configuredBackButton {
  return [[UINavigationItem alloc] init];
}

// Returns the navigation item which contain the plus button.
- (UINavigationItem*)configuredPlusButton {
  UINavigationItem* plus = [[UINavigationItem alloc] init];
  plus.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemAdd
                           target:self
                           action:@selector(didTapPlusButton)];
  return plus;
}

// Configures the navigation bar.
- (void)configureNavigationBar {
  _navigationBar = [[UINavigationBar alloc] init];
  _navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  _navigationBar.items = @[
    [self configuredBackButton],
    [self configuredPlusButton],
  ];

  // Make the navigation bar transparent so it completly match the view.
  [_navigationBar setBackgroundImage:[UIImage new]
                       forBarMetrics:UIBarMetricsDefault];
  _navigationBar.shadowImage = [UIImage new];
  _navigationBar.translucent = YES;

  _navigationBar.tintColor = [UIColor colorNamed:kSolidWhiteColor];
  _navigationBar.delegate = self;
  [self.view addSubview:_navigationBar];

  [NSLayoutConstraint activateConstraints:@[
    [_navigationBar.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [_navigationBar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_navigationBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
}

// Returns the group color dot view.
- (UIView*)groupColorDotView {
  UIView* dotView = [[UIView alloc] initWithFrame:CGRectZero];
  dotView.translatesAutoresizingMaskIntoConstraints = NO;
  dotView.layer.cornerRadius = kColoredDotSize / 2;
  dotView.layer.backgroundColor = _groupColor.CGColor;

  [NSLayoutConstraint activateConstraints:@[
    [dotView.heightAnchor constraintEqualToConstant:kColoredDotSize],
    [dotView.widthAnchor constraintEqualToConstant:kColoredDotSize],
  ]];

  return dotView;
}

// Returns the title label view.
- (UILabel*)groupTitleView {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.textColor = [UIColor colorNamed:kSolidWhiteColor];
  titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle];

  UIFontDescriptor* boldDescriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleLargeTitle]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  NSMutableAttributedString* boldTitle =
      [[NSMutableAttributedString alloc] initWithString:_groupTitle];

  [boldTitle addAttribute:NSFontAttributeName
                    value:[UIFont fontWithDescriptor:boldDescriptor size:0.0]
                    range:NSMakeRange(0, _groupTitle.length)];
  titleLabel.attributedText = boldTitle;

  titleLabel.numberOfLines = 0;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  return titleLabel;
}

// Configures the full primary title (colored dot and text title).
- (void)configurePrimaryTitle {
  UIView* fullTitleView = [[UIView alloc] initWithFrame:CGRectZero];
  fullTitleView.translatesAutoresizingMaskIntoConstraints = NO;
  fullTitleView.backgroundColor =
      [[UIColor colorNamed:kSolidWhiteColor] colorWithAlphaComponent:0.1];
  fullTitleView.layer.cornerRadius = 17;
  fullTitleView.opaque = NO;

  UIView* coloredDotView = [self groupColorDotView];
  UILabel* titleView = [self groupTitleView];
  [fullTitleView addSubview:coloredDotView];
  [fullTitleView addSubview:titleView];

  [self.view addSubview:fullTitleView];

  [NSLayoutConstraint activateConstraints:@[
    [titleView.leadingAnchor
        constraintEqualToAnchor:coloredDotView.trailingAnchor
                       constant:kDotTitleSeparationMargin],
    [coloredDotView.centerYAnchor
        constraintEqualToAnchor:titleView.centerYAnchor],
    [fullTitleView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                                constant:kLeftMargin],
    [fullTitleView.topAnchor constraintEqualToAnchor:_navigationBar.bottomAnchor
                                            constant:kFullTitleTopMargin],
    [coloredDotView.leadingAnchor
        constraintEqualToAnchor:fullTitleView.leadingAnchor
                       constant:kTitleHorizontalMargin],
    [fullTitleView.trailingAnchor
        constraintEqualToAnchor:titleView.trailingAnchor
                       constant:kTitleHorizontalMargin],
    [titleView.topAnchor constraintEqualToAnchor:fullTitleView.topAnchor
                                        constant:kTitleVerticalMargin],
    [fullTitleView.bottomAnchor constraintEqualToAnchor:titleView.bottomAnchor
                                               constant:kTitleVerticalMargin],
  ]];
}

@end
