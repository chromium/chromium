// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_header_view_controller.h"

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/feed_control_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Leading margin for title label. Its used to align with the Card leading
// margin.
const CGFloat kTitleHorizontalMargin = 19;
// Trailing/leading margins for header buttons. Its used to align with the card
// margins.
const CGFloat kButtonHorizontalMargin = 14;
// Font size for label text in header.
const CGFloat kDiscoverFeedTitleFontSize = 16;
// Insets for header menu button.
const CGFloat kHeaderMenuButtonInsetTopAndBottom = 2;
const CGFloat kHeaderMenuButtonInsetSides = 2;
// The width of the feed content. Currently hard coded in Mulder.
// TODO(crbug.com/1085419): Get card width from Mulder.
const CGFloat kDiscoverFeedContentWith = 430;
// The height of the header container. The content is unaffected.
// TODO(crbug.com/1277504): Only keep the WC header after launch.
const CGFloat kWebChannelsHeaderHeight = 52;
const CGFloat kDiscoverFeedHeaderHeight = 40;
// * Values below are exclusive to Web Channels.
// The width of the feed selector segments.
const CGFloat kHeaderSegmentWidth = 150;
// The height and width of the header menu button. Based on the default
// UISegmentedControl height.
const CGFloat kButtonSize = 28;
// Duration of the fade animation when the sort button appears/disappears.
const CGFloat kSortButtonAnimationDuration = 0.3;

// Image names for feed header icons.
NSString* kMenuIcon = @"ellipsis";
NSString* kSortIcon = @"arrow.up.arrow.down";
// TODO(crbug.com/1277974): Remove this when Web Channels is launched.
NSString* kDiscoverMenuIcon = @"infobar_settings_icon";
}

@interface FeedHeaderViewController ()

// View containing elements of the header. Handles header sizing.
@property(nonatomic, strong) UIView* container;

// Title label element for the feed.
@property(nonatomic, strong) UILabel* titleLabel;

// Button for opening top-level feed menu.
// Redefined to not be readonly.
@property(nonatomic, strong) UIButton* menuButton;

// Button for sorting feed content. Only used for Following feed.
@property(nonatomic, strong) UIButton* sortButton;

// Segmented control for toggling between the two feeds.
@property(nonatomic, strong) UISegmentedControl* segmentedControl;

// Currently selected feed.
// TODO(crbug.com/1277974): Reassign this value instead of recreating feed
// header when NTP coordinator's restart is improved.
@property(nonatomic, assign) FeedType selectedFeed;

@end

@implementation FeedHeaderViewController

- (instancetype)initWithSelectedFeed:(FeedType)selectedFeed {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _selectedFeed = selectedFeed;

    // The menu button is created early so that it can be assigned a tap action
    // before the view loads.
    _menuButton = [[UIButton alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.container = [[UIView alloc] init];

  self.view.backgroundColor =
      [[UIColor colorNamed:kBackgroundColor] colorWithAlphaComponent:0.95];

  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.container.translatesAutoresizingMaskIntoConstraints = NO;

  [self configureMenuButton:self.menuButton];

  if (IsWebChannelsEnabled()) {
    self.segmentedControl = [self createSegmentedControl];
    [self.container addSubview:self.segmentedControl];

    self.sortButton = [self createSortButton];
    self.sortButton.menu = [self createSortMenu];
    [self.container addSubview:self.sortButton];
  } else {
    self.titleLabel = [self createTitleLabel];
    [self.container addSubview:self.titleLabel];
  }

  [self.container addSubview:self.menuButton];
  [self.view addSubview:self.container];
  [self applyHeaderConstraints];
}

#pragma mark - Setters

// Sets |titleText| and updates header label if it exists.
- (void)setTitleText:(NSString*)titleText {
  _titleText = titleText;
  if (self.titleLabel) {
    self.titleLabel.text = titleText;
    [self.titleLabel setNeedsDisplay];
  }
}

#pragma mark - Private

// Creates sort menu with its content.
- (UIMenu*)createSortMenu {
  NSMutableArray<UIAction*>* sortActions = [NSMutableArray array];

  UIAction* sortByPublisherAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_FEED_SORT_PUBLISHER)
                image:nil
           identifier:nil
              handler:^(UIAction* action){
                  // TODO(crbug.com/1277974): Handle selected sorting.
              }];
  // TODO(crbug.com/1277974): Set the active state based on selected sorting.
  sortByPublisherAction.state = UIMenuElementStateOn;
  [sortActions addObject:sortByPublisherAction];

  UIAction* sortByLatestAction =
      [UIAction actionWithTitle:l10n_util::GetNSString(IDS_IOS_FEED_SORT_LATEST)
                          image:nil
                     identifier:nil
                        handler:^(UIAction* action){
                            // TODO(crbug.com/1277974): Handle selected sorting.
                        }];
  [sortActions addObject:sortByLatestAction];

  return [UIMenu menuWithTitle:@"" children:sortActions];
}

