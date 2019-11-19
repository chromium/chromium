// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/settings/cells/search_engine_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/favicon/favicon_view.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

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
const CGFloat kTableViewSeparatorTrailingInset = 16;
const int kFaviconDesiredSizeInPoint = 32;
const int kFaviconMinSizeInPoint = 16;
constexpr base::TimeDelta kMaxVisitAge = base::TimeDelta::FromDays(2);
const size_t kMaxcustomSearchEngines = 3;
const char kUmaSelectDefaultSearchEngine[] =
    "Search.iOS.SelectDefaultSearchEngine";

}  // namespace

@interface SearchEngineTableViewController () <SearchEngineObserving>
@end

@implementation SearchEngineTableViewController {
  TemplateURLService* _templateURLService;  // weak
  std::unique_ptr<SearchEngineObserverBridge> _observer;
  // Prevent unnecessary notifications when we write to the setting.
  BOOL _updatingBackend;
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

#pragma mark Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _templateURLService =
        ios::TemplateURLServiceFactory::GetForBrowserState(browserState);
    _observer =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    _templateURLService->Load();
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(browserState);
    [self setTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.separatorInset = UIEdgeInsetsMake(
      0, kTableViewSeparatorLeadingInset, 0, kTableViewSeparatorTrailingInset);

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
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

#pragma mark UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
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
  _updatingBackend = YES;
  if (indexPath.section ==
      [model sectionForSectionIdentifier:SectionIdentifierFirstList]) {
    _templateURLService->SetUserSelectedDefaultSearchProvider(
        _firstList[indexPath.row]);
  } else {
    _templateURLService->SetUserSelectedDefaultSearchProvider(
        _secondList[indexPath.row]);
  }
  [self recordUmaOfDefaultSearchEngine];
  _updatingBackend = NO;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  DCHECK(item.type == ItemTypePrepopulatedEngine ||
         item.type == ItemTypeCustomEngine);
  SearchEngineItem* engineItem =
      base::mac::ObjCCastStrict<SearchEngineItem>(item);
  TableViewURLCell* urlCell = base::mac::ObjCCastStrict<TableViewURLCell>(cell);

  if (item.type == ItemTypePrepopulatedEngine) {
    _faviconLoader->FaviconForPageUrl(
        engineItem.URL, kFaviconDesiredSizeInPoint, kFaviconMinSizeInPoint,
        /*fallback_to_google_server=*/YES, ^(FaviconAttributes* attributes) {
          // Only set favicon if the cell hasn't been reused.
          if (urlCell.cellUniqueIdentifier == engineItem.uniqueIdentifier) {
            DCHECK(attributes);
            [urlCell.faviconView configureWithAttributes:attributes];
          }
        });
  } else {
    _faviconLoader->FaviconForIconUrl(
        engineItem.URL, kFaviconDesiredSizeInPoint, kFaviconMinSizeInPoint,
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

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  if (!_updatingBackend)
    [self reloadData];
}

#pragma mark - Private methods

// Loads all TemplateURLs from TemplateURLService and classifies them into
// |_firstList| and |_secondList|. If a TemplateURL is
// prepopulated, created by policy or the default search engine, it will get
// into the first list, otherwise the second list.
- (void)loadSearchEngines {
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
  // Sort |fixedCutomeSearchEngines_| by TemplateURL's prepopulate_id. If
  // prepopulated_id == 0, it's a custom search engine and should be put in the
  // end of the list.
  std::sort(_firstList.begin(), _firstList.end(),
            [](const TemplateURL* lhs, const TemplateURL* rhs) {
              if (lhs->prepopulate_id() == 0)
                return false;
              if (rhs->prepopulate_id() == 0)
                return true;
              return lhs->prepopulate_id() < rhs->prepopulate_id();
            });

  // Partially sort |_secondList| by TemplateURL's last_visited time.
  auto begin = _secondList.begin();
  auto end = _secondList.end();
  auto pivot = begin + std::min(kMaxcustomSearchEngines, _secondList.size());
  std::partial_sort(begin, pivot, end,
                    [](const TemplateURL* lhs, const TemplateURL* rhs) {
                      return lhs->last_visited() > rhs->last_visited();
                    });

  // Keep the search engines visited within |kMaxVisitAge| and erase others.
  const base::Time cutoff = base::Time::Now() - kMaxVisitAge;
  auto cutBegin = std::find_if(begin, pivot, [cutoff](const TemplateURL* url) {
    return url->last_visited() < cutoff;
  });
  _secondList.erase(cutBegin, end);
}

// Creates a SearchEngineItem for |templateURL|.
- (SearchEngineItem*)createSearchEngineItemFromTemplateURL:
    (const TemplateURL*)templateURL {
  SearchEngineItem* item = nil;
  if (templateURL->prepopulate_id() > 0) {
    item = [[SearchEngineItem alloc] initWithType:ItemTypePrepopulatedEngine];
    // Fake up a page URL for favicons of prepopulated search engines, since
    // favicons may be fetched from Google server which doesn't suppoprt
    // icon URL.
    std::string emptyPageUrl = templateURL->url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(base::string16()),
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

@end
