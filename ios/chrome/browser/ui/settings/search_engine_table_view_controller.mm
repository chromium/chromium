// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_observer.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_search_engine_item.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFirstList = kSectionIdentifierEnumZero,
  SectionIdentifierSecondList,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  // Used for prepopulated search engine.
  ItemTypePrepopulatedEngine,
  // Used for custom search engine.
  ItemTypeCustomEngine,
};

const CGFloat kTableViewSeparatorLeadingInset = 56;
constexpr base::TimeDelta kMaxVisitAge = base::Days(2);
const size_t kMaxcustomSearchEngines = 3;
const char kUmaSelectDefaultSearchEngine[] =
    "Search.iOS.SelectDefaultSearchEngine";

}  // namespace

@interface SearchEngineTableViewController () <SearchEngineObserving> {
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

// Prevent unnecessary notifications when we write to the setting.
@property(nonatomic, assign) BOOL updatingBackend;

// Whether the search engines have changed while the backend was being updated.
@property(nonatomic, assign) BOOL searchEngineChangedInBackground;

@end

@implementation SearchEngineTableViewController {
  raw_ptr<TemplateURLService> _templateURLService;  // weak
  raw_ptr<PrefService> _prefService;                // weak
  raw_ptr<search_engines::SearchEngineChoiceService>
      _searchEngineChoiceService;  // weak
  std::unique_ptr<SearchEngineObserverBridge> _observer;
  // The list of choice screen search engines retrieved from the
  // TemplateURLService.
  std::vector<std::unique_ptr<TemplateURL>> _choiceScreenTemplateURLs;
  // The first list in the page which contains prepopulated search engines and
  // search engines that are created by policy, and possibly one custom search
  // engine if it's selected as default search engine.
  // Note that `TemplateURL` pointers should not be freed. They either come from
  // `TemplateURLService::GetTemplateURLs()`, or they are owned by
  // `_choiceScreenTemplateUrls`.
  std::vector<raw_ptr<TemplateURL>> _firstList;
  // The second list in the page which contains all remaining custom search
  // engines.
  // Note that `TemplateURL` pointers should not be freed. They either come from
  // `TemplateURLService::GetTemplateURLs()`, or they are owned by
  // `_choiceScreenTemplateUrls`.
  std::vector<raw_ptr<TemplateURL>> _secondList;
  // FaviconLoader is a keyed service that uses LargeIconService to retrieve
  // favicon images.
  raw_ptr<FaviconLoader> _faviconLoader;
}

#pragma mark - Initialization

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  DCHECK(profile);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _templateURLService =
        ios::TemplateURLServiceFactory::GetForProfile(profile);
    _observer =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    _templateURLService->Load();
    _faviconLoader = IOSChromeFaviconLoaderFactory::GetForProfile(profile);
    _prefService = profile->GetPrefs();

    _searchEngineChoiceService =
        ios::SearchEngineChoiceServiceFactory::GetForProfile(profile);

    [self setTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)];
    self.shouldDisableDoneButtonOnEdit = YES;
  }
  return self;
}

#pragma mark - Properties

