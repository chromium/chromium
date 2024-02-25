// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/voice_search_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/voice/model/speech_input_locale_config.h"
#import "ios/chrome/browser/voice/model/voice_search_prefs.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierTTS = kSectionIdentifierEnumZero,
  SectionIdentifierLanguages,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeTTSEnabled = kItemTypeEnumZero,
  ItemTypeLanguagesLanguageOption,
};

}  // namespace

@interface VoiceSearchTableViewController () <PrefObserverDelegate> {
  raw_ptr<PrefService> _prefs;  // weak
  StringPrefMember _selectedLanguage;
  BooleanPrefMember _ttsEnabled;

  // Pref observer to track changes to the voice search locale pref.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
}
// Updates all cells to check the selected language and uncheck all the other.
- (void)markAsCheckedLanguageAtIndex:(NSUInteger)index;

// Returns YES if the current language supports TTS.
- (BOOL)currentLanguageSupportsTTS;
@end

@implementation VoiceSearchTableViewController

- (instancetype)initWithPrefs:(PrefService*)prefs {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_VOICE_SEARCH_SETTING_TITLE);
    _prefs = prefs;
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefChangeRegistrar.Init(prefs);

    _selectedLanguage.Init(prefs::kVoiceSearchLocale, _prefs);
    _ttsEnabled.Init(prefs::kVoiceSearchTTS, _prefs);

    _prefObserverBridge->ObserveChangesForPreference(prefs::kVoiceSearchLocale,
                                                     &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(prefs::kVoiceSearchTTS,
                                                     &_prefChangeRegistrar);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.rowHeight = UITableViewAutomaticDimension;

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  // TTS section.
  [model addSectionWithIdentifier:SectionIdentifierTTS];
  TableViewSwitchItem* tts =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeTTSEnabled];
  tts.text = l10n_util::GetNSString(IDS_IOS_VOICE_SEARCH_SETTING_TTS);
  BOOL enabled = [self currentLanguageSupportsTTS];
  tts.on = enabled && _ttsEnabled.GetValue();
  tts.enabled = enabled;
  [model addItem:tts toSectionWithIdentifier:SectionIdentifierTTS];

  // Variables used to populate the languages section.
  voice::SpeechInputLocaleConfig* localeConfig =
      voice::SpeechInputLocaleConfig::GetInstance();
  const std::vector<voice::SpeechInputLocale>& locales =
      localeConfig->GetAvailableLocales();
  std::string selectedLocaleCode = _selectedLanguage.GetValue();

  // Section with the list of voice search languages.
  [model addSectionWithIdentifier:SectionIdentifierLanguages];
  // Add default locale option.  Using an empty string for the voice search
  // locale pref indicates using the default locale.
  TableViewDetailTextItem* defaultItem = [[TableViewDetailTextItem alloc]
      initWithType:ItemTypeLanguagesLanguageOption];
  defaultItem.text =
      l10n_util::GetNSStringF(IDS_IOS_VOICE_SEARCH_SETTINGS_DEFAULT_LOCALE,
                              localeConfig->GetDefaultLocale().display_name);
  defaultItem.accessoryType = selectedLocaleCode.empty()
                                  ? UITableViewCellAccessoryCheckmark
                                  : UITableViewCellAccessoryNone;
  [model addItem:defaultItem
      toSectionWithIdentifier:SectionIdentifierLanguages];
  // Add locale list.
  for (NSUInteger ii = 0; ii < locales.size(); ii++) {
    voice::SpeechInputLocale locale = locales[ii];
    NSString* languageName = base::SysUTF16ToNSString(locale.display_name);
    BOOL checked = (locale.code == selectedLocaleCode);

    TableViewDetailTextItem* languageItem = [[TableViewDetailTextItem alloc]
        initWithType:ItemTypeLanguagesLanguageOption];
    languageItem.text = languageName;
    languageItem.accessoryType = checked ? UITableViewCellAccessoryCheckmark
                                         : UITableViewCellAccessoryNone;
    [model addItem:languageItem
        toSectionWithIdentifier:SectionIdentifierLanguages];
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileVoiceSearchSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileVoiceSearchSettingsBack"));
}

