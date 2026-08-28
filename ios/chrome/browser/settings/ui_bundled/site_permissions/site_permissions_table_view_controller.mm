// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_site_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

@interface SitePermissionsTableViewController () <UISearchControllerDelegate,
                                                  UISearchResultsUpdating,
                                                  UISearchBarDelegate>

@property(nonatomic, strong) UISearchController* searchController;

@end

@implementation SitePermissionsTableViewController {
  NSArray<SitePermissionsSiteItem*>* _allSites;
  NSArray<SitePermissionsSiteItem*>* _filteredSites;
  NSString* _searchTerm;
}

- (instancetype)init {
  return [super initWithStyle:UITableViewStyleInsetGrouped];
}

#pragma mark - ChromeTableViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // TODO(crbug.com/553098545): Use localized string.
  self.title = @"Site Permissions";
  self.tableView.accessibilityIdentifier = kSitePermissionsTableViewId;

  [self filterSitesForSearchTerm:_searchTerm];

  _searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  _searchController.searchResultsUpdater = self;
  _searchController.obscuresBackgroundDuringPresentation = NO;
  _searchController.delegate = self;
  _searchController.searchBar.delegate = self;
  _searchController.searchBar.accessibilityIdentifier =
      kSitePermissionsSearchBarId;

  self.navigationItem.searchController = _searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
  self.definesPresentationContext = YES;

  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate sitePermissionsTableViewControllerWasRemoved:self];
  }
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  if (_filteredSites.count == 0) {
    NSString* message;
    if (_searchTerm.length > 0) {
      // TODO(crbug.com/553098545): Use localized string.
      message = @"No matching sites found";
    } else {
      // TODO(crbug.com/553098545): Use localized string.
      message = @"No sites have saved permissions";
    }
    [self addEmptyTableViewWithMessage:message image:nil];
    self.tableView.backgroundView.accessibilityIdentifier =
        kSitePermissionsEmptyViewId;
    return;
  }

  self.tableView.backgroundView = nil;
  [model addSectionWithIdentifier:SectionIdentifierSites];

  for (SitePermissionsSiteItem* siteItem in _filteredSites) {
    TableViewURLItem* item =
        [[TableViewURLItem alloc] initWithType:ItemTypeSite];
    item.title = siteItem.formattedTitle;
    item.URL = siteItem.URL;
    item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    [model addItem:item toSectionWithIdentifier:SectionIdentifierSites];
  }
}

#pragma mark - SitePermissionsConsumer

- (void)setSitePermissionsSiteItems:(NSArray<SitePermissionsSiteItem*>*)items {
  _allSites = [items copy];
  [self filterSitesForSearchTerm:_searchTerm];
  [self reloadData];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  if ([item isKindOfClass:[TableViewURLItem class]]) {
    TableViewURLItem* URLItem =
        base::apple::ObjCCastStrict<TableViewURLItem>(item);
    if (!URLItem.faviconAttributes) {
      __weak __typeof(self) weakSelf = self;
      [self.imageDataSource
          faviconForPageURL:URLItem.URL
                 completion:^(FaviconAttributes* attributes, BOOL cached) {
                   [weakSelf didFetchFaviconAttributes:attributes
                                                cached:cached
                                                  item:URLItem
                                             indexPath:indexPath];
                 }];
    }
  }

  return [super tableView:tableView cellForRowAtIndexPath:indexPath];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  if ([self.searchController.searchBar isFirstResponder]) {
    [self.searchController.searchBar resignFirstResponder];
  }

  NSInteger sitesSection =
      [self.tableViewModel sectionForSectionIdentifier:SectionIdentifierSites];
  if (indexPath.section == sitesSection &&
      indexPath.row < static_cast<NSInteger>(_filteredSites.count)) {
    SitePermissionsSiteItem* selectedSite = _filteredSites[indexPath.row];
    [self.delegate sitePermissionsTableViewController:self
                                        didSelectSite:selectedSite];
  }
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  [self filterSitesForSearchTerm:searchController.searchBar.text];
  [self reloadData];
}

#pragma mark - Private

- (void)filterSitesForSearchTerm:(NSString*)searchTerm {
  _searchTerm = [searchTerm copy];
  if (_searchTerm.length == 0) {
    _filteredSites = _allSites ?: @[];
    return;
  }

  NSPredicate* predicate = [NSPredicate
      predicateWithBlock:^BOOL(SitePermissionsSiteItem* item,
                               NSDictionary<NSString*, id>* bindings) {
        return [item.formattedTitle
                   localizedCaseInsensitiveContainsString:searchTerm] ||
               [item.origin localizedCaseInsensitiveContainsString:searchTerm];
      }];
  _filteredSites = [_allSites filteredArrayUsingPredicate:predicate];
}

- (void)reloadData {
  if (!self.tableViewModel) {
    return;
  }
  [self loadModel];
  [self.tableView reloadData];
}

- (void)didFetchFaviconAttributes:(FaviconAttributes*)attributes
                           cached:(BOOL)cached
                             item:(TableViewURLItem*)item
                        indexPath:(NSIndexPath*)indexPath {
  item.faviconAttributes = attributes;
  if (!cached && attributes.faviconImage) {
    if (![self.tableViewModel hasItemAtIndexPath:indexPath] ||
        [self.tableViewModel itemAtIndexPath:indexPath] != item) {
      return;
    }
    LegacyTableViewCell* cell = base::apple::ObjCCast<LegacyTableViewCell>(
        [self.tableView cellForRowAtIndexPath:indexPath]);
    if (!cell) {
      return;
    }
    [item configureCell:cell];
  }
}

@end
