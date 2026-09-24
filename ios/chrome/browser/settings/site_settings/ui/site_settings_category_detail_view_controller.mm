// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "components/content_settings/core/common/content_settings.h"
#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_mutator.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_site_exception.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_disclosure_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Section identifiers for the Category Detail table view.
enum SectionIdentifier {
  SectionIdentifierDefaultSetting = kSectionIdentifierEnumZero,
  SectionIdentifierNotAllowed,
  SectionIdentifierAllowed,
};

// Item types for the Category Detail table view.
enum ItemType {
  ItemTypeDefaultSettingAsk = kItemTypeEnumZero,
  ItemTypeDefaultSettingBlock,
  ItemTypeNotAllowedHeader,
  ItemTypeNotAllowedSite,
  ItemTypeAllowedHeader,
  ItemTypeAllowedSite,
};

}  // namespace

@interface SiteSettingsCategoryDetailViewController () <
    UISearchControllerDelegate,
    UISearchResultsUpdating,
    UISearchBarDelegate>

@end

@implementation SiteSettingsCategoryDetailViewController {
  SiteSettingsCategory _category;
  ContentSetting _defaultSetting;
  NSArray<SiteSettingsSiteException*>* _allAllowedSites;
  NSArray<SiteSettingsSiteException*>* _allNotAllowedSites;
  NSArray<SiteSettingsSiteException*>* _filteredAllowedSites;
  NSArray<SiteSettingsSiteException*>* _filteredNotAllowedSites;
  NSString* _searchTerm;
  UISearchController* _searchController;
}

- (instancetype)initWithCategory:(SiteSettingsCategory)category {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _category = category;
    _defaultSetting = CONTENT_SETTING_DEFAULT;
    _allAllowedSites = @[];
    _allNotAllowedSites = @[];
    _filteredAllowedSites = @[];
    _filteredNotAllowedSites = @[];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = [self categoryTitle];
  self.tableView.accessibilityIdentifier =
      kSiteSettingsCategoryDetailTableViewId;
  self.tableView.allowsMultipleSelectionDuringEditing = YES;

  _searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  _searchController.searchResultsUpdater = self;
  _searchController.obscuresBackgroundDuringPresentation = NO;
  _searchController.delegate = self;
  _searchController.searchBar.delegate = self;
  _searchController.searchBar.accessibilityIdentifier =
      kSiteSettingsCategoryDetailSearchBarId;

  self.navigationItem.searchController = _searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
  self.definesPresentationContext = YES;

  [self filterSitesForSearchTerm:_searchTerm];
  [self loadModel];
  [self updateUIForEditState];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateUIForEditState];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate siteSettingsCategoryDetailViewControllerWasRemoved:self];
  }
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];

  // 1. Default Setting Section.
  [self loadDefaultSettingSection];

  // 2. Not Allowed Section (only if non-empty).
  [self loadSitesSectionWithIdentifier:SectionIdentifierNotAllowed
                            headerType:ItemTypeNotAllowedHeader
                              itemType:ItemTypeNotAllowedSite
                             titleText:@"Not Allowed"
                            sectionTag:@"NotAllowed"
                                 sites:_filteredNotAllowedSites];

  // 3. Allowed Section (only if non-empty).
  [self loadSitesSectionWithIdentifier:SectionIdentifierAllowed
                            headerType:ItemTypeAllowedHeader
                              itemType:ItemTypeAllowedSite
                             titleText:@"Allowed"
                            sectionTag:@"Allowed"
                                 sites:_filteredAllowedSites];
}

- (BOOL)shouldHideToolbar {
  return self.navigationController &&
         self.navigationController.visibleViewController != self &&
         self.navigationController.topViewController != self;
}

- (BOOL)shouldShowEditDoneButton {
  return NO;
}

- (BOOL)editButtonEnabled {
  return _filteredAllowedSites.count > 0 || _filteredNotAllowedSites.count > 0;
}

- (void)updateUIForEditState {
  [super updateUIForEditState];
  [self updatedToolbarForEditState];
}

- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  NSMutableArray<SiteSettingsSiteException*>* sitesToDelete =
      [NSMutableArray arrayWithCapacity:indexPaths.count];
  for (NSIndexPath* indexPath in indexPaths) {
    SiteSettingsSiteException* siteException =
        [self siteExceptionForIndexPath:indexPath];
    if (siteException) {
      [sitesToDelete addObject:siteException];
    }
  }
  [self setEditing:NO animated:YES];
  [self updateUIForEditState];
  [self.mutator deleteSettingsForSites:sitesToDelete];
}

#pragma mark - SiteSettingsCategoryDetailConsumer

- (void)setDefaultSetting:(ContentSetting)setting {
  if (_defaultSetting == setting) {
    return;
  }
  _defaultSetting = setting;
  if (!self.tableViewModel) {
    return;
  }

  [self updateCheckmarkForItemType:ItemTypeDefaultSettingAsk
                          selected:_defaultSetting == CONTENT_SETTING_ASK];
  [self updateCheckmarkForItemType:ItemTypeDefaultSettingBlock
                          selected:_defaultSetting == CONTENT_SETTING_BLOCK];
}

- (void)setAllowedSites:(NSArray<SiteSettingsSiteException*>*)allowedSites
        notAllowedSites:(NSArray<SiteSettingsSiteException*>*)notAllowedSites {
  _allAllowedSites = [allowedSites copy];
  _allNotAllowedSites = [notAllowedSites copy];
  [self filterSitesForSearchTerm:_searchTerm];
  [self reloadSitesAndUpdateEditState];
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  return [item isKindOfClass:[TableViewURLItem class]];
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  if ([item isKindOfClass:[TableViewURLItem class]]) {
    [self loadFaviconForURLItem:base::apple::ObjCCastStrict<TableViewURLItem>(
                                    item)
                    atIndexPath:indexPath];
  }

  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.accessibilityActivationPointBlock = nil;
  if ([item isKindOfClass:[TableViewURLItem class]]) {
    [self configureSiteExceptionCell:cell atIndexPath:indexPath];
  }
  return cell;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* header = [super tableView:tableView viewForHeaderInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  if (sectionIdentifier != SectionIdentifierNotAllowed &&
      sectionIdentifier != SectionIdentifierAllowed) {
    return header;
  }

  header.tag = sectionIdentifier;
  for (UIGestureRecognizer* recognizer in header.gestureRecognizers) {
    [header removeGestureRecognizer:recognizer];
  }
  UITapGestureRecognizer* tapGesture = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleHeaderTap:)];
  [header addGestureRecognizer:tapGesture];

  return header;
}

#pragma mark - UITableViewDelegate

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSIndexPath* superIndexPath = [super tableView:tableView
                        willSelectRowAtIndexPath:indexPath];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:indexPath.section];
  BOOL isSiteExceptionSection =
      sectionIdentifier == SectionIdentifierNotAllowed ||
      sectionIdentifier == SectionIdentifierAllowed;
  if (tableView.editing) {
    return isSiteExceptionSection ? superIndexPath : nil;
  }
  return isSiteExceptionSection ? nil : superIndexPath;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  if (tableView.editing) {
    self.deleteButton.enabled = YES;
    return;
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeDefaultSettingAsk:
      [self.mutator setDefaultSetting:CONTENT_SETTING_ASK];
      break;
    case ItemTypeDefaultSettingBlock:
      [self.mutator setDefaultSetting:CONTENT_SETTING_BLOCK];
      break;
    default:
      break;
  }
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didDeselectRowAtIndexPath:indexPath];
  if (!tableView.editing) {
    return;
  }
  self.deleteButton.enabled = tableView.indexPathsForSelectedRows.count > 0;
}

- (UISwipeActionsConfiguration*)tableView:(UITableView*)tableView
    trailingSwipeActionsConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (tableView.editing) {
    return nil;
  }
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  if (![item isKindOfClass:[TableViewURLItem class]]) {
    return nil;
  }

  SiteSettingsSiteException* siteException =
      [self siteExceptionForIndexPath:indexPath];
  if (!siteException) {
    return nil;
  }

  __weak __typeof(self) weakSelf = self;
  UIContextualAction* deleteAction = [UIContextualAction
      contextualActionWithStyle:UIContextualActionStyleDestructive
                          title:l10n_util::GetNSString(
                                    IDS_IOS_DELETE_ACTION_TITLE)
                        handler:^(UIContextualAction* action,
                                  UIView* sourceView,
                                  void (^completionHandler)(BOOL)) {
                          // Complete the swipe action before mutating the
                          // setting, since the mutation synchronously triggers
                          // a full table reload that would otherwise interrupt
                          // the dismissal animation.
                          completionHandler(YES);
                          [weakSelf.mutator deleteSettingForSite:siteException];
                        }];
  return
      [UISwipeActionsConfiguration configurationWithActions:@[ deleteAction ]];
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  [self filterSitesForSearchTerm:searchController.searchBar.text];
  [self reloadSitesAndUpdateEditState];
}