- (void)setUpdatingBackend:(BOOL)updatingBackend {
  if (_updatingBackend == updatingBackend)
    return;

  _updatingBackend = updatingBackend;

  if (!self.searchEngineChangedInBackground) {
    return;
  }

  [self loadSearchEngines];

  BOOL hasSecondSection = [self.tableViewModel
      hasSectionForSectionIdentifier:SectionIdentifierSecondList];
  BOOL secondSectionExistenceChanged = hasSecondSection == _secondList.empty();
  BOOL numberOfCustomItemDifferent =
      secondSectionExistenceChanged ||
      (hasSecondSection &&
       [self.tableViewModel
           itemsInSectionWithIdentifier:SectionIdentifierSecondList]
               .count != _secondList.size());

  BOOL numberOfPrepopulatedItemDifferent =
      [self.tableViewModel
          itemsInSectionWithIdentifier:SectionIdentifierFirstList]
          .count != _firstList.size();

  if (numberOfPrepopulatedItemDifferent || numberOfCustomItemDifferent) {
    // The number of items has changed.
    [self reloadData];
    return;
  }

  NSArray* firstListItem = [self.tableViewModel
      itemsInSectionWithIdentifier:SectionIdentifierFirstList];
  for (NSUInteger index = 0; index < firstListItem.count; index++) {
    if (![self isItem:firstListItem[index]
            equalForTemplateURL:_firstList[index]]) {
      // Item has changed, reload the TableView.
      [self reloadData];
      return;
    }
  }

  if (hasSecondSection) {
    NSArray* secondListItem = [self.tableViewModel
        itemsInSectionWithIdentifier:SectionIdentifierSecondList];
    for (NSUInteger index = 0; index < secondListItem.count; index++) {
      if (![self isItem:secondListItem[index]
              equalForTemplateURL:_secondList[index]]) {
        // Item has changed, reload the TableView.
        [self reloadData];
        return;
      }
    }
  }
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // With no header on first appearance, UITableView adds a 35 points space at
  // the beginning of the table view. This space remains after this table view
  // reloads with headers. Setting a small tableHeaderView avoids this.
  self.tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];

  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.separatorInset =
      UIEdgeInsetsMake(0, kTableViewSeparatorLeadingInset, 0, 0);

  [self updateUIForEditState];
  [self loadModel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.navigationController.toolbarHidden = NO;
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  if (editing) {
    base::RecordAction(
        base::UserMetricsAction("IOS.SearchEngines.RecentlyViewed.Edit"));
  }

  [super setEditing:editing animated:animated];

  // Disable prepopulated engines and remove the checkmark in editing mode, and
  // recover them in normal mode.
  [self updatePrepopulatedEnginesForEditing:editing];
  [self updateUIForEditState];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed)
    return;

  self.tableView.accessibilityIdentifier = kSearchEngineTableViewControllerId;
  TableViewModel* model = self.tableViewModel;
  [self loadSearchEngines];

  // Add prior search engines.
  if (_firstList.size() > 0) {
    [model addSectionWithIdentifier:SectionIdentifierFirstList];

    if (search_engines::IsEeaChoiceCountry(
            _searchEngineChoiceService->GetCountryId())) {
      TableViewTextHeaderFooterItem* header =
          [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
      header.subtitle =
          l10n_util::GetNSString(IDS_SEARCH_ENGINE_CHOICE_SETTINGS_SUBTITLE);
      [model setHeader:header
          forSectionWithIdentifier:SectionIdentifierFirstList];
    }

    for (const TemplateURL* templateURL : _firstList) {
      TableViewItem* item = nil;
      item = [self createSettingsSearchEngineItemFromTemplateURL:templateURL];
      [model addItem:item toSectionWithIdentifier:SectionIdentifierFirstList];
    }
  }

  // Add custom search engines.
  if (_secondList.size() > 0) {
    [model addSectionWithIdentifier:SectionIdentifierSecondList];

    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
    header.text = l10n_util::GetNSString(
        IDS_IOS_SEARCH_ENGINE_SETTING_CUSTOM_SECTION_HEADER);
    [model setHeader:header
        forSectionWithIdentifier:SectionIdentifierSecondList];

    for (const TemplateURL* templateURL : _secondList) {
      DCHECK(templateURL->prepopulate_id() == 0);
      TableViewItem* item = nil;
      item = [self createSettingsSearchEngineItemFromTemplateURL:templateURL];
      [model addItem:item toSectionWithIdentifier:SectionIdentifierSecondList];
    }
  }
  // The toolbar edit button's state needs to be updated.
  [self updatedToolbarForEditState];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileSearchEngineSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileSearchEngineSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  // Remove observer bridges.
  _observer.reset();

  // Clear C++ ivars.
  _templateURLService = nullptr;
  _prefService = nullptr;
  _searchEngineChoiceService = nullptr;
  _faviconLoader = nullptr;

  _settingsAreDismissed = YES;
}

#pragma mark - SettingsRootTableViewController

- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  base::RecordAction(
      base::UserMetricsAction("IOS.SearchEngines.RecentlyViewed.Delete"));
  // Do not call super as this also deletes the section if it is empty.
  [self deleteItemAtIndexPaths:indexPaths];
}

