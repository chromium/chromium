// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/universal_opt_out_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "components/universal_optout/prefs.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitch = kSectionIdentifierEnumZero,
};

NSString* const kUniversalOptOutTableViewAccessibilityIdentifier =
    @"UniversalOptOutTableViewAccessibilityIdentifier";
NSString* const kUniversalOptOutSwitchAccessibilityIdentifier =
    @"UniversalOptOutSwitchAccessibilityIdentifier";

// Help center article URL about Universal Opt-Out.
constexpr char kUniversalOptOutLearnMoreUrl[] =
    "https://support.google.com/chrome?p=opt_out_request";

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSwitch = kItemTypeEnumZero,
  ItemTypeFooter,
};

}  // namespace

@interface UniversalOptOutTableViewController () <BooleanObserver> {
  // Pref for whether Universal Opt Out is enabled.
  PrefBackedBoolean* _universalOptOutEnabled;

  // Item for displaying Universal Opt Out switch.
  TableViewSwitchItem* _universalOptOutSwitchItem;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

@end

@implementation UniversalOptOutTableViewController

#pragma mark - Initialization

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  DCHECK(profile);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title =
        l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_UNIVERSAL_OPT_OUT);
    _universalOptOutEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:profile->GetPrefs()
                   prefName:universal_optout::prefs::kUniversalOptOutEnabled];
    _universalOptOutEnabled.observer = self;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.estimatedSectionFooterHeight = 70;
  self.tableView.accessibilityIdentifier =
      kUniversalOptOutTableViewAccessibilityIdentifier;

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitch];
  _universalOptOutSwitchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeSwitch];
  _universalOptOutSwitchItem.accessibilityIdentifier =
      kUniversalOptOutSwitchAccessibilityIdentifier;
  _universalOptOutSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_UNIVERSAL_OPT_OUT);
  _universalOptOutSwitchItem.on = _universalOptOutEnabled.value;
  _universalOptOutSwitchItem.target = self;
  _universalOptOutSwitchItem.selector = @selector(switchChanged:);
  [model addItem:_universalOptOutSwitchItem
      toSectionWithIdentifier:SectionIdentifierSwitch];

  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_UNIVERSAL_OPT_OUT_DETAILS);
  footer.urls =
      @[ [[CrURL alloc] initWithGURL:GURL(kUniversalOptOutLearnMoreUrl)] ];
  [model setFooter:footer forSectionWithIdentifier:SectionIdentifierSwitch];
}

#pragma mark - SettingsControllerProtocol

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  // Stop observable prefs.
  [_universalOptOutEnabled stop];
  _universalOptOutEnabled.observer = nil;
  _universalOptOutEnabled = nil;

  _settingsAreDismissed = YES;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView = [super tableView:tableView
                 viewForFooterInSection:section];
  TableViewLinkHeaderFooterView* footer =
      base::apple::ObjCCast<TableViewLinkHeaderFooterView>(footerView);
  if (footer) {
    footer.delegate = self;
  }
  return footerView;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (!_universalOptOutSwitchItem) {
    return;
  }
  // Update the cell.
  _universalOptOutSwitchItem.on = _universalOptOutEnabled.value;
  [self reconfigureCellsForItems:@[ _universalOptOutSwitchItem ]];
}

#pragma mark - Private

- (void)switchChanged:(UISwitch*)switchView {
  _universalOptOutEnabled.value = switchView.isOn;
  _universalOptOutSwitchItem.on = switchView.isOn;
  [self reconfigureCellsForItems:@[ _universalOptOutSwitchItem ]];
}

@end
