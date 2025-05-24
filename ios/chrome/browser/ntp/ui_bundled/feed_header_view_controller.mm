// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/feed_header_view_controller.h"

#import "ios/chrome/browser/content_suggestions/ui_bundled/ntp_home_constant.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ntp/ui_bundled/discover_feed_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_control_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
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
// The height of the header container.
const CGFloat kDiscoverFeedHeaderHeight = 30;

const CGFloat kCustomSearchEngineLabelHeight = 18;
// * Values below are exclusive to Web Channels.
// The height and width of the header menu button. Based on the default
// UISegmentedControl height.
const CGFloat kButtonSize = 28;
// Duration of the fade animation for elements that toggle when switching feeds.
const CGFloat kSegmentAnimationDuration = 0.3;
// Padding on top of the header.
const CGFloat kTopVerticalPadding = 15;

// Max size that the Title and Segmented Control fonts will scale to.
const CGFloat kMaxFontSize = 24;

// The size of feed symbol images.
NSInteger kFeedSymbolPointSize = 17;

}  // namespace

@interface FeedHeaderViewController ()

// View containing elements of the header. Handles header sizing.
@property(nonatomic, strong) UIView* container;

// Title label element for the feed.
@property(nonatomic, strong) UILabel* titleLabel;

// Button for sorting feed content. Only used for Following feed.
@property(nonatomic, strong) UIButton* sortButton;

// Segmented control for toggling between the two feeds.
@property(nonatomic, strong) UISegmentedControl* segmentedControl;

// The view informing the user that the feed is powered by Google if they don't
// have Google as their default search engine.
@property(nonatomic, strong) UILabel* customSearchEngineView;

// The constraints for the currently visible components of the header.
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* feedHeaderConstraints;

@end

@implementation FeedHeaderViewController

- (instancetype)init {
  return [super initWithNibName:nil bundle:nil];
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

  [self configureHeaderViews];

  [self.view addSubview:self.container];
  [self applyHeaderConstraints];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitPreferredContentSizeCategory.class ]);
    [self registerForTraitChanges:traits
                       withTarget:self
                           action:@selector(updateFonts)];
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
    [self updateFonts];
  }
}
#endif

#pragma mark - Public

- (CGFloat)feedHeaderHeight {
  if ([self.feedControlDelegate isFollowingFeedAvailable] ||
      !ShouldRemoveDiscoverLabel(
          [self.NTPDelegate isGoogleDefaultSearchEngine])) {
    return kDiscoverFeedHeaderHeight;
  }
  return 0;
}

- (void)updateForDefaultSearchEngineChanged {
  if (!self.viewLoaded) {
    return;
  }
  BOOL isGoogleDefaultSearchEngine =
      [self.NTPDelegate isGoogleDefaultSearchEngine];
  if (![self.feedControlDelegate isFollowingFeedAvailable]) {
    [self.titleLabel removeFromSuperview];
    self.titleLabel = nil;
    if (!ShouldRemoveDiscoverLabel(isGoogleDefaultSearchEngine)) {
      self.titleLabel = [self createTitleLabel];
      [self.container addSubview:self.titleLabel];
    }
  }
  if (isGoogleDefaultSearchEngine) {
    [self removeCustomSearchEngineView];
  } else {
    [self addCustomSearchEngineView];
  }
  [self applyHeaderConstraints];
}

- (void)updateForFeedVisibilityChanged {
  if (![self.feedControlDelegate isFollowingFeedAvailable]) {
    [self.titleLabel setText:[self feedHeaderTitleText]];
    [self.titleLabel setNeedsDisplay];
    return;
  }

  [self resetView];
  [self addViewsForFeed];
  [self applyHeaderConstraints];
}