#pragma mark - Private

// Updates edit mode and toolbar buttons when the visible site exceptions change
// and reloads the table view data.
- (void)reloadSitesAndUpdateEditState {
  if (self.isViewLoaded) {
    if (![self editButtonEnabled] && self.tableView.editing) {
      [self setEditing:NO animated:YES];
    }
    [self updateUIForEditState];
  }
  [self reloadData];
}

// Populates the default permission setting section with Ask and Block options.
- (void)loadDefaultSettingSection {
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierDefaultSetting];

  TableViewDetailIconItem* askItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeDefaultSettingAsk];
  askItem.text = [self askOptionTitle];
  askItem.iconImage = [self askOptionIcon];
  askItem.iconTintColor = [UIColor colorNamed:kGrey600Color];
  askItem.accessibilityIdentifier = kSiteSettingsCategoryDetailAskCellId;
  askItem.accessoryType = _defaultSetting == CONTENT_SETTING_ASK
                              ? UITableViewCellAccessoryCheckmark
                              : UITableViewCellAccessoryNone;
  [model addItem:askItem
      toSectionWithIdentifier:SectionIdentifierDefaultSetting];

  TableViewDetailIconItem* blockItem = [[TableViewDetailIconItem alloc]
      initWithType:ItemTypeDefaultSettingBlock];
  blockItem.text = [self blockOptionTitle];
  blockItem.detailText = [self blockOptionSubtitle];
  blockItem.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
  blockItem.iconImage = [self blockOptionIcon];
  blockItem.iconTintColor = [UIColor colorNamed:kGrey600Color];
  blockItem.accessibilityIdentifier = kSiteSettingsCategoryDetailBlockCellId;
  blockItem.accessoryType = _defaultSetting == CONTENT_SETTING_BLOCK
                                ? UITableViewCellAccessoryCheckmark
                                : UITableViewCellAccessoryNone;
  [model addItem:blockItem
      toSectionWithIdentifier:SectionIdentifierDefaultSetting];
}

// Adds a collapsible section for the given `sites` list if non-empty,
// configuring the header with item count, collapse state key, and site items.
- (void)loadSitesSectionWithIdentifier:(SectionIdentifier)sectionIdentifier
                            headerType:(ItemType)headerType
                              itemType:(ItemType)itemType
                             titleText:(NSString*)titleText
                            sectionTag:(NSString*)sectionTag
                                 sites:(NSArray<SiteSettingsSiteException*>*)
                                           sites {
  if (sites.count == 0) {
    return;
  }
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:sectionIdentifier];
  NSString* collapsedKey =
      [NSString stringWithFormat:@"SiteSettings_%d_%@",
                                 static_cast<int>(_category), sectionTag];
  [model setSectionIdentifier:sectionIdentifier collapsedKey:collapsedKey];

  TableViewDisclosureHeaderFooterItem* header =
      [[TableViewDisclosureHeaderFooterItem alloc] initWithType:headerType];
  // TODO(crbug.com/553098545): Use localized strings.
  header.text = [NSString
      stringWithFormat:@"%@ (%lu)", titleText, (unsigned long)sites.count];
  header.collapsed = [model sectionIsCollapsed:sectionIdentifier];
  [model setHeader:header forSectionWithIdentifier:sectionIdentifier];

  for (SiteSettingsSiteException* siteException in sites) {
    TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:itemType];
    if (!siteException.URL) {
      item.title = siteException.formattedTitle;
    }
    item.URL = siteException.URL;
    [model addItem:item toSectionWithIdentifier:sectionIdentifier];
  }
}

