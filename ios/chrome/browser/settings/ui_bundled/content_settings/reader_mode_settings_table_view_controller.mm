// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/content_settings/reader_mode_settings_table_view_controller.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "components/dom_distiller/ios/distilled_page_prefs_observer_bridge.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSuggestion = kSectionIdentifierEnumZero,
  SectionIdentifierLinks,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSettingsShowSuggestion = kItemTypeEnumZero,
  ItemTypeSettingsShowHyperlinks,
};

}  // namespace

@interface ReaderModeSettingsTableViewController () <
    BooleanObserver,
    DistilledPagePrefsObserving>

@property(nonatomic, strong, readonly) PrefBackedBoolean* showSuggestionEnabled;
@property(nonatomic, strong) TableViewSwitchItem* showSuggestionItem;
@property(nonatomic, strong) TableViewSwitchItem* showHyperlinksItem;

@end

@implementation ReaderModeSettingsTableViewController {
  raw_ptr<dom_distiller::DistilledPagePrefs> _distilledPagePrefs;
  std::unique_ptr<DistilledPagePrefsObserverBridge> _observerBridge;
}

- (instancetype)initWithDistilledPagePrefs:
                    (dom_distiller::DistilledPagePrefs*)distilledPagePrefs
                               prefService:(PrefService*)prefService {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    CHECK(distilledPagePrefs);
    CHECK(prefService);
    _distilledPagePrefs = distilledPagePrefs;
    self.title =
        l10n_util::GetNSString(IDS_IOS_READER_MODE_CONTENT_SETTINGS_TITLE);

    _showSuggestionEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:prefs::kIosReaderModeShowAvailability];
    [_showSuggestionEnabled setObserver:self];

    _observerBridge = std::make_unique<DistilledPagePrefsObserverBridge>(
        self, distilledPagePrefs);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSuggestion];
  [model addItem:[self showSuggestionItem]
      toSectionWithIdentifier:SectionIdentifierSuggestion];

  [model addSectionWithIdentifier:SectionIdentifierLinks];
  [model addItem:[self showHyperlinksItem]
      toSectionWithIdentifier:SectionIdentifierLinks];
}

#pragma mark - Items

- (TableViewSwitchItem*)showSuggestionItem {
  if (!_showSuggestionItem) {
    _showSuggestionItem = [[TableViewSwitchItem alloc]
        initWithType:ItemTypeSettingsShowSuggestion];
    _showSuggestionItem.text =
        l10n_util::GetNSString(IDS_IOS_READING_MODE_SHOW_SUGGESTION_TITLE);
    _showSuggestionItem.detailText =
        l10n_util::GetNSString(IDS_IOS_READING_MODE_SETTING_DESCRIPTION);
    _showSuggestionItem.on = [self.showSuggestionEnabled value];
    _showSuggestionItem.accessibilityIdentifier =
        kReaderModeSettingsShowSuggestionAccessibilityIdentifier;
    _showSuggestionItem.target = self;
    _showSuggestionItem.selector = @selector(showSuggestionSwitchToggled:);
  }
  return _showSuggestionItem;
}

- (TableViewSwitchItem*)showHyperlinksItem {
  if (!_showHyperlinksItem) {
    _showHyperlinksItem = [[TableViewSwitchItem alloc]
        initWithType:ItemTypeSettingsShowHyperlinks];
    _showHyperlinksItem.text =
        l10n_util::GetNSString(IDS_IOS_READING_MODE_SHOW_HYPERLINKS_TITLE);
    _showHyperlinksItem.detailText = l10n_util::GetNSString(
        IDS_IOS_READING_MODE_SHOW_HYPERLINKS_DESCRIPTION);
    _showHyperlinksItem.on = _distilledPagePrefs->GetLinksEnabled();
    _showHyperlinksItem.accessibilityIdentifier =
        kReaderModeSettingsShowHyperlinksAccessibilityIdentifier;
    _showHyperlinksItem.target = self;
    _showHyperlinksItem.selector = @selector(showHyperlinksSwitchToggled:);
  }
  return _showHyperlinksItem;
}

#pragma mark - Switch Actions

- (void)showSuggestionSwitchToggled:(UISwitch*)sender {
  BOOL newSwitchValue = sender.isOn;
  self.showSuggestionItem.on = newSwitchValue;
  [self.showSuggestionEnabled setValue:newSwitchValue];
}

- (void)showHyperlinksSwitchToggled:(UISwitch*)sender {
  BOOL newSwitchValue = sender.isOn;
  self.showHyperlinksItem.on = newSwitchValue;
  _distilledPagePrefs->SetLinksEnabled(newSwitchValue);
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == self.showSuggestionEnabled) {
    self.showSuggestionItem.on = [self.showSuggestionEnabled value];
    [self reconfigureCellsForItems:@[ self.showSuggestionItem ]];
  }
}

#pragma mark - DistilledPagePrefsObserving

- (void)onChangeLinksEnabled:(bool)enabled {
  self.showHyperlinksItem.on = enabled;
  [self reconfigureCellsForItems:@[ self.showHyperlinksItem ]];
}

@end
