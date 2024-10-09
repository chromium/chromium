// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/feed_header_view_controller.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ntp/ui_bundled/discover_feed_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_control_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_menu_commands.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Leading margin for title label. Its used to align with the Card leading
// margin.
const CGFloat kTitleHorizontalMargin = 19;
// Trailing/leading margins for header buttons. Its used to align with the card
// margins.
const CGFloat kButtonHorizontalMargin = 14;
// Font size for the custom search engine label.
const CGFloat kCustomSearchEngineLabelFontSize = 13;
// Font size for the hidden feed label.
const CGFloat kHiddenFeedLabelFontSize = 16;
// The width of the label for when the feed is hidden.
const CGFloat kHiddenFeedLabelWidth = 250;
// Insets for header menu button.
const CGFloat kHeaderManagementButtonInset = 2;
// The height of the header container without the Following feed. The content is
// unaffected.
const CGFloat kDiscoverFeedHeaderHeightWithoutFollowing = 40;
// The height of the header container with the Following feed. The content is
// unaffected.
const CGFloat kDiscoverFeedHeaderHeightWithFollowing = 30;

const CGFloat kCustomSearchEngineLabelHeight = 18;
// * Values below are exclusive to Web Channels.
// The height and width of the header menu button. Based on the default
// UISegmentedControl height.
const CGFloat kButtonSize = 28;
// Duration of the fade animation for elements that toggle when switching feeds.
const CGFloat kSegmentAnimationDuration = 0.3;
// Padding on top of the header.
const CGFloat kTopVerticalPaddingFollowing = 15;
const CGFloat kTopVerticalPadding = 5;

// The size of feed symbol images.
NSInteger kFeedSymbolPointSize = 17;

}  // namespace

@interface FeedHeaderViewController ()

// View containing elements of the header. Handles header sizing.
@property(nonatomic, strong) UIView* container;

// Title label element for the feed.
@property(nonatomic, strong) UILabel* titleLabel;

// Button for opening top-level feed management menu.
// Redefined to not be readonly.
@property(nonatomic, strong) UIButton* managementButton;

// Button for sorting feed content. Only used for Following feed.
@property(nonatomic, strong) UIButton* sortButton;

// Segmented control for toggling between the two feeds.
@property(nonatomic, strong) UISegmentedControl* segmentedControl;

// The view informing the user that the feed is powered by Google if they don't
// have Google as their default search engine.
@property(nonatomic, strong) UILabel* customSearchEngineView;

// The label for when the feed visibility is disabled.
@property(nonatomic, strong) UILabel* hiddenFeedLabel;

// The constraints for the currently visible components of the header.
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* feedHeaderConstraints;

@end

@implementation FeedHeaderViewController

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    // The menu button is created early so that it can be assigned a tap action
    // before the view loads.
    _managementButton = [[UIButton alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // The background color will be clear for continuity with the overall NTP
  // gradient view.
  self.view.backgroundColor = [UIColor clearColor];
  self.view.maximumContentSizeCategory =
      UIContentSizeCategoryAccessibilityMedium;
  self.view.accessibilityLabel = kNTPFeedHeaderIdentifier;

  self.container = [[UIView alloc] init];

  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.container.translatesAutoresizingMaskIntoConstraints = NO;

  if (!IsHomeCustomizationEnabled()) {
    [self configureManagementButton:self.managementButton];
  }
  [self configureHeaderViews];

  [self.view addSubview:self.container];
  [self applyHeaderConstraints];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitPreferredContentSizeCategory.self ]);
    [self registerForTraitChanges:traits
                       withTarget:self.view
                           action:@selector(setNeedsLayout)];
  }
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    [self updateSegmentedControlFont:self.segmentedControl];
  } else {
    UIFont* font = [self fontForTitleLabel];
    self.titleLabel.font = font;
  }
}

#pragma mark - UITraitEnvironment

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    [self.view setNeedsLayout];
  }
}
#endif

#pragma mark - Public