// Configures the feed header's menu button.
- (void)configureMenuButton:(UIButton*)menuButton {
  menuButton.translatesAutoresizingMaskIntoConstraints = NO;
  menuButton.accessibilityIdentifier = kNTPFeedHeaderMenuButtonIdentifier;
  menuButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_MENU_ACCESSIBILITY_LABEL);
  if (IsWebChannelsEnabled()) {
    [menuButton setImage:[UIImage systemImageNamed:kMenuIcon]
                forState:UIControlStateNormal];
    menuButton.backgroundColor = [UIColor colorNamed:kGrey100Color];
    menuButton.clipsToBounds = YES;
    menuButton.layer.cornerRadius = kButtonSize / 2;
  } else {
    [menuButton
        setImage:[[UIImage imageNamed:kDiscoverMenuIcon]
                     imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
        forState:UIControlStateNormal];
    menuButton.tintColor = [UIColor colorNamed:kGrey600Color];
    menuButton.imageEdgeInsets = UIEdgeInsetsMake(
        kHeaderMenuButtonInsetTopAndBottom, kHeaderMenuButtonInsetSides,
        kHeaderMenuButtonInsetTopAndBottom, kHeaderMenuButtonInsetSides);
  }
}

// Configures and returns the feed header's sorting button.
- (UIButton*)createSortButton {
  DCHECK(IsWebChannelsEnabled());

  UIButton* sortButton = [[UIButton alloc] init];

  sortButton.translatesAutoresizingMaskIntoConstraints = NO;
  sortButton.accessibilityIdentifier = kNTPFeedHeaderSortButtonIdentifier;
  sortButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_FEED_SORT_ACCESSIBILITY_LABEL);
  [sortButton setImage:[UIImage systemImageNamed:kSortIcon]
              forState:UIControlStateNormal];
  sortButton.showsMenuAsPrimaryAction = YES;

  // The sort button is only visible if the Following feed is selected.
  // TODO(crbug.com/1277974): Determine if the button should show when the feed
  // is hidden.
  sortButton.alpha = self.selectedFeed == FeedType::kFollowingFeed ? 1 : 0;

  return sortButton;
}

// Configures and returns the feed header's title label.
- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont systemFontOfSize:kDiscoverFeedTitleFontSize
                                      weight:UIFontWeightMedium];
  titleLabel.textColor = [UIColor colorNamed:kGrey700Color];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.accessibilityIdentifier =
      ntp_home::DiscoverHeaderTitleAccessibilityID();
  titleLabel.text = self.titleText;
  return titleLabel;
}