- (void)updateForFollowingFeedVisibilityChanged {
  [self resetView];
  [self.titleLabel removeFromSuperview];

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
    [self addViewsForFeed];
  } else if (!ShouldRemoveDiscoverLabel(
                 [self.NTPDelegate isGoogleDefaultSearchEngine])) {
    self.titleLabel = [self createTitleLabel];
    [self.container addSubview:self.titleLabel];
  }
  if (![self.NTPDelegate isGoogleDefaultSearchEngine]) {
    [self addCustomSearchEngineView];
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

// Configures and returns the feed header's sorting button.
- (UIButton*)createSortButton {
  CHECK([self.feedControlDelegate isFollowingFeedAvailable]);

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
  titleLabel.font = PreferredFontForTextStyle(
      UIFontTextStyleFootnote, UIFontWeightSemibold, kMaxFontSize);
  titleLabel.adjustsFontForContentSizeCategory = YES;
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

// Updates the font and color of the segmented control header to adapt to the
// current dynamic sizing.
- (void)updateSegmentedControlFont:(UISegmentedControl*)segmentedControl {
  UIFont* font = PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                           UIFontWeightMedium, kMaxFontSize);
  NSDictionary* normalAttributes = [NSDictionary
      dictionaryWithObjectsAndKeys:font, NSFontAttributeName,
                                   [UIColor colorNamed:kTextSecondaryColor],
                                   NSForegroundColorAttributeName, nil];
  [segmentedControl setTitleTextAttributes:normalAttributes
                                  forState:UIControlStateNormal];
  NSDictionary* selectedAttributes = [NSDictionary
      dictionaryWithObjectsAndKeys:font, NSFontAttributeName,
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

  [self anchorContainer];
  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    [self anchorSegmentedControlAndSortButton];
  } else {
    [self anchorTitleLabel];
  }
  if (![self.NTPDelegate isGoogleDefaultSearchEngine]) {
    [self anchorCustomSearchEngineView];
  }
  [NSLayoutConstraint activateConstraints:self.feedHeaderConstraints];
}

// Anchors feed header container.
- (void)anchorContainer {
  CGFloat totalHeaderHeight = [self feedHeaderHeight];
  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    totalHeaderHeight += kTopVerticalPadding;
    if (![self.NTPDelegate isGoogleDefaultSearchEngine]) {
      totalHeaderHeight += kCustomSearchEngineLabelHeight;
    }
  }
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
}

// Anchors feed header elements that should be shown when following feed is
// available.
- (void)anchorSegmentedControlAndSortButton {
  CHECK([self.feedControlDelegate isFollowingFeedAvailable]);
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
}

// Anchors the title label that should be shown when the following feed is not
// available.
- (void)anchorTitleLabel {
  CHECK(![self.feedControlDelegate isFollowingFeedAvailable]);
  if (ShouldRemoveDiscoverLabel(
          [self.NTPDelegate isGoogleDefaultSearchEngine])) {
    return;
  }
  [self.feedHeaderConstraints addObjectsFromArray:@[
    [self.titleLabel.leadingAnchor
        constraintEqualToAnchor:self.container.leadingAnchor
                       constant:kTitleHorizontalMargin],
    [self.titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.container.trailingAnchor],
    [self.titleLabel.bottomAnchor
        constraintEqualToAnchor:self.container.bottomAnchor]
  ]];
}

// Anchors the cusstom search engine view if default search engine is NOT
// google.
- (void)anchorCustomSearchEngineView {
  CHECK(![self.NTPDelegate isGoogleDefaultSearchEngine]);
  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    [self.feedHeaderConstraints addObjectsFromArray:@[
      // Anchors custom search engine view.
      [self.customSearchEngineView.widthAnchor
          constraintEqualToAnchor:self.view.widthAnchor],
      [self.customSearchEngineView.heightAnchor
          constraintEqualToConstant:kCustomSearchEngineLabelHeight],
      [self.customSearchEngineView.bottomAnchor
          constraintEqualToAnchor:self.container.topAnchor],
    ]];
  } else {
    [self.feedHeaderConstraints addObjectsFromArray:@[
      [self.customSearchEngineView.heightAnchor
          constraintEqualToConstant:kCustomSearchEngineLabelHeight],
      [self.customSearchEngineView.trailingAnchor
          constraintEqualToAnchor:self.container.trailingAnchor
                         constant:-kButtonHorizontalMargin],
      [self.customSearchEngineView.bottomAnchor
          constraintEqualToAnchor:self.container.bottomAnchor]
    ]];
  }
}

// Adds subviews for the feed.
- (void)addViewsForFeed {
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

// Removes the subviews from the header.
- (void)resetView {
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
  return l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE);
}

// Updates fonts when the preferred content size class changes.
- (void)updateFonts {
  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    [self updateSegmentedControlFont:self.segmentedControl];
  }
}

@end