// Requests the favicon for `URLItem` at `indexPath` if it has not been loaded
// yet.
- (void)loadFaviconForURLItem:(TableViewURLItem*)URLItem
                  atIndexPath:(NSIndexPath*)indexPath {
  if (URLItem.faviconAttributes) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  [self.imageDataSource
      faviconForPageURL:URLItem.URL
             completion:^(FaviconAttributes* attributes, BOOL cached) {
               [weakSelf didFetchFaviconAttributes:attributes
                                              item:URLItem
                                         indexPath:indexPath];
             }];
}

// Configures the trailing popup menu button and VoiceOver properties on `cell`
// for the site exception at `indexPath`.
- (void)configureSiteExceptionCell:(UITableViewCell*)cell
                       atIndexPath:(NSIndexPath*)indexPath {
  SiteSettingsSiteException* siteException =
      [self siteExceptionForIndexPath:indexPath];
  if (!siteException) {
    return;
  }
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:indexPath.section];
  ContentSetting currentSetting = sectionIdentifier == SectionIdentifierAllowed
                                      ? CONTENT_SETTING_ALLOW
                                      : CONTENT_SETTING_BLOCK;

  UIButton* menuButton = [self menuButtonForSiteException:siteException
                                           currentSetting:currentSetting];
  cell.accessoryView = menuButton;
  // TODO(crbug.com/553098545): Use localized strings.
  cell.accessibilityValue =
      currentSetting == CONTENT_SETTING_ALLOW ? @"Allowed" : @"Not Allowed";
  __weak UITableViewCell* weakCell = cell;
  __weak UIView* weakButton = menuButton;
  cell.accessibilityActivationPointBlock = ^CGPoint() {
    if (weakCell.editing) {
      return [weakCell.contentView accessibilityActivationPoint];
    }
    return [weakButton accessibilityActivationPoint];
  };
}

// Creates a trailing popup menu button allowing the user to switch
// `siteException` between Allowed (`CONTENT_SETTING_ALLOW`) and Not Allowed
// (`CONTENT_SETTING_BLOCK`).
- (UIButton*)menuButtonForSiteException:
                 (SiteSettingsSiteException*)siteException
                         currentSetting:(ContentSetting)currentSetting {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImage* chevronImage =
      DefaultAccessorySymbolConfigurationWithRegularWeight(SymbolChevronUpDown);
  [button setImage:chevronImage forState:UIControlStateNormal];
  button.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
  button.showsMenuAsPrimaryAction = YES;

  __weak __typeof(self) weakSelf = self;
  // TODO(crbug.com/553098545): Use localized strings.
  UIAction* allowAction =
      [UIAction actionWithTitle:@"Allowed"
                          image:nil
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf.mutator setSetting:CONTENT_SETTING_ALLOW
                                               forSite:siteException];
                        }];
  allowAction.state = currentSetting == CONTENT_SETTING_ALLOW
                          ? UIMenuElementStateOn
                          : UIMenuElementStateOff;

  UIAction* notAllowedAction =
      [UIAction actionWithTitle:@"Not Allowed"
                          image:nil
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf.mutator setSetting:CONTENT_SETTING_BLOCK
                                               forSite:siteException];
                        }];
  notAllowedAction.state = currentSetting == CONTENT_SETTING_BLOCK
                               ? UIMenuElementStateOn
                               : UIMenuElementStateOff;

  button.menu = [UIMenu menuWithTitle:@""
                                image:nil
                           identifier:nil
                              options:UIMenuOptionsSingleSelection
                             children:@[ allowAction, notAllowedAction ]];
  [button sizeToFit];
  return button;
}

// Updates the accessory checkmark for the default setting row identified by
// `itemType`.
- (void)updateCheckmarkForItemType:(ItemType)itemType selected:(BOOL)selected {
  TableViewModel* model = self.tableViewModel;
  if (![model hasItemForItemType:itemType
               sectionIdentifier:SectionIdentifierDefaultSetting]) {
    return;
  }
  NSIndexPath* indexPath =
      [model indexPathForItemType:itemType
                sectionIdentifier:SectionIdentifierDefaultSetting];
  TableViewItem* item = [model itemAtIndexPath:indexPath];
  item.accessoryType = selected ? UITableViewCellAccessoryCheckmark
                                : UITableViewCellAccessoryNone;
  [self reconfigureCellsForItems:@[ item ]];
}

