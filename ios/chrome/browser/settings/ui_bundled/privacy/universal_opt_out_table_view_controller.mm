// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/universal_opt_out_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/universal_optout/prefs.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/universal_optout/model/constants.h"
#import "ios/chrome/browser/web_extension/model/extension_service.h"
#import "ios/chrome/browser/web_extension/model/extension_service_factory.h"
#import "ios/chrome/browser/web_extension/model/extension_service_observer_bridge.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Size of the error icon.
constexpr CGFloat kIconSize = 24.0;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierError,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSwitch = kItemTypeEnumZero,
  ItemTypeFooter,
  ItemTypeErrorMessage,
  ItemTypeErrorLearnMore,
};

}  // namespace

NSString* const kUniversalOptOutTableViewAccessibilityIdentifier =
    @"UniversalOptOutTableViewAccessibilityIdentifier";
NSString* const kUniversalOptOutSwitchAccessibilityIdentifier =
    @"UniversalOptOutSwitchAccessibilityIdentifier";
NSString* const kUniversalOptOutErrorMessageItemAccessibilityIdentifier =
    @"UniversalOptOutErrorMessageItemAccessibilityIdentifier";
NSString* const kUniversalOptOutErrorLearnMoreItemAccessibilityIdentifier =
    @"UniversalOptOutErrorLearnMoreItemAccessibilityIdentifier";

@interface UniversalOptOutTableViewController () <BooleanObserver,
                                                  ExtensionServiceObserving> {
  // Profile.
  raw_ptr<ProfileIOS> _profile;

  // Pref for whether Universal Opt Out is enabled.
  PrefBackedBoolean* _universalOptOutEnabled;

  // Item for displaying Universal Opt Out switch.
  TableViewSwitchItem* _universalOptOutSwitchItem;

  // Item for displaying Universal Opt Out error message.
  TableViewDetailIconItem* _universalOptOutErrorMessageItem;

  // Item for displaying Universal Opt Out error "Learn more" action.
  TableViewTextItem* _universalOptOutErrorLearnMoreItem;

  // Bridge to observe ExtensionService.
  std::unique_ptr<ExtensionServiceObserverBridge>
      _extensionServiceObserverBridge;

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
    _profile = profile;
    self.title =
        l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_UNIVERSAL_OPT_OUT);
    _universalOptOutEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:profile->GetPrefs()
                   prefName:universal_optout::prefs::kUniversalOptOutEnabled];
    _universalOptOutEnabled.observer = self;

    ExtensionService* extensionService =
        ExtensionServiceFactory::GetForProfile(_profile);
    if (extensionService) {
      _extensionServiceObserverBridge =
          std::make_unique<ExtensionServiceObserverBridge>(extensionService,
                                                           self);
    }
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
  if (_settingsAreDismissed) {
    return;
  }
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

  const bool hasError = [self hasExtensionLoadError];
  if (hasError) {
    [model addSectionWithIdentifier:SectionIdentifierError];
    [model addItem:[self errorMessageItem]
        toSectionWithIdentifier:SectionIdentifierError];
    [model addItem:[self errorLearnMoreItem]
        toSectionWithIdentifier:SectionIdentifierError];
  }

  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_UNIVERSAL_OPT_OUT_DETAILS);
  footer.urls =
      @[ [[CrURL alloc] initWithGURL:GURL(kUniversalOptOutLearnMoreURL)] ];
  NSInteger footerSectionIdentifier =
      hasError ? SectionIdentifierError : SectionIdentifierSwitch;
  [model setFooter:footer forSectionWithIdentifier:footerSectionIdentifier];
}

#pragma mark - SettingsControllerProtocol

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  // Stop observable prefs.
  [_universalOptOutEnabled stop];
  _universalOptOutEnabled.observer = nil;
  _universalOptOutEnabled = nil;

  _extensionServiceObserverBridge.reset();
  _profile = nullptr;

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

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  return itemType == ItemTypeErrorLearnMore;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  if (itemType == ItemTypeErrorLearnMore) {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    [self didTapLearnMore];
  }
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

#pragma mark - ExtensionServiceObserving