#pragma mark - UITableViewDelegate

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [super tableView:tableView cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeTTSEnabled) {
    // Have the switch send a message on UIControlEventValueChanged.
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    switchCell.selectionStyle = UITableViewCellSelectionStyleNone;
    [switchCell.switchView addTarget:self
                              action:@selector(ttsToggled:)
                    forControlEvents:UIControlEventValueChanged];
  }

  return cell;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  TableViewModel* model = self.tableViewModel;
  TableViewItem* item = [model itemAtIndexPath:indexPath];

  // Language options.
  if (item.type == ItemTypeLanguagesLanguageOption) {
    NSInteger index = [model indexInItemTypeForIndexPath:indexPath];
    std::string selectedLocaleCode;
    if (index > 0) {
      // Fetch selected locale code from locale list if non-default option was
      // selected.  Setting the preference to the empty string denotes using the
      // default locale.
      voice::SpeechInputLocaleConfig* localeConfig =
          voice::SpeechInputLocaleConfig::GetInstance();
      DCHECK_LT(static_cast<size_t>(index - 1),
                localeConfig->GetAvailableLocales().size());
      selectedLocaleCode = localeConfig->GetAvailableLocales()[index - 1].code;
    }
    _selectedLanguage.SetValue(selectedLocaleCode);

    // Update the UI.
    [self markAsCheckedLanguageAtIndex:index];
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - Actions

- (void)ttsToggled:(id)sender {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:ItemTypeTTSEnabled
                              sectionIdentifier:SectionIdentifierTTS];

  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  TableViewSwitchCell* switchCell =
      base::apple::ObjCCastStrict<TableViewSwitchCell>(
          [self.tableView cellForRowAtIndexPath:switchPath]);

  // Update the model and the preference with the current value of the switch.
  DCHECK_EQ(switchCell.switchView, sender);
  BOOL isOn = switchCell.switchView.isOn;
  switchItem.on = isOn;
  _ttsEnabled.SetValue(isOn);
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kVoiceSearchTTS) {
    [self updateTTSSwitchState];
    return;
  }

  DCHECK(preferenceName == prefs::kVoiceSearchLocale);
  NSUInteger indexOfSelectedLanguage = 0;
  std::string selectedLocaleCode = _selectedLanguage.GetValue();

  // The empty locale code corresponds to the default language, found at
  // position 0 in displayed list of languages.
  if (!selectedLocaleCode.empty()) {
    voice::SpeechInputLocaleConfig* localeConfig =
        voice::SpeechInputLocaleConfig::GetInstance();
    const std::vector<voice::SpeechInputLocale>& availableLocales =
        localeConfig->GetAvailableLocales();
    for (NSUInteger i = 0; i < availableLocales.size(); i++) {
      if (availableLocales[i].code == selectedLocaleCode) {
        // Offset by 1 since the displayed list of languages starts with the
        // default language, which is not part of `availableLocales`.
        indexOfSelectedLanguage = i + 1;
        break;
      }
    }
  }

  [self markAsCheckedLanguageAtIndex:indexOfSelectedLanguage];
}

#pragma mark - Private methods

- (void)markAsCheckedLanguageAtIndex:(NSUInteger)index {
  // Update the collection view model with the new selected language.
  NSArray* languageItems = [self.tableViewModel
      itemsInSectionWithIdentifier:SectionIdentifierLanguages];
  NSMutableArray* modifiedItems = [NSMutableArray array];
  for (NSUInteger ii = 0; ii < [languageItems count]; ++ii) {
    UITableViewCellAccessoryType type = (ii == index)
                                            ? UITableViewCellAccessoryCheckmark
                                            : UITableViewCellAccessoryNone;

    TableViewDetailTextItem* textItem =
        base::apple::ObjCCastStrict<TableViewDetailTextItem>(
            [languageItems objectAtIndex:ii]);
    if (textItem.accessoryType != type) {
      textItem.accessoryType = type;
      [modifiedItems addObject:textItem];
    }
  }

  [self updateTTSSwitchState];
  [self reconfigureCellsForItems:modifiedItems];
}

- (BOOL)currentLanguageSupportsTTS {
  voice::SpeechInputLocaleConfig* localeConfig =
      voice::SpeechInputLocaleConfig::GetInstance();
  std::string localeCode = _selectedLanguage.GetValue().empty()
                               ? localeConfig->GetDefaultLocale().code
                               : _selectedLanguage.GetValue();
  return localeConfig->IsTextToSpeechEnabledForCode(localeCode);
}

// Updates the TTS switch when the underlying preference changes or when the
// current language changes.
- (void)updateTTSSwitchState {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:ItemTypeTTSEnabled
                              sectionIdentifier:SectionIdentifierTTS];
  // Some languages do not support TTS.  Disable the switch for those
  // languages.
  BOOL enabled = [self currentLanguageSupportsTTS];
  BOOL on = enabled && _ttsEnabled.GetValue();

  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  switchItem.enabled = enabled;
  switchItem.on = on;

  [self reconfigureCellsForItems:@[ switchItem ]];
}

@end