- (BOOL)shouldHideToolbar {
  return NO;
}

- (BOOL)shouldShowEditDoneButton {
  return NO;
}

- (BOOL)editButtonEnabled {
  // The custom search engine can only be deleted when they are not selected
  // (so in the second section).
  return [self.tableViewModel hasItemForItemType:ItemTypeCustomEngine
                               sectionIdentifier:SectionIdentifierSecondList];
}

- (void)updateUIForEditState {
  [super updateUIForEditState];
  [self updatedToolbarForEditState];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  if (_settingsAreDismissed)
    return;

  // Keep selection in editing mode.
  if (self.editing) {
    self.deleteButton.enabled = YES;
    return;
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  TableViewModel* model = self.tableViewModel;
  TableViewItem* selectedItem = [model itemAtIndexPath:indexPath];

  // Only search engine items can be selected.
  DCHECK(selectedItem.type == ItemTypePrepopulatedEngine ||
         selectedItem.type == ItemTypeCustomEngine);

  // Do nothing if the tapped engine was already the default.
  if (selectedItem.accessoryType == UITableViewCellAccessoryCheckmark) {
    return;
  }

  // Iterate through the engines and remove the checkmark from any that have it.
  if ([model hasSectionForSectionIdentifier:SectionIdentifierFirstList]) {
    for (TableViewItem* item in
         [model itemsInSectionWithIdentifier:SectionIdentifierFirstList]) {
      if (item.accessoryType == UITableViewCellAccessoryCheckmark) {
        item.accessoryType = UITableViewCellAccessoryNone;
        UITableViewCell* cell =
            [tableView cellForRowAtIndexPath:[model indexPathForItem:item]];
        cell.accessoryType = UITableViewCellAccessoryNone;
      }
    }
  }
  if ([model hasSectionForSectionIdentifier:SectionIdentifierSecondList]) {
    for (TableViewItem* item in
         [model itemsInSectionWithIdentifier:SectionIdentifierSecondList]) {
      DCHECK(item.type == ItemTypeCustomEngine);
      if (item.accessoryType == UITableViewCellAccessoryCheckmark) {
        item.accessoryType = UITableViewCellAccessoryNone;
        UITableViewCell* cell =
            [tableView cellForRowAtIndexPath:[model indexPathForItem:item]];
        cell.accessoryType = UITableViewCellAccessoryNone;
      }
    }
  }

  // Show the checkmark on the new default engine.
  TableViewItem* newDefaultEngine = [model itemAtIndexPath:indexPath];
  newDefaultEngine.accessoryType = UITableViewCellAccessoryCheckmark;
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  cell.accessoryType = UITableViewCellAccessoryCheckmark;

  // Set the new engine as the default.
  self.updatingBackend = YES;
  if (indexPath.section ==
      [model sectionForSectionIdentifier:SectionIdentifierFirstList]) {
    _templateURLService->SetUserSelectedDefaultSearchProvider(
        _firstList[indexPath.row],
        search_engines::ChoiceMadeLocation::kSearchEngineSettings);
  } else {
    _templateURLService->SetUserSelectedDefaultSearchProvider(
        _secondList[indexPath.row],
        search_engines::ChoiceMadeLocation::kSearchEngineSettings);
  }
  [self recordUmaOfDefaultSearchEngine];
  self.updatingBackend = NO;
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didDeselectRowAtIndexPath:indexPath];
  if (!self.tableView.editing)
    return;

  if (self.tableView.indexPathsForSelectedRows.count == 0)
    self.deleteButton.enabled = NO;
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // Only the search engines from the second section can be removed.
  // In the first section, search engines are either prepopulated or
  // selected custom search engines.
  TableViewModel* model = self.tableViewModel;
  NSInteger sectionIdentifier =
      [model sectionIdentifierForSectionIndex:indexPath.section];
  return sectionIdentifier == SectionIdentifierSecondList;
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK(editingStyle == UITableViewCellEditingStyleDelete);
  [self deleteItemAtIndexPaths:@[ indexPath ]];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  if (!self.updatingBackend) {
    [self reloadData];
  } else {
    self.searchEngineChangedInBackground = YES;
  }
}

