// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_view_controller.h"

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/timer/elapsed_timer.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_view_controller_audience.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Duration for fading in/out views.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(200);

}  // namespace

@interface HistorySyncViewController ()

@property(nonatomic, strong) ActivityOverlayView* overlay;

@end

@implementation HistorySyncViewController {
  // Tracks the duration between when the view appeared with hidden buttons
  // and when the buttons are shown.
  std::unique_ptr<base::ElapsedTimer> _userVisibileLatency;
}

@dynamic delegate;

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kHistorySyncViewAccessibilityIdentifier;
  self.shouldHideBanner = YES;
  self.scrollToEndMandatory = YES;
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);
  self.headerImageType = PromoStyleImageType::kAvatar;
  self.headerBackgroundImage =
      [UIImage imageNamed:@"history_sync_opt_in_background"];
  self.titleText = l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_TITLE);
  self.subtitleText = l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_SUBTITLE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_PRIMARY_ACTION);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_SECONDARY_ACTION);
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  if (base::FeatureList::IsEnabled(
          switches::kMinorModeRestrictionsForHistorySyncOptIn)) {
    // Hide the buttons, title, and subtitle only if button visibility has not
    // been updated.
    if (self.actionButtonsVisibility == ActionButtonsVisibility::kDefault) {
      self.actionButtonsVisibility = ActionButtonsVisibility::kHidden;

      // Hide the title and subtitles.
      self.titleLabel.alpha = 0;
      self.subtitleLabel.alpha = 0;

      // Start the spinner.
      [self.view addSubview:self.overlay];
      AddSameConstraints(self.view, self.overlay);
      [self.overlay.indicator startAnimating];

      // Record availability metrics and start the latency timer.
      base::UmaHistogramBoolean(
          "Signin.AccountCapabilities.ImmediatelyAvailable", false);
      _userVisibileLatency = std::make_unique<base::ElapsedTimer>();

      // Notify audience.
      [self.audience viewAppearedWithHiddenButtons];
    }
  }
}

- (ActivityOverlayView*)overlay {
  if (!_overlay) {
    _overlay = [[ActivityOverlayView alloc] init];
    _overlay.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _overlay;
}

#pragma mark - HistorySyncConsumer

- (void)setPrimaryIdentityAvatarImage:(UIImage*)primaryIdentityAvatarImage {
  self.headerImage = primaryIdentityAvatarImage;
}

- (void)setPrimaryIdentityAvatarAccessibilityLabel:
    (NSString*)primaryIdentityAvatarAccessibilityLabel {
  self.headerAccessibilityLabel = primaryIdentityAvatarAccessibilityLabel;
}

- (void)setFooterText:(NSString*)text {
  self.disclaimerText = text;
}

- (void)displayButtonsWithRestrictionStatus:(BOOL)isRestricted {
  if (base::FeatureList::IsEnabled(
          switches::kMinorModeRestrictionsForHistorySyncOptIn)) {
    if (self.actionButtonsVisibility == ActionButtonsVisibility::kDefault) {
      // Buttons are updated without ever being hidden when capabilities are
      // immediately available.
      base::UmaHistogramBoolean(
          "Signin.AccountCapabilities.ImmediatelyAvailable", true);
      base::UmaHistogramTimes("Signin.AccountCapabilities.UserVisibleLatency",
                              base::Seconds(0));
    } else if (self.actionButtonsVisibility ==
               ActionButtonsVisibility::kHidden) {
      // Fade out the spinner while fading in the title and subtitle.
      // The buttons will be shown simultaneously.
      __weak __typeof(self) weakSelf = self;
      [UIView animateWithDuration:kAnimationDuration.InSecondsF()
          animations:^{
            weakSelf.overlay.alpha = 0;
            // titleLabel is created on-demand and should not be
            // created with an empty titleText.
            if (weakSelf.titleText) {
              weakSelf.titleLabel.alpha = 1;
            }
            weakSelf.subtitleLabel.alpha = 1;
          }
          completion:^(BOOL finished) {
            [weakSelf.overlay removeFromSuperview];
            [weakSelf recordLatencyMetrics];
          }];
    }

    // Show action buttons.
    self.actionButtonsVisibility =
        isRestricted ? ActionButtonsVisibility::kEquallyWeightedButtonShown
                     : ActionButtonsVisibility::kRegularButtonsShown;
  }
}

#pragma mark - HistorySyncConsumer

- (void)recordLatencyMetrics {
  if (_userVisibileLatency) {
    base::TimeDelta elapsed = _userVisibileLatency->Elapsed();
    base::UmaHistogramTimes("Signin.AccountCapabilities.UserVisibleLatency",
                            elapsed);
    base::UmaHistogramTimes("Signin.AccountCapabilities.FetchLatency", elapsed);
  }
}

@end
