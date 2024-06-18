// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/about_chrome_table_view_controller.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/terms_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/version_item.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierLinks = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeLinksCredits = kItemTypeEnumZero,
  ItemTypeLinksTerms,
  ItemTypeLinksPrivacy,
  ItemTypeVersion,
};

const CGFloat kDefaultHeight = 70;

}  // namespace

@interface AboutChromeTableViewController ()<VersionFooterDelegate>
@end

@implementation AboutChromeTableViewController

#pragma mark - Public

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_ABOUT_PRODUCT_NAME);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = kDefaultHeight;
  self.tableView.sectionFooterHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedSectionFooterHeight = kDefaultHeight;
  self.styler.cellTitleColor = [UIColor colorNamed:kTextPrimaryColor];
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierLinks];

  TableViewDetailTextItem* credits =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeLinksCredits];
  credits.text = l10n_util::GetNSString(IDS_IOS_OPEN_SOURCE_LICENSES);
  credits.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  credits.accessibilityTraits = UIAccessibilityTraitButton;
  credits.accessibilityIdentifier = kSettingsOpenSourceLicencesCellId;
  [model addItem:credits toSectionWithIdentifier:SectionIdentifierLinks];

  TableViewDetailTextItem* terms =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeLinksTerms];
  terms.text = l10n_util::GetNSString(IDS_IOS_TERMS_OF_SERVICE);
  terms.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  terms.accessibilityTraits = UIAccessibilityTraitButton;
  terms.accessibilityIdentifier = kSettingsTOSCellId;
  [model addItem:terms toSectionWithIdentifier:SectionIdentifierLinks];

  TableViewDetailTextItem* privacy =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeLinksPrivacy];
  privacy.text = l10n_util::GetNSString(IDS_IOS_PRIVACY_POLICY);
  privacy.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  privacy.accessibilityTraits = UIAccessibilityTraitButton;
  privacy.accessibilityIdentifier = kSettingsPrivacyNoticeCellId;
  [model addItem:privacy toSectionWithIdentifier:SectionIdentifierLinks];

  VersionItem* version = [[VersionItem alloc] initWithType:ItemTypeVersion];
  version.text = [self versionDescriptionString];
  version.accessibilityTraits = UIAccessibilityTraitButton;
  [model setFooter:version forSectionWithIdentifier:SectionIdentifierLinks];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAboutSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAboutSettingsBack"));
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footer = [super tableView:tableView viewForFooterInSection:section];
  VersionFooter* versionFooter =
      base::apple::ObjCCastStrict<VersionFooter>(footer);
  versionFooter.delegate = self;
  return footer;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeLinksCredits:
      [self openURL:GURL(kChromeUICreditsURL)];
      break;
    case ItemTypeLinksTerms:
      [self openURL:GetUnifiedTermsOfServiceURL(false)];
      break;
    case ItemTypeLinksPrivacy:
      [self openURL:GURL(l10n_util::GetStringUTF8(IDS_IOS_PRIVACY_POLICY_URL))];
      break;
    case ItemTypeVersion:
      // Version is a footer, it is not interactable.
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

#pragma mark - VersionFooterDelegate

- (void)didTapVersionFooter:(VersionFooter*)footer {
  StoreTextInPasteboard([self versionOnlyString]);

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  NSString* messageText = l10n_util::GetNSString(IDS_IOS_VERSION_COPIED);
  MDCSnackbarMessage* message = CreateSnackbarMessage(messageText);
  message.category = @"version copied";
  [self.snackbarHandler showSnackbarMessage:message bottomOffset:0];
}

#pragma mark - Private methods

- (void)openURL:(GURL)URL {
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.applicationHandler closeSettingsUIAndOpenURL:command];
}

- (std::string)versionString {
  std::string versionString(version_info::GetVersionNumber());
  std::string versionStringModifier = GetChannelString();
  if (!versionStringModifier.empty()) {
    versionString = versionString + " " + versionStringModifier;
  }
  return versionString;
}

- (NSString*)versionDescriptionString {
  return l10n_util::GetNSStringF(IDS_IOS_VERSION,
                                 base::UTF8ToUTF16([self versionString]));
}

- (NSString*)versionOnlyString {
  return base::SysUTF8ToNSString([self versionString]);
}

@end
