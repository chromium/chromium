// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/autofill_country_selection_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/cells/country_item.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierCountries = kSectionIdentifierEnumZero,
};

}  // namespace

@interface AutofillCountrySelectionTableViewController () <
    UISearchControllerDelegate,
    UISearchResultsUpdating> {
}

// The delegate passed to this instance.
@property(nonatomic, weak)
    id<AutofillCountrySelectionTableViewControllerDelegate>
        delegate;

// This ViewController's search controller.
@property(nonatomic, strong) UISearchController* searchController;

// Denotes the currently selected country.
@property(nonatomic, strong) NSString* currentlySelectedCountry;

// The current search filter. May be nil.
@property(nonatomic, strong) NSPredicate* searchPredicate;

// Scrim overlay covering the entire tableView when the search bar is focused.
@property(nonatomic, strong) UIControl* scrimView;

// The fetched country list.
@property(nonatomic, weak) NSArray<CountryItem*>* allCountries;

// If YES, denotes that the view is shown in the settings.
@property(nonatomic, assign) BOOL settingsView;

@end

@implementation AutofillCountrySelectionTableViewController

- (instancetype)initWithDelegate:
                    (id<AutofillCountrySelectionTableViewControllerDelegate>)
                        delegate
                 selectedCountry:(NSString*)selectedCountry
                    allCountries:(NSArray<CountryItem*>*)allCountries
                    settingsView:(BOOL)settingsView {
  DCHECK(delegate);

  self = [super initWithStyle:(settingsView ? ChromeTableViewStyle()
                                            : UITableViewStylePlain)];
  if (self) {
    _delegate = delegate;
    _currentlySelectedCountry = selectedCountry;
    _allCountries = allCountries;
    _settingsView = settingsView;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  if (!self.settingsView) {
    self.title = l10n_util::GetNSString(IDS_IOS_AUTOFILL_SELECT_COUNTRY);
    self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    self.tableView.sectionHeaderHeight = 0;
    self.tableView.sectionFooterHeight = 0;
  } else {
    self.title = l10n_util::GetNSString(IDS_IOS_AUTOFILL_EDIT_ADDRESS);
  }
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Search controller.
  self.searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.searchController.searchResultsUpdater = self;
  self.searchController.searchBar.accessibilityIdentifier =
      kAutofillCountrySelectionTableViewId;
  // Presentation of searchController will walk up the view controller hierarchy
  // until it finds the root view controller or one that defines a presentation
  // context. Make this view controller the presentation context so that the
  // searchController does not present on top of the navigation controller.
  self.definesPresentationContext = YES;
  // Place the search bar in the navigation bar.
  self.navigationItem.searchController = self.searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeAutomatic;

  // Scrim.
  self.scrimView = [[UIControl alloc] init];
  self.scrimView.alpha = 0.0f;
  self.scrimView.backgroundColor = UIColor.clearColor;
  self.scrimView.translatesAutoresizingMaskIntoConstraints = NO;
  self.scrimView.accessibilityIdentifier =
      kAutofillCountrySelectionSearchScrimId;
  [self.scrimView addTarget:self
                     action:@selector(dismissSearchController:)
           forControlEvents:UIControlEventTouchUpInside];

  [self loadModel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Autoscroll to the selected country.
  for (CountryItem* item in [self.tableViewModel
           itemsInSectionWithIdentifier:SectionIdentifierCountries]) {
    if (item.accessoryType == UITableViewCellAccessoryCheckmark) {
      NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
      [self.tableView scrollToRowAtIndexPath:indexPath
                            atScrollPosition:UITableViewScrollPositionMiddle
                                    animated:NO];
    }
  }
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierCountries];
  [self populateCountriesSection];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (@available(iOS 16.0, *)) {
    return;
  }

  CountryItem* item = base::apple::ObjCCastStrict<CountryItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  [self.delegate didSelectCountry:item];
}

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  CountryItem* item = base::apple::ObjCCastStrict<CountryItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  [self.delegate didSelectCountry:item];
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  NSString* searchText = searchController.searchBar.text;

  // Set the current search filter to filter the languages based on the display
  // name of the language in the current locale and the language locale. The
  // search is case insensitive and diacritic insensitive. If the search text is
  // empty all languages will be displayed.
  self.searchPredicate = [[NSPredicate
      predicateWithFormat:@"$searchText.length == 0 OR text CONTAINS[cd] "
                          @"$searchText"]
      predicateWithSubstitutionVariables:@{@"searchText" : searchText}];

  // Show the scrim overlay only if the search text is empty and the search
  // controller is active (it is not being dismissed); Otherwise hide it.
  if (searchText.length == 0 && self.searchController.active) {
    [self showScrim];
  } else {
    [self hideScrim];
  }

  [self updateCountriesSection];
}

#pragma mark - Helper methods

// Populates the country items in the section.
- (void)populateCountriesSection {
  TableViewModel* model = self.tableViewModel;

  // Filter the countries items based on the current search text, if applicable.
  NSArray<CountryItem*>* filteredSupportedCountries = self.allCountries;
  if (self.searchPredicate) {
    filteredSupportedCountries = [filteredSupportedCountries
        filteredArrayUsingPredicate:self.searchPredicate];
  }

  for (CountryItem* item in filteredSupportedCountries) {
    [model addItem:item toSectionWithIdentifier:SectionIdentifierCountries];
  }
}

// Shows scrim overlay and hide toolbar.
- (void)showScrim {
  if (self.scrimView.alpha < 1.0f) {
    self.scrimView.alpha = 0.0f;
    [self.tableView addSubview:self.scrimView];
    // We attach our constraints to the superview because the tableView is
    // a scrollView and it seems that we get an empty frame when attaching to
    // it.
    AddSameConstraints(self.scrimView, self.view.superview);
    self.tableView.accessibilityElementsHidden = YES;
    self.tableView.scrollEnabled = NO;
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:kTableViewNavigationScrimFadeDuration
                     animations:^{
                       weakSelf.scrimView.alpha = 1.0f;
                       [weakSelf.view layoutIfNeeded];
                     }];
  }
}

// Hides scrim and restore toolbar.
- (void)hideScrim {
  if (self.scrimView.alpha > 0.0f) {
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:kTableViewNavigationScrimFadeDuration
        animations:^{
          weakSelf.scrimView.alpha = 0.0f;
        }
        completion:^(BOOL finished) {
          [weakSelf.scrimView removeFromSuperview];
          weakSelf.tableView.accessibilityElementsHidden = NO;
          weakSelf.tableView.scrollEnabled = YES;
        }];
  }
}

// Dismisses the search controller when the scrim overlay is tapped.
- (void)dismissSearchController:(UIControl*)sender {
  self.searchController.active = NO;
}

// Reloads the countries items in thes ection.
- (void)updateCountriesSection {
  // Update the model.
  [self.tableViewModel
      deleteAllItemsFromSectionWithIdentifier:SectionIdentifierCountries];
  [self populateCountriesSection];

  // Update the table view.
  NSUInteger index = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierCountries];
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationNone];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // TODO(crbug.com/1407666): Record for this VC.
}

- (void)reportBackUserAction {
  // TODO(crbug.com/1407666): Record for this VC.
}

@end