// Returns the `SiteSettingsSiteException` at `indexPath`, or nil if
// the index path does not correspond to an exception site row.
- (SiteSettingsSiteException*)siteExceptionForIndexPath:
    (NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:indexPath.section];
  NSArray<SiteSettingsSiteException*>* sites = nil;
  if (sectionIdentifier == SectionIdentifierNotAllowed) {
    sites = _filteredNotAllowedSites;
  } else if (sectionIdentifier == SectionIdentifierAllowed) {
    sites = _filteredAllowedSites;
  }

  if (indexPath.row >= 0 &&
      indexPath.row < static_cast<NSInteger>(sites.count)) {
    return sites[indexPath.row];
  }
  return nil;
}

// Handles tap gestures on section disclosure headers to toggle expansion.
- (void)handleHeaderTap:(UITapGestureRecognizer*)sender {
  if (sender.state != UIGestureRecognizerStateEnded) {
    return;
  }
  NSInteger sectionIdentifier = sender.view.tag;
  if (![self.tableViewModel hasSectionForSectionIdentifier:sectionIdentifier]) {
    return;
  }

  [self toggleExpansionOfSectionIdentifier:sectionIdentifier];
  [self updateDisclosureHeaderForSectionIdentifier:sectionIdentifier];
}

// Updates the disclosure chevron direction and collapsed state on the section
// header.
- (void)updateDisclosureHeaderForSectionIdentifier:
    (NSInteger)sectionIdentifier {
  NSInteger sectionIndex =
      [self.tableViewModel sectionForSectionIdentifier:sectionIdentifier];
  ListItem* headerItem =
      [self.tableViewModel headerForSectionIndex:sectionIndex];
  UITableViewHeaderFooterView* headerView =
      [self.tableView headerViewForSection:sectionIndex];
  TableViewDisclosureHeaderFooterView* disclosureHeaderView =
      base::apple::ObjCCast<TableViewDisclosureHeaderFooterView>(headerView);
  TableViewDisclosureHeaderFooterItem* disclosureItem =
      base::apple::ObjCCast<TableViewDisclosureHeaderFooterItem>(headerItem);
  BOOL collapsed = [self.tableViewModel sectionIsCollapsed:sectionIdentifier];
  DisclosureDirection direction =
      collapsed ? DisclosureDirectionTrailing : DisclosureDirectionDown;
  [disclosureHeaderView rotateToDirection:direction];
  disclosureItem.collapsed = collapsed;
}

// Animates expanding or collapsing rows within the specified section.
- (void)toggleExpansionOfSectionIdentifier:(NSInteger)sectionIdentifier {
  if (![self.tableViewModel hasSectionForSectionIdentifier:sectionIdentifier]) {
    return;
  }
  NSArray* items =
      [self.tableViewModel itemsInSectionWithIdentifier:sectionIdentifier];
  if ([items count] == 0) {
    return;
  }

  NSInteger sectionIndex =
      [self.tableViewModel sectionForSectionIdentifier:sectionIdentifier];
  NSMutableArray* cellIndexPathsToDeleteOrInsert = [NSMutableArray array];
  for (NSUInteger i = 0; i < [items count]; i++) {
    NSIndexPath* indexPath = [NSIndexPath indexPathForRow:i
                                                inSection:sectionIndex];
    [cellIndexPathsToDeleteOrInsert addObject:indexPath];
  }

  void (^tableUpdates)(void) = ^{
    if ([self.tableViewModel sectionIsCollapsed:sectionIdentifier]) {
      [self.tableViewModel setSection:sectionIdentifier collapsed:NO];
      [self.tableView insertRowsAtIndexPaths:cellIndexPathsToDeleteOrInsert
                            withRowAnimation:UITableViewRowAnimationFade];
    } else {
      [self.tableViewModel setSection:sectionIdentifier collapsed:YES];
      [self.tableView deleteRowsAtIndexPaths:cellIndexPathsToDeleteOrInsert
                            withRowAnimation:UITableViewRowAnimationFade];
    }
  };

  [self.tableView performBatchUpdates:tableUpdates completion:nil];
  if (self.tableView.editing) {
    self.deleteButton.enabled =
        self.tableView.indexPathsForSelectedRows.count > 0;
  }
}