- (void)extensionService:(ExtensionService*)service
    didChangeExtensionLoadError:(bool)hasLoadError {
  if (!self.viewLoaded || _settingsAreDismissed) {
    return;
  }
  [self reloadData];
}

#pragma mark - Private

// Opens the Universal Opt Out learn more URL in a new tab.
- (void)didTapLearnMore {
  DCHECK(self.sceneHandler);
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:GURL(kUniversalOptOutLearnMoreURL)];
  [self.sceneHandler closePresentedViewsAndOpenURL:command];
}

// Returns whether the extension service reports a load error.
- (bool)hasExtensionLoadError {
  if (!_profile) {
    return false;
  }
  ExtensionService* extensionService =
      ExtensionServiceFactory::GetForProfile(_profile);
  return extensionService && extensionService->HasLoadError();
}

// Returns the error message item, lazily initialized.
- (TableViewDetailIconItem*)errorMessageItem {
  if (!_universalOptOutErrorMessageItem) {
    _universalOptOutErrorMessageItem =
        [[TableViewDetailIconItem alloc] initWithType:ItemTypeErrorMessage];
    _universalOptOutErrorMessageItem.accessibilityIdentifier =
        kUniversalOptOutErrorMessageItemAccessibilityIdentifier;
    _universalOptOutErrorMessageItem.verticalAlignment =
        UIStackViewAlignmentTop;
    _universalOptOutErrorMessageItem.iconImage =
        SymbolWithPointSize(SymbolInfoCircle, kIconSize);
    _universalOptOutErrorMessageItem.iconTintColor =
        [UIColor colorNamed:kBlueColor];
    int messageId = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
                        ? IDS_IOS_OPTIONS_ENABLE_UNIVERSAL_OPT_OUT_ERROR_IPAD
                        : IDS_IOS_OPTIONS_ENABLE_UNIVERSAL_OPT_OUT_ERROR_IPHONE;
    _universalOptOutErrorMessageItem.text = l10n_util::GetNSString(messageId);
    _universalOptOutErrorMessageItem.textFont =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    _universalOptOutErrorMessageItem.textColor =
        [UIColor colorNamed:kTextSecondaryColor];
    _universalOptOutErrorMessageItem.textNumberOfLines = 0;
    _universalOptOutErrorMessageItem.selectionStyle =
        UITableViewCellSelectionStyleNone;
  }
  return _universalOptOutErrorMessageItem;
}

// Returns the error "Learn more" item, lazily initialized.
- (TableViewTextItem*)errorLearnMoreItem {
  if (!_universalOptOutErrorLearnMoreItem) {
    _universalOptOutErrorLearnMoreItem =
        [[TableViewTextItem alloc] initWithType:ItemTypeErrorLearnMore];
    _universalOptOutErrorLearnMoreItem.accessibilityIdentifier =
        kUniversalOptOutErrorLearnMoreItemAccessibilityIdentifier;
    _universalOptOutErrorLearnMoreItem.reservesLeadingSpace = YES;
    _universalOptOutErrorLearnMoreItem.text = l10n_util::GetNSString(
        IDS_IOS_OPTIONS_ENABLE_UNIVERSAL_OPT_OUT_ERROR_LEARN_MORE);
    _universalOptOutErrorLearnMoreItem.textColor =
        [UIColor colorNamed:kBlueColor];
    _universalOptOutErrorLearnMoreItem.accessibilityTraits =
        UIAccessibilityTraitButton;
  }
  return _universalOptOutErrorLearnMoreItem;
}

// Updates the preference and switch state when the user toggles the switch.
- (void)switchChanged:(UISwitch*)switchView {
  _universalOptOutEnabled.value = switchView.isOn;
  _universalOptOutSwitchItem.on = switchView.isOn;

  if (switchView.isOn) {
    base::RecordAction(
        base::UserMetricsAction("Privacy.UniversalOptOut.SettingsToggleOn"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Privacy.UniversalOptOut.SettingsToggleOff"));
  }
  [self reconfigureCellsForItems:@[ _universalOptOutSwitchItem ]];
}

@end