- (CGFloat)feedHeaderHeight {
  return [self.feedControlDelegate isFollowingFeedAvailable]
             ? kDiscoverFeedHeaderHeightWithFollowing
             : kDiscoverFeedHeaderHeightWithoutFollowing;
}

- (CGFloat)customSearchEngineViewHeight {
  return [self.NTPDelegate isGoogleDefaultSearchEngine] ||
                 ![self.feedControlDelegate isFollowingFeedAvailable]
             ? 0
             : kCustomSearchEngineLabelHeight;
}

- (void)updateForDefaultSearchEngineChanged {
  if (!self.viewLoaded) {
    return;
  }
  if (![self.feedControlDelegate isFollowingFeedAvailable]) {
    [self.titleLabel setText:[self feedHeaderTitleText]];
    [self.titleLabel setNeedsDisplay];
    return;
  }

  if ([self.NTPDelegate isGoogleDefaultSearchEngine]) {
    [self removeCustomSearchEngineView];
  } else {
    [self addCustomSearchEngineView];
  }
  [self applyHeaderConstraints];
}

- (void)updateForFeedVisibilityChanged {
  // When feed visibility changes, the menu content is recreated.
  [self.feedMenuHandler configureManagementMenu:self.managementButton];

  if (![self.feedControlDelegate isFollowingFeedAvailable]) {
    [self.titleLabel setText:[self feedHeaderTitleText]];
    [self.titleLabel setNeedsDisplay];
    return;
  }

  [self resetView];

  if ([self.feedControlDelegate shouldFeedBeVisible]) {
    [self addViewsForVisibleFeed];
  } else {
    [self addViewsForHiddenFeed];
  }

  [self applyHeaderConstraints];
}

- (void)updateForFollowingFeedVisibilityChanged {
  [self resetView];
  [self.titleLabel removeFromSuperview];

  // The management button is different for the Following feed header, so it's
  // recreated.
  if (self.managementButton) {
    [self.managementButton removeFromSuperview];
    self.managementButton = nil;
  }
  if (!IsHomeCustomizationEnabled()) {
    self.managementButton = [[UIButton alloc] init];
    [self configureManagementButton:self.managementButton];
    [self.feedMenuHandler configureManagementMenu:self.managementButton];
  }

  [self configureHeaderViews];
  [self applyHeaderConstraints];
}

- (void)updateForSelectedFeed {
  FeedType selectedFeed = [self.feedControlDelegate selectedFeed];
  self.segmentedControl.selectedSegmentIndex =
      static_cast<NSInteger>(selectedFeed);
  if (!IsFollowUIUpdateEnabled()) {
    self.sortButton.alpha = selectedFeed == FeedTypeDiscover ? 0 : 1;
  }
}

#pragma mark - Setters

// Sets `followingFeedSortType` and recreates the sort menu to assign the active
// sort type.
- (void)setFollowingFeedSortType:(FollowingFeedSortType)followingFeedSortType {
  CHECK(!IsFollowUIUpdateEnabled());
  _followingFeedSortType = followingFeedSortType;
  if (self.sortButton) {
    self.sortButton.menu = [self createSortMenu];
  }
}

#pragma mark - Private

- (void)configureHeaderViews {
  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    if ([self.feedControlDelegate shouldFeedBeVisible]) {
      [self addViewsForVisibleFeed];
    } else {
      [self addViewsForHiddenFeed];
    }

    if (![self.NTPDelegate isGoogleDefaultSearchEngine]) {
      [self addCustomSearchEngineView];
    }
  } else {
    self.titleLabel = [self createTitleLabel];
    [self.container addSubview:self.titleLabel];
  }
  if (!IsHomeCustomizationEnabled()) {
    [self.feedMenuHandler configureManagementMenu:self.managementButton];
  }
}

