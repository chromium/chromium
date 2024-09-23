// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/default_browser/default_browser_settings_table_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/strcat.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
const char kDefaultBrowserSettingsPageUsageHistogram[] =
    "IOS.DefaultBrowserSettingsPageUsage";

// Values of the UMA IOS.DefaultBrowserSettingsPageUsage.{Source} histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange
enum class DefaultBrowserSettingsPageUsage {
  kOpenIOSSettings = 0,
  kDismiss,
  kDisplay,
  kMaxValue = kDisplay,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)
}  // namespace

@interface DefaultBrowserSettingsTableViewController () {
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;

  // Whether the user visited the iOS Default Browser settings page.
  BOOL _defaultBrowserSettingsVisited;
}
@end

@implementation DefaultBrowserSettingsTableViewController

- (instancetype)init {
  UITableViewStyle style = ChromeTableViewStyle();
  return [super initWithStyle:style];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_DEFAULT_BROWSER);
  self.shouldHideDoneButton = YES;
  self.tableView.accessibilityIdentifier = kDefaultBrowserSettingsTableViewId;

  [self addDefaultBrowserVideoInstructionsView];

  [self recordMetrics:DefaultBrowserSettingsPageUsage::kDisplay];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  [IntentDonationHelper donateIntent:IntentType::kSetDefaultBrowser];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
}

- (void)reportBackUserAction {
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  // Record app settings has been dismissed without opening iOS settings. As
  // opening iOS settings doesn't dismiss the view users can open settings and
  // then return to the app and dismiss the view manually. Don't record dismiss
  // action that that case.
  if (!_defaultBrowserSettingsVisited) {
    [self recordMetrics:DefaultBrowserSettingsPageUsage::kDismiss];
  }

  // No-op as there are no C++ objects or observers.

  _settingsAreDismissed = YES;
}

#pragma mark Private

// Responds to user action to go to default browser iOS settings.
- (void)openSettingsButtonPressed {
  // Record iOS settings opened only once per app settings display. As
  // opening iOS settings doesn't dismiss the view users can open iOS settings
  // multiple times. Check that it is the first instance before recording.
  if (!_defaultBrowserSettingsVisited) {
    [self recordMetrics:DefaultBrowserSettingsPageUsage::kOpenIOSSettings];
  }

  RecordDefaultBrowserPromoLastAction(
      IOSDefaultBrowserPromoAction::kActionButton);
  base::RecordAction(base::UserMetricsAction("Settings.DefaultBrowser"));
  base::UmaHistogramEnumeration("Settings.DefaultBrowserFromSource",
                                self.source);

  _defaultBrowserSettingsVisited = YES;

  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
}

// Adds default browser video instructions view as a background view.
- (void)addDefaultBrowserVideoInstructionsView {
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];
  DefaultBrowserInstructionsView* instructionsView =
      [[DefaultBrowserInstructionsView alloc] initWithDismissButton:NO
                                                   hasRemindMeLater:NO
                                                           hasSteps:YES
                                                      actionHandler:self
                                          alertScreenViewController:alertScreen
                                                          titleText:nil];

  self.tableView.backgroundView = [[UIView alloc] init];
  [self.tableView.backgroundView
      setBackgroundColor:[UIColor colorNamed:kGrey100Color]];
  [self.tableView addSubview:instructionsView];
  instructionsView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(instructionsView, self.tableView);
  AddSameConstraints(self.tableView.backgroundView, self.tableView);
}

- (void)recordMetrics:(DefaultBrowserSettingsPageUsage)usage {
  base::UmaHistogramEnumeration(kDefaultBrowserSettingsPageUsageHistogram,
                                usage);

  std::string perSourceHistogram =
      base::StrCat({kDefaultBrowserSettingsPageUsageHistogram, ".",
                    DefaultBrowserSettingsPageSourceToString(self.source)});
  base::UmaHistogramEnumeration(perSourceHistogram, usage);
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self openSettingsButtonPressed];
}

@end
