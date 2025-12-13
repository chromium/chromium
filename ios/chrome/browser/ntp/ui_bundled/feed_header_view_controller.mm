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
// Font size and height for the custom search engine label.
const CGFloat kCustomSearchEngineLabelFontSize = 13;
const CGFloat kCustomSearchEngineLabelHeight = 18;
// The height of the header container.
const CGFloat kDiscoverFeedHeaderHeight = 30;
// Max size that the Title and Segmented Control fonts will scale to.
const CGFloat kMaxFontSize = 24;

}  // namespace

@interface FeedHeaderViewController ()

// View containing elements of the header. Handles header sizing.
@property(nonatomic, strong) UIView* container;

// Title label element for the feed.
@property(nonatomic, strong) UILabel* titleLabel;

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
}

#pragma mark - Public

- (CGFloat)feedHeaderHeight {
  return kDiscoverFeedHeaderHeight;
}

- (void)updateForDefaultSearchEngineChanged {
  if (!self.viewLoaded) {
    return;
  }
  [self.titleLabel removeFromSuperview];
  self.titleLabel = [self createTitleLabel];
  [self.container addSubview:self.titleLabel];
  if ([self.NTPDelegate isGoogleDefaultSearchEngine]) {
    [self removeCustomSearchEngineView];
  } else {
    [self addCustomSearchEngineView];
  }
  [self applyHeaderConstraints];
}

#pragma mark - Private

- (void)configureHeaderViews {
  self.titleLabel = [self createTitleLabel];
  [self.container addSubview:self.titleLabel];
  if (![self.NTPDelegate isGoogleDefaultSearchEngine]) {
    [self addCustomSearchEngineView];
  }
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
  titleLabel.text = l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE);
  return titleLabel;
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
  [self anchorTitleLabel];
  if (![self.NTPDelegate isGoogleDefaultSearchEngine]) {
    [self anchorCustomSearchEngineView];
  }
  [NSLayoutConstraint activateConstraints:self.feedHeaderConstraints];
}

// Anchors feed header container.
- (void)anchorContainer {
  CGFloat feedHeaderHeight = [self feedHeaderHeight];
  // Anchor container.
  [self.feedHeaderConstraints addObjectsFromArray:@[
    // Anchor container and menu button.
    [self.view.heightAnchor constraintEqualToConstant:feedHeaderHeight],
    [self.container.heightAnchor constraintEqualToConstant:feedHeaderHeight],
    [self.container.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.container.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.container.widthAnchor constraintEqualToAnchor:self.view.widthAnchor],
  ]];
}

// Anchors the title label that should be shown when the following feed is not
// available.
- (void)anchorTitleLabel {
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

@end