// Creates sort menu with its content and active sort type.
- (UIMenu*)createSortMenu {
  NSMutableArray<UIAction*>* sortActions = [NSMutableArray array];

  // Create menu actions.
  UIAction* sortByPublisherAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_FEED_SORT_PUBLISHER)
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                [self.feedControlDelegate handleSortTypeForFollowingFeed:
                                              FollowingFeedSortTypeByPublisher];
              }];
  [sortActions addObject:sortByPublisherAction];
  UIAction* sortByLatestAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_FEED_SORT_LATEST)
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                [self.feedControlDelegate handleSortTypeForFollowingFeed:
                                              FollowingFeedSortTypeByLatest];
              }];
  [sortActions addObject:sortByLatestAction];

  // Set active sorting.
  switch (self.followingFeedSortType) {
    case FollowingFeedSortTypeByLatest:
      sortByLatestAction.state = UIMenuElementStateOn;
      break;
    case FollowingFeedSortTypeByPublisher:
    default:
      sortByPublisherAction.state = UIMenuElementStateOn;
  }

  return [UIMenu menuWithTitle:@"" children:sortActions];
}

// Configures the feed header's menu button.
- (void)configureManagementButton:(UIButton*)managementButton {
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];

  managementButton.translatesAutoresizingMaskIntoConstraints = NO;
  managementButton.showsMenuAsPrimaryAction = YES;
  [managementButton addTarget:self.feedMenuHandler
                       action:@selector(configureManagementMenu:)
             forControlEvents:UIControlEventTouchDown];

  managementButton.accessibilityIdentifier =
      kNTPFeedHeaderManagementButtonIdentifier;
  managementButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_MENU_ACCESSIBILITY_LABEL);

  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    buttonConfiguration.image =
        DefaultSymbolTemplateWithPointSize(kMenuSymbol, kFeedSymbolPointSize);
    managementButton.clipsToBounds = YES;
  } else {
    UIImage* menuIcon = DefaultSymbolTemplateWithPointSize(
        kSettingsFilledSymbol, kFeedSymbolPointSize);
    buttonConfiguration.image = menuIcon;
    buttonConfiguration.baseForegroundColor =
        [UIColor colorNamed:kGrey600Color];
    buttonConfiguration.imagePadding = kHeaderManagementButtonInset;
  }

  [self.container addSubview:managementButton];
  managementButton.configuration = buttonConfiguration;
}

// Configures and returns the feed header's sorting button.
- (UIButton*)createSortButton {
  DCHECK([self.feedControlDelegate isFollowingFeedAvailable]);

  UIButton* sortButton = [[UIButton alloc] init];

  sortButton.translatesAutoresizingMaskIntoConstraints = NO;
  sortButton.accessibilityIdentifier = kNTPFeedHeaderSortButtonIdentifier;
  sortButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_FEED_SORT_ACCESSIBILITY_LABEL);
  [sortButton setImage:DefaultSymbolTemplateWithPointSize(kSortSymbol,
                                                          kFeedSymbolPointSize)
              forState:UIControlStateNormal];
  sortButton.showsMenuAsPrimaryAction = YES;

  // The sort button is only visible if the Following feed is selected, and if
  // the Follow UI update is not enabled.
  if (IsFollowUIUpdateEnabled()) {
    sortButton.alpha = 0;
  } else {
    sortButton.alpha =
        [self.feedControlDelegate selectedFeed] == FeedTypeDiscover ? 0 : 1;
  }

  sortButton.configuration = [UIButtonConfiguration plainButtonConfiguration];

  return sortButton;
}

// Configures and returns the feed header's title label.
- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [self fontForTitleLabel];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.accessibilityIdentifier =
      ntp_home::DiscoverHeaderTitleAccessibilityID();
  titleLabel.text = [self feedHeaderTitleText];
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

  [self updateSegmentedControlFont:segmentedControl];

  // Set selected feed and tap action.
  segmentedControl.selectedSegmentIndex =
      static_cast<NSInteger>([self.feedControlDelegate selectedFeed]);
  [segmentedControl addTarget:self
                       action:@selector(onSegmentSelected:)
             forControlEvents:UIControlEventValueChanged];

  segmentedControl.accessibilityIdentifier =
      kNTPFeedHeaderSegmentedControlIdentifier;

  return segmentedControl;
}