#pragma mark - Private methods

// Loads all TemplateURLs from TemplateURLService and classifies them into
// `_firstList` and `_secondList`. If a TemplateURL is
// prepopulated, created by policy or the default search engine, it will get
// into the first list, otherwise the second list.
- (void)loadSearchEngines {
  if (_settingsAreDismissed)
    return;

  // TODO(b/280753739) Update this method to return the correct list of search
  // engines directly (for both choice-screen-eligible users and
  // non-choice-screen-eligible users). This way we don't have to worry about
  // calling two different methods anymore.
  std::vector<raw_ptr<TemplateURL, VectorExperimental>> urls =
      _templateURLService->GetTemplateURLs();
  _firstList.clear();
  _firstList.reserve(urls.size());
  _secondList.clear();
  _secondList.reserve(urls.size());
  // Classify TemplateURLs.
  for (TemplateURL* url : urls) {
    if ([self isPrepopulatedOrDefaultSearchEngine:url]) {
      _firstList.push_back(url);
    } else {
      _secondList.push_back(url);
    }
  }

  // Do not sort prepopulated search engines, they are already sorted by
  // locale use.

  // Partially sort `_secondList` by TemplateURL's last_visited time.
  auto begin = _secondList.begin();
  auto end = _secondList.end();
  auto pivot = begin + std::min(kMaxcustomSearchEngines, _secondList.size());
  std::partial_sort(begin, pivot, end,
                    [](const TemplateURL* lhs, const TemplateURL* rhs) {
                      return lhs->last_visited() > rhs->last_visited();
                    });

  // Keep the search engines visited within `kMaxVisitAge` and erase others.
  auto cutBegin = base::ranges::lower_bound(
      begin, pivot, base::Time::Now() - kMaxVisitAge,
      base::ranges::greater_equal(), &TemplateURL::last_visited);
  _secondList.erase(cutBegin, end);
}

// Creates a SettingsSearchEngineItem for `templateURL`.
- (TableViewItem*)createSettingsSearchEngineItemFromTemplateURL:
    (const TemplateURL*)templateURL {
  if (_settingsAreDismissed)
    return nil;

  SettingsSearchEngineItem* item = nil;
  if (templateURL->prepopulate_id() > 0) {
    item = [[SettingsSearchEngineItem alloc]
        initWithType:ItemTypePrepopulatedEngine];
  } else {
    item = [[SettingsSearchEngineItem alloc] initWithType:ItemTypeCustomEngine];
  }
  item.templateURL = templateURL;
  item.text = base::SysUTF16ToNSString(templateURL->short_name());
  item.detailText = base::SysUTF16ToNSString(templateURL->keyword());
  if ([self isItem:item
          equalForTemplateURL:_templateURLService
                                  ->GetDefaultSearchProvider()]) {
    [item setAccessoryType:UITableViewCellAccessoryCheckmark];
  }
  __weak __typeof(self) weakSelf = self;
  GetSearchEngineFavicon(
      *templateURL, _searchEngineChoiceService, _templateURLService,
      _faviconLoader, ^(FaviconAttributes* attributes) {
        [weakSelf faviconReceivedFor:item faviconAttributes:attributes];
      });
  return item;
}

- (void)faviconReceivedFor:(SettingsSearchEngineItem*)item
         faviconAttributes:(FaviconAttributes*)attributes {
  item.faviconAttributes = attributes;
  [self reconfigureCellsForItems:@[ item ]];
}

// Records the type of the selected default search engine.
- (void)recordUmaOfDefaultSearchEngine {
  UMA_HISTOGRAM_ENUMERATION(
      kUmaSelectDefaultSearchEngine,
      _templateURLService->GetDefaultSearchProvider()->GetEngineType(
          _templateURLService->search_terms_data()),
      SEARCH_ENGINE_MAX);
}

