// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/default_browser/default_browser_settings_table_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_instructions_view.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Icon image names.
NSString* const kSettingsImageName = @"settings";
NSString* const kSelectChromeStepImageName = @"chrome_icon";

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierIntro = kSectionIdentifierEnumZero,
  SectionIdentifierSteps,
  SectionIdentifierOpenSettings,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeOpenSettingsStep = kItemTypeEnumZero,
  ItemTypeTapDefaultBrowserAppStep,
  ItemTypeSelectChromeStep,
  ItemTypeOpenSettingsButton,
  ItemTypeHeaderItem,
};

}  // namespace

@interface DefaultBrowserSettingsTableViewController () {
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
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

  if (IsDefaultBrowserVideoInSettingsEnabled()) {
    [self addDefaultBrowserVideoInstructionsView];
  } else {
    [self loadModel];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  [IntentDonationHelper donateIntent:IntentType::kSetDefaultBrowser];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  // The Default Browser Settings page breaks down into 3 sections, as follows:

  // Section 1: Introduction.
  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierIntro];

  TableViewLinkHeaderFooterItem* headerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeHeaderItem];
  headerItem.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_HEADER_TEXT);
  [self.tableViewModel setHeader:headerItem
        forSectionWithIdentifier:SectionIdentifierIntro];

  // Section 2: Instructions for setting the default browser.
  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierSteps];

  TableViewLinkHeaderFooterItem* followStepsBelowItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeHeaderItem];
  followStepsBelowItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_FOLLOW_STEPS_BELOW_TEXT);
  [self.tableViewModel setHeader:followStepsBelowItem
        forSectionWithIdentifier:SectionIdentifierSteps];

  TableViewDetailIconItem* openSettingsStepItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeOpenSettingsStep];
  openSettingsStepItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_OPEN_SETTINGS_STEP);
  openSettingsStepItem.iconImage = [UIImage imageNamed:kSettingsImageName];
  [self.tableViewModel addItem:openSettingsStepItem
       toSectionWithIdentifier:SectionIdentifierSteps];

  TableViewDetailIconItem* tapDefaultBrowserAppStepItem =
      [[TableViewDetailIconItem alloc]
          initWithType:ItemTypeTapDefaultBrowserAppStep];
  tapDefaultBrowserAppStepItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_TAP_DEFAULT_BROWSER_APP_STEP);
  tapDefaultBrowserAppStepItem.iconImage =
      [UIImage imageNamed:kSettingsImageName];
  [self.tableViewModel addItem:tapDefaultBrowserAppStepItem
       toSectionWithIdentifier:SectionIdentifierSteps];

  TableViewDetailIconItem* selectChromeStepItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeSelectChromeStep];
  selectChromeStepItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SELECT_CHROME_STEP);
  selectChromeStepItem.iconImage =
      [UIImage imageNamed:kSelectChromeStepImageName];
  [self.tableViewModel addItem:selectChromeStepItem
       toSectionWithIdentifier:SectionIdentifierSteps];

  // Section 3: 'Open Chrome Settings' action
  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierOpenSettings];

  TableViewTextItem* openSettingsButtonItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeOpenSettingsButton];
  openSettingsButtonItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_OPEN_CHROME_SETTINGS_BUTTON_TEXT);
  openSettingsButtonItem.accessibilityTraits |= UIAccessibilityTraitButton;
  openSettingsButtonItem.textColor = [UIColor colorNamed:kBlueColor];
  [self.tableViewModel addItem:openSettingsButtonItem
       toSectionWithIdentifier:SectionIdentifierOpenSettings];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
}

- (void)reportBackUserAction {
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  // No-op as there are no C++ objects or observers.

  _settingsAreDismissed = YES;
}

#pragma mark UITableViewDelegate

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if (_settingsAreDismissed)
    return cell;
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeOpenSettingsStep ||
      itemType == ItemTypeTapDefaultBrowserAppStep ||
      itemType == ItemTypeSelectChromeStep) {
    [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
  }
  return cell;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  [self.tableView deselectRowAtIndexPath:indexPath animated:NO];

  if (itemType == ItemTypeOpenSettingsButton) {
    [self openSettingsButtonPressed];
  }
}

#pragma mark Private

// Responds to user action to go to default browser settings.
- (void)openSettingsButtonPressed {
  base::RecordAction(base::UserMetricsAction("Settings.DefaultBrowser"));
  base::UmaHistogramEnumeration("Settings.DefaultBrowserFromSource",
                                self.source);
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
}

// Adds default browser video instructions view as a background view.
- (void)addDefaultBrowserVideoInstructionsView {
  DefaultBrowserInstructionsView* instructionsView =
      [[DefaultBrowserInstructionsView alloc] init:NO
                                          hasSteps:YES
                                     actionHandler:self];

  self.tableView.backgroundView = [[UIView alloc] init];
  [self.tableView.backgroundView
      setBackgroundColor:[UIColor colorNamed:kGrey100Color]];
  [self.tableView addSubview:instructionsView];
  instructionsView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(instructionsView, self.tableView);
  AddSameConstraints(self.tableView.backgroundView, self.tableView);
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self openSettingsButtonPressed];
}

@end