- (UIFont*)fontForTitleLabel {
  return CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold,
                           self.view);
}

- (UIFont*)fontForSegmentedControl {
  return CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightMedium,
                           self.view);
}

// Configures and returns the label for when the feed visibility is
// disabled.
- (UILabel*)createHiddenFeedLabel {
  UILabel* hiddenFeedLabel = [[UILabel alloc] init];
  [hiddenFeedLabel setText:l10n_util::GetNSString(
                               IDS_IOS_DISCOVER_FEED_HEADER_TURNED_OFF_LABEL)];
  hiddenFeedLabel.translatesAutoresizingMaskIntoConstraints = NO;
  hiddenFeedLabel.font = [UIFont systemFontOfSize:kHiddenFeedLabelFontSize];
  hiddenFeedLabel.textColor = [UIColor colorNamed:kGrey600Color];
  hiddenFeedLabel.numberOfLines = 0;
  hiddenFeedLabel.textAlignment = NSTextAlignmentCenter;
  return hiddenFeedLabel;
}

// Updates the font and color of the segmented control header to adapt to the
// current dynamic sizing.
- (void)updateSegmentedControlFont:(UISegmentedControl*)segmentedControl {
  NSDictionary* normalAttributes = [NSDictionary
      dictionaryWithObjectsAndKeys:[self fontForSegmentedControl],
                                   NSFontAttributeName,
                                   [UIColor colorNamed:kTextSecondaryColor],
                                   NSForegroundColorAttributeName, nil];
  [segmentedControl setTitleTextAttributes:normalAttributes
                                  forState:UIControlStateNormal];
  NSDictionary* selectedAttributes = [NSDictionary
      dictionaryWithObjectsAndKeys:[self fontForSegmentedControl],
                                   NSFontAttributeName,
                                   [UIColor colorNamed:kTextPrimaryColor],
                                   NSForegroundColorAttributeName, nil];
  [segmentedControl setTitleTextAttributes:selectedAttributes
                                  forState:UIControlStateSelected];
}

- (void)addCustomSearchEngineView {
  if (self.customSearchEngineView) {
    [self removeCustomSearchEngineView];
  }
  self.customSearchEngineView = [[UILabel alloc] init];
  self.customSearchEngineView.text =
      l10n_util::GetNSString(IDS_IOS_FEED_CUSTOM_SEARCH_ENGINE_LABEL);
  self.customSearchEngineView.font =
      [UIFont systemFontOfSize:kCustomSearchEngineLabelFontSize];
  self.customSearchEngineView.textColor = [UIColor colorNamed:kGrey500Color];
  self.customSearchEngineView.translatesAutoresizingMaskIntoConstraints = NO;
  self.customSearchEngineView.textAlignment = NSTextAlignmentCenter;
  [self.view addSubview:self.customSearchEngineView];
}

- (void)removeCustomSearchEngineView {
  [self.customSearchEngineView removeFromSuperview];
  self.customSearchEngineView = nil;
}

