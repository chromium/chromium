// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"

#import <memory>

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_observer.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/search_engine_item.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFirstList = kSectionIdentifierEnumZero,
  SectionIdentifierSecondList,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypePrepopulatedEngine = kItemTypeEnumZero,
  ItemTypeHeader,
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
  TemplateURLService* _templateURLService;  // weak
  std::unique_ptr<SearchEngineObserverBridge> _observer;
  // The first list in the page which contains prepopulted search engines and
  // search engines that are created by policy, and possibly one custom search
  // engine if it's selected as default search engine.
  std::vector<TemplateURL*> _firstList;
  // The second list in the page which contains all remaining custom search
  // engines.
  std::vector<TemplateURL*> _secondList;
  // FaviconLoader is a keyed service that uses LargeIconService to retrieve
  // favicon images.
  FaviconLoader* _faviconLoader;
}

#pragma mark - Initialization

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(browserState);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _templateURLService =
        ios::TemplateURLServiceFactory::GetForBrowserState(browserState);
    _observer =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    _templateURLService->Load();
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(browserState);
    [self setTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)];
    self.shouldDisableDoneButtonOnEdit = YES;
    [self updateUIForEditState];
  }
  return self;
}

#pragma mark - Properties

- (void)setUpdatingBackend:(BOOL)updatingBackend {
  if (_updatingBackend == updatingBackend)
    return;

  _updatingBackend = updatingBackend;

  if (!self.searchEngineChangedInBackground)
    return;

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
    if ([self isItem:firstListItem[index]
            differentForTemplateURL:_firstList[index]]) {
      // Item has changed, reload the TableView.
      [self reloadData];
      return;
    }
  }

  if (hasSecondSection) {
    NSArray* secondListItem = [self.tableViewModel
        itemsInSectionWithIdentifier:SectionIdentifierSecondList];
    for (NSUInteger index = 0; index < secondListItem.count; index++) {
      if ([self isItem:secondListItem[index]
              differentForTemplateURL:_secondList[index]]) {
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

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed)
    return;

  TableViewModel* model = self.tableViewModel;
  [self loadSearchEngines];

  // Add prior search engines.
  if (_firstList.size() > 0) {
    [model addSectionWithIdentifier:SectionIdentifierFirstList];

    for (const TemplateURL* templateURL : _firstList) {
      [model addItem:[self createSearchEngineItemFromTemplateURL:templateURL]
          toSectionWithIdentifier:SectionIdentifierFirstList];
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
      [model addItem:[self createSearchEngineItemFromTemplateURL:templateURL]
          toSectionWithIdentifier:SectionIdentifierSecondList];
    }
  }
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
  return [self.tableViewModel hasItemForItemType:ItemTypeCustomEngine
                               sectionIdentifier:SectionIdentifierFirstList] ||
         [self.tableViewModel hasItemForItemType:ItemTypeCustomEngine
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
  SearchEngineItem* selectedTextItem =
      base::mac::ObjCCastStrict<SearchEngineItem>(selectedItem);
  if (selectedTextItem.accessoryType == UITableViewCellAccessoryCheckmark) {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    return;
  }

  // Iterate through the engines and remove the checkmark from any that have it.
  if ([model hasSectionForSectionIdentifier:SectionIdentifierFirstList]) {
    for (TableViewItem* item in
         [model itemsInSectionWithIdentifier:SectionIdentifierFirstList]) {
      SearchEngineItem* textItem =
          base::mac::ObjCCastStrict<SearchEngineItem>(item);
      if (textItem.accessoryType == UITableViewCellAccessoryCheckmark) {
        textItem.accessoryType = UITableViewCellAccessoryNone;
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
      SearchEngineItem* textItem =
          base::mac::ObjCCastStrict<SearchEngineItem>(item);
      if (textItem.accessoryType == UITableViewCellAccessoryCheckmark) {
        textItem.accessoryType = UITableViewCellAccessoryNone;
        UITableViewCell* cell =
            [tableView cellForRowAtIndexPath:[model indexPathForItem:item]];
        cell.accessoryType = UITableViewCellAccessoryNone;
      }
    }
  }

  // Show the checkmark on the new default engine.

  SearchEngineItem* newDefaultEngine =
      base::mac::ObjCCastStrict<SearchEngineItem>(
          [model itemAtIndexPath:indexPath]);
  newDefaultEngine.accessoryType = UITableViewCellAccessoryCheckmark;
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  cell.accessoryType = UITableViewCellAccessoryCheckmark;

  // Set the new engine as the default.
  self.updatingBackend = YES;
  if (indexPath.section ==
      [model sectionForSectionIdentifier:SectionIdentifierFirstList]) {
    _templateURLService->SetUserSelectedDefaultSearchProvider(
        _firstList[indexPath.row]);
  } else {
    _templateURLService->SetUserSelectedDefaultSearchProvider(
        _secondList[indexPath.row]);
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

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if (_settingsAreDismissed)
    return cell;

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  DCHECK(item.type == ItemTypePrepopulatedEngine ||
         item.type == ItemTypeCustomEngine);
  SearchEngineItem* engineItem =
      base::mac::ObjCCastStrict<SearchEngineItem>(item);
  TableViewURLCell* urlCell = base::mac::ObjCCastStrict<TableViewURLCell>(cell);

  if (item.type == ItemTypePrepopulatedEngine) {
    _faviconLoader->FaviconForPageUrl(
        engineItem.URL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
        /*fallback_to_google_server=*/YES, ^(FaviconAttributes* attributes) {
          // Only set favicon if the cell hasn't been reused.
          if (urlCell.cellUniqueIdentifier == engineItem.uniqueIdentifier) {
            DCHECK(attributes);
            [urlCell.faviconView configureWithAttributes:attributes];
          }
        });
  } else {
    _faviconLoader->FaviconForIconUrl(
        engineItem.URL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
        ^(FaviconAttributes* attributes) {
          // Only set favicon if the cell hasn't been reused.
          if (urlCell.cellUniqueIdentifier == engineItem.uniqueIdentifier) {
            DCHECK(attributes);
            [urlCell.faviconView configureWithAttributes:attributes];
          }
        });
  }
  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  return item.type == ItemTypeCustomEngine;
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

  std::vector<TemplateURL*> urls = _templateURLService->GetTemplateURLs();
  _firstList.clear();
  _firstList.reserve(urls.size());
  _secondList.clear();
  _secondList.reserve(urls.size());

  // Classify TemplateURLs.
  for (TemplateURL* url : urls) {
    if (_templateURLService->IsPrepopulatedOrCreatedByPolicy(url) ||
        url == _templateURLService->GetDefaultSearchProvider())
      _firstList.push_back(url);
    else
      _secondList.push_back(url);
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

// Creates a SearchEngineItem for `templateURL`.
- (SearchEngineItem*)createSearchEngineItemFromTemplateURL:
    (const TemplateURL*)templateURL {
  if (_settingsAreDismissed)
    return nil;

  SearchEngineItem* item = nil;
  if (templateURL->prepopulate_id() > 0) {
    item = [[SearchEngineItem alloc] initWithType:ItemTypePrepopulatedEngine];
    // Fake up a page URL for favicons of prepopulated search engines, since
    // favicons may be fetched from Google server which doesn't suppoprt
    // icon URL.
    std::string emptyPageUrl = templateURL->url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(std::u16string()),
        _templateURLService->search_terms_data());
    item.URL = GURL(emptyPageUrl);
  } else {
    item = [[SearchEngineItem alloc] initWithType:ItemTypeCustomEngine];
    // Use icon URL for favicons of custom search engines.
    item.URL = templateURL->favicon_url();
  }
  item.text = base::SysUTF16ToNSString(templateURL->short_name());
  item.detailText = base::SysUTF16ToNSString(templateURL->keyword());
  if (templateURL == _templateURLService->GetDefaultSearchProvider()) {
    [item setAccessoryType:UITableViewCellAccessoryCheckmark];
  }
  return item;
}

// Records the type of the selected default search engine.
- (void)recordUmaOfDefaultSearchEngine {
  UMA_HISTOGRAM_ENUMERATION(
      kUmaSelectDefaultSearchEngine,
      _templateURLService->GetDefaultSearchProvider()->GetEngineType(
          _templateURLService->search_terms_data()),
      SEARCH_ENGINE_MAX);
}

// Deletes custom search engines at `indexPaths`. If a custom engine is selected
// as the default engine, resets default engine to the first prepopulated
// engine.
- (void)deleteItemAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  if (_settingsAreDismissed)
    return;

  // Update `_templateURLService`, `_firstList` and `_secondList`.
  _updatingBackend = YES;
  size_t removedItemsInSecondList = 0;
  NSInteger firstSection = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierFirstList];
  bool resetDefaultEngine = false;

  // Remove search engines from `_firstList`, `_secondList` and
  // `_templateURLService`.
  for (NSIndexPath* path : indexPaths) {
    TemplateURL* engine = nullptr;
    if (path.section == firstSection) {
      TableViewItem* item = [self.tableViewModel itemAtIndexPath:path];
      // Only custom search engine can be deleted.
      DCHECK(item.type == ItemTypeCustomEngine);
      // The custom search engine in the first section should be the last one.
      DCHECK(path.row == static_cast<int>(_firstList.size()) - 1);

      engine = _firstList.back();
      _firstList.pop_back();
    } else {
      DCHECK(path.row < static_cast<int>(_secondList.size()));

      engine = _secondList[path.row];
      // Mark as deleted by setting to nullptr.
      _secondList[path.row] = nullptr;
      ++removedItemsInSecondList;
    }
    // If `engine` is selected as default search engine, reset the default
    // engine to the first prepopulated engine.
    if (engine == _templateURLService->GetDefaultSearchProvider()) {
      DCHECK(_firstList.size() > 0);
      _templateURLService->SetUserSelectedDefaultSearchProvider(_firstList[0]);
      resetDefaultEngine = true;
    }
    _templateURLService->Remove(engine);
  }

  // Clean up the second list.
  if (removedItemsInSecondList > 0) {
    if (removedItemsInSecondList == _secondList.size()) {
      _secondList.clear();
    } else {
      std::vector<TemplateURL*> newList(
          _secondList.size() - removedItemsInSecondList, nullptr);
      for (size_t i = 0, added = 0; i < _secondList.size(); ++i) {
        if (_secondList[i]) {
          newList[added++] = _secondList[i];
        }
      }
      _secondList = std::move(newList);
    }
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

        // Update the first prepopulated engine if it's reset as default.
        if (resetDefaultEngine) {
          NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0
                                                      inSection:firstSection];
          TableViewItem* item = [model itemAtIndexPath:indexPath];
          SearchEngineItem* engineItem =
              base::mac::ObjCCastStrict<SearchEngineItem>(item);
          engineItem.accessoryType = UITableViewCellAccessoryCheckmark;
          [strongSelf.tableView
              reloadRowsAtIndexPaths:@[ indexPath ]
                    withRowAnimation:UITableViewRowAnimationAutomatic];
        }

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

  NSArray<NSIndexPath*>* indexPaths =
      [self.tableViewModel indexPathsForItemType:ItemTypePrepopulatedEngine
                               sectionIdentifier:SectionIdentifierFirstList];
  for (NSIndexPath* indexPath in indexPaths) {
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    SearchEngineItem* engineItem =
        base::mac::ObjCCastStrict<SearchEngineItem>(item);
    engineItem.enabled = !editing;
    if (!editing && _firstList[indexPath.item] ==
                        _templateURLService->GetDefaultSearchProvider()) {
      engineItem.accessoryType = UITableViewCellAccessoryCheckmark;
    } else {
      engineItem.accessoryType = UITableViewCellAccessoryNone;
    }
  }
  [self.tableView reloadRowsAtIndexPaths:indexPaths
                        withRowAnimation:UITableViewRowAnimationAutomatic];
}

// Returns whether the `item` is different from an item that would be created
// from `templateURL`.
- (BOOL)isItem:(SearchEngineItem*)item
    differentForTemplateURL:(TemplateURL*)templateURL {
  NSString* name = base::SysUTF16ToNSString(templateURL->short_name());
  NSString* keyword = base::SysUTF16ToNSString(templateURL->keyword());
  return ![item.text isEqualToString:name] ||
         ![item.detailText isEqualToString:keyword];
}

@end
