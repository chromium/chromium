// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/language/add_language_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/language/cells/language_item.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_data_source.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_histograms.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_ui_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierLanguages = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeLanguage = kItemTypeEnumZero,  // This is a repeating type.
};

}  // namespace

@interface AddLanguageTableViewController () <UISearchResultsUpdating> {
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

// The data source passed to this instance.
@property(nonatomic, strong) id<LanguageSettingsDataSource> dataSource;

// The delegate passed to this instance.
@property(nonatomic, weak) id<AddLanguageTableViewControllerDelegate> delegate;

// This ViewController's search controller.
@property(nonatomic, strong) UISearchController* searchController;

// The list of supported languages fetched from the data source.
@property(nonatomic, strong) NSArray<LanguageItem*>* supportedLanguages;

// The current search filter. May be nil.
@property(nonatomic, strong) NSPredicate* searchPredicate;

// Scrim overlay covering the entire tableView when the search bar is focused.
@property(nonatomic, strong) UIControl* scrimView;

@end

@implementation AddLanguageTableViewController

- (instancetype)initWithDataSource:(id<LanguageSettingsDataSource>)dataSource
                          delegate:(id<AddLanguageTableViewControllerDelegate>)
                                       delegate {
  DCHECK(dataSource);
  DCHECK(delegate);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _dataSource = dataSource;
    _delegate = delegate;

    UMA_HISTOGRAM_ENUMERATION(kLanguageSettingsPageImpressionHistogram,
                              LanguageSettingsPages::PAGE_ADD_LANGUAGE);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title =
      l10n_util::GetNSString(IDS_IOS_LANGUAGE_SETTINGS_ADD_LANGUAGE_TITLE);
  self.shouldHideDoneButton = YES;
  self.tableView.accessibilityIdentifier =
      kAddLanguageTableViewAccessibilityIdentifier;

  // Search controller.
  self.searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.searchController.searchResultsUpdater = self;
  self.searchController.searchBar.accessibilityIdentifier =
      kAddLanguageSearchControllerAccessibilityIdentifier;
  // Presentation of searchController will walk up the view controller hierarchy
  // until it finds the root view controller or one that defines a presentation
  // context. Make this view controller the presentation context so that the
  // searchController does not present on top of the navigation controller.
  self.definesPresentationContext = YES;
  // Place the search bar in the navigation bar.
  self.navigationItem.searchController = self.searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;

  // Scrim.
  self.scrimView = [[UIControl alloc] init];
  self.scrimView.alpha = 0.0f;
  self.scrimView.backgroundColor = [UIColor colorNamed:kScrimBackgroundColor];
  self.scrimView.translatesAutoresizingMaskIntoConstraints = NO;
  self.scrimView.accessibilityIdentifier =
      kAddLanguageSearchScrimAccessibilityIdentifier;
  [self.scrimView addTarget:self
                     action:@selector(dismissSearchController:)
           forControlEvents:UIControlEventTouchUpInside];

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierLanguages];
  [self populateLanguagesSectionFromDataSource:YES];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  LanguageItem* languageItem = base::apple::ObjCCastStrict<LanguageItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);

  [self.delegate addLanguageTableViewController:self
                          didSelectLanguageCode:languageItem.languageCode];
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
                          @"$searchText OR leadingDetailText "
                          @"CONTAINS[cd] $searchText"]
      predicateWithSubstitutionVariables:@{@"searchText" : searchText}];

  // Show the scrim overlay only if the search text is empty and the search
  // controller is active (it is not being dismissed); Otherwise hide it.
  if (searchText.length == 0 && self.searchController.active) {
    [self showScrim];
  } else {
    [self hideScrim];
  }

  [self updateLanguagesSectionFromDataSource:NO];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
}

- (void)reportBackUserAction {
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  // No-op as there are no C++ objects or observers.

  _settingsAreDismissed = YES;
}

#pragma mark - Public methods

- (void)supportedLanguagesListChanged {
  // Update the model and the table view.
  [self updateLanguagesSectionFromDataSource:YES];
}

#pragma mark - Helper methods

// Populates the language items in the language section. Queries the data source
// if `fromDataSource` is true. Otherwise uses the previously loaded items.
- (void)populateLanguagesSectionFromDataSource:(BOOL)fromDataSource {
  TableViewModel* model = self.tableViewModel;

  if (fromDataSource) {
    self.supportedLanguages = [self.dataSource supportedLanguagesItems];
  }

  // Filter the language items based on the current search text, if applicable.
  NSArray<LanguageItem*>* filteredSupportedLanguages = self.supportedLanguages;
  if (self.searchPredicate) {
    filteredSupportedLanguages = [self.supportedLanguages
        filteredArrayUsingPredicate:self.searchPredicate];
  }

  // Languages items.
  [filteredSupportedLanguages
      enumerateObjectsUsingBlock:^(LanguageItem* item, NSUInteger index,
                                   BOOL* stop) {
        item.type = ItemTypeLanguage;
        [model addItem:item toSectionWithIdentifier:SectionIdentifierLanguages];
      }];
}

// Reloads the language items in the language section. Queries the data source
// if `fromDataSource` is true. Otherwise uses the previously loaded items.
- (void)updateLanguagesSectionFromDataSource:(BOOL)fromDataSource {
  // Update the model.
  [self.tableViewModel
      deleteAllItemsFromSectionWithIdentifier:SectionIdentifierLanguages];
  [self populateLanguagesSectionFromDataSource:fromDataSource];

  // Update the table view.
  NSUInteger index = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierLanguages];
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationNone];
}

// Shows the scrim overlay.
- (void)showScrim {
  self.tableView.accessibilityElementsHidden = YES;
  self.tableView.scrollEnabled = NO;
  [self.tableView addSubview:self.scrimView];
  // Attach constraints to the superview because tableView is a scrollView and
  // the scrim view will have an empty frame when attaching constraints to it.
  AddSameConstraints(self.scrimView, self.tableView.superview);
  [UIView animateWithDuration:kTableViewNavigationScrimFadeDuration
                   animations:^{
                     self.scrimView.alpha = 1.0f;
                     [self.view layoutIfNeeded];
                   }];
}

// Hides the scrim overlay.
- (void)hideScrim {
  [UIView animateWithDuration:kTableViewNavigationScrimFadeDuration
      animations:^{
        self.scrimView.alpha = 0.0f;
      }
      completion:^(BOOL finished) {
        [self.scrimView removeFromSuperview];
        self.tableView.accessibilityElementsHidden = NO;
        self.tableView.scrollEnabled = YES;
      }];
}

// Dismisses the search controller when the scrim overlay is tapped.
- (void)dismissSearchController:(UIControl*)sender {
  self.searchController.active = NO;
}

@end