// Applies constraints for the feed header elements' positioning.
- (void)applyHeaderConstraints {
  // Remove previous constraints if they were already set.
  if (self.feedHeaderConstraints) {
    [NSLayoutConstraint deactivateConstraints:self.feedHeaderConstraints];
    self.feedHeaderConstraints = nil;
  }

  self.feedHeaderConstraints = [[NSMutableArray alloc] init];

  CGFloat totalHeaderHeight =
      [self feedHeaderHeight] + [self customSearchEngineViewHeight];
  totalHeaderHeight += [self.feedControlDelegate isFollowingFeedAvailable]
                           ? kTopVerticalPaddingFollowing
                           : kTopVerticalPadding;
  // Anchor container.
  [self.feedHeaderConstraints addObjectsFromArray:@[
    // Anchor container and menu button.
    [self.view.heightAnchor constraintEqualToConstant:totalHeaderHeight],
    [self.container.heightAnchor
        constraintEqualToConstant:[self feedHeaderHeight]],
    [self.container.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.container.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.container.widthAnchor constraintEqualToAnchor:self.view.widthAnchor],
  ]];

  if (!IsHomeCustomizationEnabled()) {
    // Anchor management button.
    [self.feedHeaderConstraints addObjectsFromArray:@[
      [self.managementButton.trailingAnchor
          constraintEqualToAnchor:self.container.trailingAnchor
                         constant:-kButtonHorizontalMargin],
      [self.managementButton.centerYAnchor
          constraintEqualToAnchor:self.container.centerYAnchor],
      // Set menu button size.
      [self.managementButton.heightAnchor
          constraintEqualToConstant:kButtonSize],
      [self.managementButton.widthAnchor constraintEqualToConstant:kButtonSize],
    ]];
  }

  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    // Anchor views based on the feed being visible or hidden.
    if ([self.feedControlDelegate shouldFeedBeVisible]) {
      [self anchorSegmentedControl];

      // Anchor sort button.
      [self.feedHeaderConstraints addObjectsFromArray:@[
        [self.sortButton.heightAnchor constraintEqualToConstant:kButtonSize],
        [self.sortButton.widthAnchor constraintEqualToConstant:kButtonSize],
        [self.sortButton.leadingAnchor
            constraintEqualToAnchor:self.container.leadingAnchor
                           constant:kButtonHorizontalMargin],
        [self.sortButton.centerYAnchor
            constraintEqualToAnchor:self.container.centerYAnchor],
      ]];
    } else {
      [self.feedHeaderConstraints addObjectsFromArray:@[
        [self.hiddenFeedLabel.centerXAnchor
            constraintEqualToAnchor:self.container.centerXAnchor],
        [self.hiddenFeedLabel.centerYAnchor
            constraintEqualToAnchor:self.container.centerYAnchor],
        [self.hiddenFeedLabel.widthAnchor
            constraintEqualToConstant:kHiddenFeedLabelWidth],
      ]];
    }

    // If Google is not the default search engine, anchor the custom search
    // engine view.
    if (![self.NTPDelegate isGoogleDefaultSearchEngine] &&
        [self.feedControlDelegate shouldFeedBeVisible]) {
      [self.feedHeaderConstraints addObjectsFromArray:@[
        // Anchors custom search engine view.
        [self.customSearchEngineView.widthAnchor
            constraintEqualToAnchor:self.view.widthAnchor],
        [self.customSearchEngineView.heightAnchor
            constraintEqualToConstant:kCustomSearchEngineLabelHeight],
        [self.customSearchEngineView.bottomAnchor
            constraintEqualToAnchor:self.container.topAnchor],
      ]];
    }

  } else {
    [self.feedHeaderConstraints addObjectsFromArray:@[
      // Anchors title label.
      [self.titleLabel.leadingAnchor
          constraintEqualToAnchor:self.container.leadingAnchor
                         constant:kTitleHorizontalMargin],
      [self.titleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:IsHomeCustomizationEnabled()
                                                ? self.container.trailingAnchor
                                                : self.managementButton
                                                      .leadingAnchor],
      [self.titleLabel.centerYAnchor
          constraintEqualToAnchor:self.container.centerYAnchor],
    ]];
  }
  [NSLayoutConstraint activateConstraints:self.feedHeaderConstraints];
}

// Anchors the segmented control.
- (void)anchorSegmentedControl {
  // Anchor segmented control.
  [self.feedHeaderConstraints addObjectsFromArray:@[
    [self.segmentedControl.centerXAnchor
        constraintEqualToAnchor:self.container.centerXAnchor],
    [self.segmentedControl.centerYAnchor
        constraintEqualToAnchor:self.container.centerYAnchor],
    [self.segmentedControl.leadingAnchor
        constraintEqualToAnchor:self.sortButton.trailingAnchor
                       constant:kButtonHorizontalMargin],
  ]];

  if (IsHomeCustomizationEnabled()) {
    [self.feedHeaderConstraints addObjectsFromArray:@[
      [self.segmentedControl.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.container.leadingAnchor],
    ]];
  } else {
    [self.feedHeaderConstraints addObjectsFromArray:@[
      [self.segmentedControl.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.managementButton.leadingAnchor
                                   constant:-kButtonHorizontalMargin],
    ]];
  }
}