// Configures and returns the segmented control for toggling between feeds.
- (UISegmentedControl*)createSegmentedControl {
  // Create segmented control with labels.
  NSArray* headerLabels = [NSArray
      arrayWithObjects:l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE),
                       l10n_util::GetNSString(IDS_IOS_FOLLOWING_FEED_TITLE),
                       nil];
  UISegmentedControl* segmentedControl =
      [[UISegmentedControl alloc] initWithItems:headerLabels];
  segmentedControl.translatesAutoresizingMaskIntoConstraints = NO;
  [segmentedControl setApportionsSegmentWidthsByContent:NO];
  for (NSUInteger i = 0; i < segmentedControl.numberOfSegments; ++i) {
    [segmentedControl setWidth:kHeaderSegmentWidth forSegmentAtIndex:i];
  }

  // Set text font and color.
  UIFont* font = [UIFont systemFontOfSize:kDiscoverFeedTitleFontSize
                                   weight:UIFontWeightMedium];
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
  [segmentedControl setTitleTextAttributes:attributes
                                  forState:UIControlStateNormal];
  segmentedControl.backgroundColor = [UIColor colorNamed:kGrey100Color];

  // Set selected feed and tap action.
  segmentedControl.selectedSegmentIndex =
      static_cast<NSInteger>(self.selectedFeed);
  [segmentedControl addTarget:self
                       action:@selector(onSegmentSelected:)
             forControlEvents:UIControlEventValueChanged];

  return segmentedControl;
}

// Applies constraints for the feed header elements' positioning.
- (void)applyHeaderConstraints {
  // Anchor container and menu button.
  [NSLayoutConstraint activateConstraints:@[
    [self.container.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.container.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.container.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.container.widthAnchor
        constraintEqualToConstant:MIN(kDiscoverFeedContentWith,
                                      self.view.frame.size.width)],
    [self.menuButton.trailingAnchor
        constraintEqualToAnchor:self.container.trailingAnchor
                       constant:-kButtonHorizontalMargin],
    [self.menuButton.centerYAnchor
        constraintEqualToAnchor:self.container.centerYAnchor],
  ]];
  if (IsWebChannelsEnabled()) {
    [NSLayoutConstraint activateConstraints:@[
      [self.view.heightAnchor
          constraintEqualToConstant:kWebChannelsHeaderHeight],
      // Anchor segmented control.
      [self.segmentedControl.centerXAnchor
          constraintEqualToAnchor:self.container.centerXAnchor],
      [self.segmentedControl.centerYAnchor
          constraintEqualToAnchor:self.container.centerYAnchor],
      // Set menu button size.
      [self.menuButton.heightAnchor constraintEqualToConstant:kButtonSize],
      [self.menuButton.widthAnchor constraintEqualToConstant:kButtonSize],
      // Anchor sort button and set size.
      [self.sortButton.heightAnchor constraintEqualToConstant:kButtonSize],
      [self.sortButton.widthAnchor constraintEqualToConstant:kButtonSize],
      [self.sortButton.leadingAnchor
          constraintEqualToAnchor:self.container.leadingAnchor
                         constant:kButtonHorizontalMargin],
      [self.sortButton.centerYAnchor
          constraintEqualToAnchor:self.container.centerYAnchor],
    ]];
  } else {
    // Anchors title label.
    [NSLayoutConstraint activateConstraints:@[
      [self.view.heightAnchor
          constraintEqualToConstant:kDiscoverFeedHeaderHeight],
      [self.titleLabel.leadingAnchor
          constraintEqualToAnchor:self.container.leadingAnchor
                         constant:kTitleHorizontalMargin],
      [self.titleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.menuButton.leadingAnchor],
      [self.titleLabel.centerYAnchor
          constraintEqualToAnchor:self.container.centerYAnchor],
    ]];
  }
}

// Handles a new feed being selected from the header.
- (void)onSegmentSelected:(UISegmentedControl*)segmentedControl {
  switch (segmentedControl.selectedSegmentIndex) {
    case static_cast<NSInteger>(FeedType::kDiscoverFeed): {
      [self.feedControlDelegate handleFeedSelected:FeedType::kDiscoverFeed];
      [UIView animateWithDuration:kSortButtonAnimationDuration
                       animations:^{
                         self.sortButton.alpha = 0;
                       }];
      break;
    }
    case static_cast<NSInteger>(FeedType::kFollowingFeed): {
      [self.feedControlDelegate handleFeedSelected:FeedType::kFollowingFeed];
      [UIView animateWithDuration:kSortButtonAnimationDuration
                       animations:^{
                         self.sortButton.alpha = 1;
                       }];
      break;
    }
  }
}

@end