// Filters allowed and not allowed sites matching `searchTerm` against domain
// and origin strings.
- (void)filterSitesForSearchTerm:(NSString*)searchTerm {
  _searchTerm = [searchTerm copy];
  if (_searchTerm.length == 0) {
    _filteredAllowedSites = _allAllowedSites ?: @[];
    _filteredNotAllowedSites = _allNotAllowedSites ?: @[];
    return;
  }

  NSPredicate* predicate = [NSPredicate
      predicateWithBlock:^BOOL(SiteSettingsSiteException* siteException,
                               NSDictionary<NSString*, id>* bindings) {
        return [siteException.formattedTitle
                   localizedCaseInsensitiveContainsString:searchTerm] ||
               [siteException.origin
                   localizedCaseInsensitiveContainsString:searchTerm];
      }];
  _filteredAllowedSites =
      [_allAllowedSites filteredArrayUsingPredicate:predicate];
  _filteredNotAllowedSites =
      [_allNotAllowedSites filteredArrayUsingPredicate:predicate];
}

// Updates the favicon image on `item` once attributes have been resolved.
- (void)didFetchFaviconAttributes:(FaviconAttributes*)attributes
                             item:(TableViewURLItem*)item
                        indexPath:(NSIndexPath*)indexPath {
  if (!attributes) {
    return;
  }
  item.faviconAttributes = attributes;
  [self reconfigureCellsForItems:@[ item ]];
}

#pragma mark - Copy & Icons

// Returns the navigation bar title for the current permission category.
- (NSString*)categoryTitle {
  // TODO(crbug.com/553098545): Use localized strings.
  switch (_category) {
    case SiteSettingsCategory::kMicrophone:
      return @"Microphone";
    case SiteSettingsCategory::kCamera:
      return @"Camera";
    case SiteSettingsCategory::kLocation:
      return @"Location";
  }
}

// Returns the primary title for the "Ask" permission choice.
- (NSString*)askOptionTitle {
  // TODO(crbug.com/553098545): Use localized strings.
  switch (_category) {
    case SiteSettingsCategory::kMicrophone:
      return @"Sites can ask for your microphone";
    case SiteSettingsCategory::kCamera:
      return @"Sites can ask for your camera";
    case SiteSettingsCategory::kLocation:
      return @"Sites can ask for your location";
  }
}

// Returns the primary title for the "Block" permission choice.
- (NSString*)blockOptionTitle {
  // TODO(crbug.com/553098545): Use localized strings.
  switch (_category) {
    case SiteSettingsCategory::kMicrophone:
      return @"Don't allow sites to use your microphone";
    case SiteSettingsCategory::kCamera:
      return @"Don't allow sites to use your camera";
    case SiteSettingsCategory::kLocation:
      return @"Don't allow sites to use your location";
  }
}

// Returns the explanatory subtitle for the "Block" permission choice.
- (NSString*)blockOptionSubtitle {
  // TODO(crbug.com/553098545): Use localized strings.
  switch (_category) {
    case SiteSettingsCategory::kMicrophone:
      return @"Features that need a microphone won't work";
    case SiteSettingsCategory::kCamera:
      return @"Features that need a camera won't work";
    case SiteSettingsCategory::kLocation:
      return @"Features that need your location won't work";
  }
}

// Returns the icon for the "Ask" permission choice.
- (UIImage*)askOptionIcon {
  Symbol symbol;
  switch (_category) {
    case SiteSettingsCategory::kMicrophone:
      symbol = SymbolMicrophone;
      break;
    case SiteSettingsCategory::kCamera:
      symbol = SymbolSystemCamera;
      break;
    case SiteSettingsCategory::kLocation:
      symbol = SymbolLocation;
      break;
  }
  return SymbolWithPointSize(symbol, kSettingsRootSymbolImagePointSize);
}

// Returns the slashed icon for the "Block" permission choice.
- (UIImage*)blockOptionIcon {
  Symbol symbol;
  switch (_category) {
    case SiteSettingsCategory::kMicrophone:
      symbol = SymbolMicrophoneSlash;
      break;
    case SiteSettingsCategory::kCamera:
      symbol = SymbolCameraSlash;
      break;
    case SiteSettingsCategory::kLocation:
      symbol = SymbolLocationSlash;
      break;
  }
  return SymbolWithPointSize(symbol, kSettingsRootSymbolImagePointSize);
}

@end