// Adds views that only appear when the feed visibility is enabled.
- (void)addViewsForVisibleFeed {
  self.segmentedControl = [self createSegmentedControl];
  [self.container addSubview:self.segmentedControl];

  self.sortButton = [self createSortButton];
  // If the Follow UI update is enabled, we still create the sort button to help
  // anchor the segmented control. However, the sort button remains invisible
  // and the menu is not created.
  if (!IsFollowUIUpdateEnabled()) {
    self.sortButton.menu = [self createSortMenu];
  }

  [self.container addSubview:self.sortButton];

  if (![self.NTPDelegate isGoogleDefaultSearchEngine]) {
    [self addCustomSearchEngineView];
  }
}

// Adds views that only appear when the feed visibility is disabled.
- (void)addViewsForHiddenFeed {
  self.hiddenFeedLabel = [self createHiddenFeedLabel];
  [self.container addSubview:self.hiddenFeedLabel];
}

// Removes the subviews from the header.
- (void)resetView {
  if (self.hiddenFeedLabel) {
    [self.hiddenFeedLabel removeFromSuperview];
    self.hiddenFeedLabel = nil;
  }

  if (self.segmentedControl) {
    [self.segmentedControl removeFromSuperview];
    self.segmentedControl = nil;
  }

  if (self.sortButton) {
    [self.sortButton removeFromSuperview];
    self.sortButton = nil;
  }

  if (self.customSearchEngineView) {
    [self removeCustomSearchEngineView];
  }
}

// Handles a new feed being selected from the header.
- (void)onSegmentSelected:(UISegmentedControl*)segmentedControl {
  switch (segmentedControl.selectedSegmentIndex) {
    case static_cast<NSInteger>(FeedTypeDiscover): {
      [self.feedMetricsRecorder
                recordFeedSelected:FeedTypeDiscover
          fromPreviousFeedPosition:[self.feedControlDelegate
                                           lastVisibleFeedCardIndex]];
      [self.feedControlDelegate handleFeedSelected:FeedTypeDiscover];
      [UIView animateWithDuration:kSegmentAnimationDuration
                       animations:^{
                         self.sortButton.alpha = 0;
                       }];
      break;
    }
    case static_cast<NSInteger>(FeedTypeFollowing): {
      [self.feedMetricsRecorder
                recordFeedSelected:FeedTypeFollowing
          fromPreviousFeedPosition:[self.feedControlDelegate
                                           lastVisibleFeedCardIndex]];
      [self.feedControlDelegate handleFeedSelected:FeedTypeFollowing];
      // Only show sorting button for Following feed.
      [UIView animateWithDuration:kSegmentAnimationDuration
                       animations:^{
                         self.sortButton.alpha =
                             IsFollowUIUpdateEnabled() ? 0 : 1;
                       }];
      break;
    }
  }
}

// The title text for the Discover feed header based on user prefs.
- (NSString*)feedHeaderTitleText {
  DCHECK(![self.feedControlDelegate isFollowingFeedAvailable]);

  // Set the title based on the default search engine.
  NSString* feedHeaderTitleText =
      [self.NTPDelegate isGoogleDefaultSearchEngine]
          ? l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE)
          : l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE_NON_DSE);

  // Append the title text if the feed is turned off.
  if (![self.feedControlDelegate shouldFeedBeVisible]) {
    feedHeaderTitleText =
        [NSString stringWithFormat:@"%@ – %@", feedHeaderTitleText,
                                   l10n_util::GetNSString(
                                       IDS_IOS_DISCOVER_FEED_TITLE_OFF_LABEL)];
  }

  return feedHeaderTitleText;
}

@end