// Deletes custom search engines at `indexPaths`.
// The selected custom search engine cannot be removed.
- (void)deleteItemAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  if (_settingsAreDismissed) {
    return;
  }
  // Update `_templateURLService` and `_secondList`.
  _updatingBackend = YES;
  NSInteger secondSectionIdentifier = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierSecondList];

  // Remove search engines from `_templateURLService` and `_secondList`.
  for (NSIndexPath* path : indexPaths) {
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:path];
    SettingsSearchEngineItem* engineItem =
        base::apple::ObjCCastStrict<SettingsSearchEngineItem>(item);
    // It should not be possible to remove a search engine from the first
    // section. The settings should either contains a selected custom search
    // engine (which cannot be removed as long as it is selected),
    // or prepopulated search engine.
    CHECK_EQ(path.section, secondSectionIdentifier, base::NotFatalUntil::M135);
    std::erase(_secondList, engineItem.templateURL);
    _templateURLService->Remove(engineItem.templateURL);
  }

  // Update UI.
  __weak SearchEngineTableViewController* weakSelf = self;
  [self.tableView
      performBatchUpdates:^{
        SearchEngineTableViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;

        TableViewModel* model = strongSelf.tableViewModel;
        [strongSelf removeFromModelItemAtIndexPaths:indexPaths];
        [strongSelf.tableView
            deleteRowsAtIndexPaths:indexPaths
                  withRowAnimation:UITableViewRowAnimationAutomatic];

        // Remove second section if it's empty.
        if (strongSelf->_secondList.empty() &&
            [model
                hasSectionForSectionIdentifier:SectionIdentifierSecondList]) {
          NSInteger section =
              [model sectionForSectionIdentifier:SectionIdentifierSecondList];
          [model removeSectionWithIdentifier:SectionIdentifierSecondList];
          [strongSelf.tableView
                deleteSections:[NSIndexSet indexSetWithIndex:section]
              withRowAnimation:UITableViewRowAnimationFade];
        }

        _updatingBackend = NO;
      }
      completion:^(BOOL finished) {
        SearchEngineTableViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;

        // Update editing status.
        if (![strongSelf editButtonEnabled]) {
          [strongSelf setEditing:NO animated:YES];
        }
        [strongSelf updateUIForEditState];
      }];
}

// Disables prepopulated engines and removes the checkmark in editing mode.
// Enables engines and recovers the checkmark in normal mode.
- (void)updatePrepopulatedEnginesForEditing:(BOOL)editing {
  if (_settingsAreDismissed)
    return;
  // All prepopulated items in the first section should be updated.
  // The selected custom item should be updated since the user is not allowed
  // to remove it.
  NSArray<TableViewItem*>* items = [self.tableViewModel
      itemsInSectionWithIdentifier:SectionIdentifierFirstList];
  for (TableViewItem* item in items) {
    SettingsSearchEngineItem* engineItem =
        base::apple::ObjCCastStrict<SettingsSearchEngineItem>(item);
    engineItem.enabled = !editing;
    if (!editing && [self isItem:engineItem
                        equalForTemplateURL:_templateURLService
                                                ->GetDefaultSearchProvider()]) {
      engineItem.accessoryType = UITableViewCellAccessoryCheckmark;
    } else {
      engineItem.accessoryType = UITableViewCellAccessoryNone;
    }
  }
  [self reloadCellsForItems:items
           withRowAnimation:UITableViewRowAnimationAutomatic];
}

// Returns whether the `item` is the same as an item that would be created
// from `templateURL`.
- (BOOL)isItem:(SettingsSearchEngineItem*)item
    equalForTemplateURL:(const TemplateURL*)templateURL {
  if (!templateURL) {
    return NO;
  }
  NSString* name = base::SysUTF16ToNSString(templateURL->short_name());
  NSString* keyword = base::SysUTF16ToNSString(templateURL->keyword());
  return [item.text isEqualToString:name] &&
         [item.detailText isEqualToString:keyword];
}

- (BOOL)isPrepopulatedOrDefaultSearchEngine:(const TemplateURL*)templateURL {
  return _templateURLService->IsPrepopulatedOrDefaultProviderByPolicy(
             templateURL) ||
         templateURL == _templateURLService->GetDefaultSearchProvider();
}

@end
