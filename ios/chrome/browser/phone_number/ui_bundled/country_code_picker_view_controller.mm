// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/phone_number/ui_bundled/country_code_picker_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/phone_number/ui_bundled/phone_number_actions_view_controller.h"
#import "ios/chrome/browser/phone_number/ui_bundled/phone_number_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "third_party/libphonenumber/phonenumber_api.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using i18n::phonenumbers::PhoneNumberUtil;

const CGFloat kCountryCodeViewTitleWidth = 200;
const CGFloat kCountryCodeViewTitleHeight = 40;

const CGFloat kCountryCodeLabelTitleHeight = 22;

const CGFloat kPhoneNumberLabelTitleHeight = 22;
const CGFloat kPhoneNumberYPos = 22;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierCountrieCodes = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCountryCode = kItemTypeEnumZero,
};

}  // namespace

@interface CountryCodePickerViewController () <UISearchResultsUpdating>

// Scrim overlay covering the entire tableView when the search bar is focused.
@property(nonatomic, strong) UIControl* scrimView;

@end

@implementation CountryCodePickerViewController {
  // The detected and passed phone number.
  NSString* _phoneNumber;

  // The displayed phone number on title, changes based on the selected country
  // code.
  NSString* _displayedPhoneNumber;

  // List of pairs (country,country code).
  NSArray* _countriesAndCodes;

  // Saved index of the item that was last selected. Can be nil.
  NSIndexPath* _selectedIndexPath;

  // Last selected country code.
  NSString* _selectedCountryCode;

  // Last selected country. Can be nil.
  NSString* _selectedCountry;

  // CountryCodeViewController's search controller.
  UISearchController* _searchController;

  // The current search filter. Can be nil.
  NSPredicate* _searchPredicate;

  // Cancel button item in navigation bar.
  UIBarButtonItem* _cancelButton;

  // Add button item in navigation bar.
  UIBarButtonItem* _addButton;
}

- (instancetype)initWithPhoneNumber:(NSString*)phoneNumber {
  self = [super initWithStyle:UITableViewStyleInsetGrouped];

  if (self) {
    _phoneNumber = phoneNumber;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  PhoneNumberUtil* phone_number_util = PhoneNumberUtil::GetInstance();
  std::set<std::string> regions;
  std::set<int> country_codes;
  phone_number_util->GetSupportedRegions(&regions);
  phone_number_util->GetSupportedGlobalNetworkCallingCodes(&country_codes);
  NSMutableArray* countriesAndCodes = [[NSMutableArray alloc] init];
  for (auto region : regions) {
    NSString* countryCode = base::SysUTF8ToNSString(region);
    NSString* identifier =
        [NSLocale localeIdentifierFromComponents:
                      [NSDictionary dictionaryWithObject:countryCode
                                                  forKey:NSLocaleCountryCode]];
    NSString* country =
        [[NSLocale currentLocale] displayNameForKey:NSLocaleIdentifier
                                              value:identifier];
    [countriesAndCodes addObject:@{
      @"country" : country,
      @"code" : [NSString
          stringWithFormat:@"+%d", phone_number_util->GetCountryCodeForRegion(
                                       base::SysNSStringToUTF8(countryCode))]
    }];
  }
  _countriesAndCodes = [countriesAndCodes sortedArrayUsingDescriptors:@[
    [NSSortDescriptor sortDescriptorWithKey:@"country" ascending:YES]
  ]];

  _displayedPhoneNumber =
      [NSString stringWithFormat:@"+(%@) %@", @" ", _phoneNumber];
  _searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  _searchController.searchResultsUpdater = self;
  self.navigationItem.searchController = _searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
  self.navigationItem.preferredSearchBarPlacement =
      UINavigationItemSearchBarPlacementStacked;

  [self updateTitle];

  _cancelButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_PHONE_NUMBER_CANCEL)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(cancelButtonTapped:)];
  self.navigationItem.leftBarButtonItem = _cancelButton;

  _addButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_PHONE_NUMBER_ADD)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(AddButtonTapped:)];
  _addButton.enabled = NO;
  self.navigationItem.rightBarButtonItem = _addButton;

  self.scrimView = [[UIControl alloc] init];
  self.scrimView.alpha = 0.0f;
  self.scrimView.backgroundColor = [UIColor colorNamed:kScrimBackgroundColor];
  self.scrimView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.scrimView addTarget:self
                     action:@selector(dismissSearchController:)
           forControlEvents:UIControlEventTouchUpInside];
  self.tableView.accessibilityIdentifier =
      kCountryCodePickerTableViewIdentifier;
  [self loadModel];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitVerticalSizeClass.self ]);
    [self registerForTraitChanges:traits withAction:@selector(updateTitle)];
  }
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierCountrieCodes];
  [_countriesAndCodes enumerateObjectsUsingBlock:^(NSDictionary* countryAndCode,
                                                   NSUInteger index,
                                                   BOOL* stop) {
    TableViewDetailIconItem* item =
        [[TableViewDetailIconItem alloc] initWithType:ItemTypeCountryCode];
    item.text = countryAndCode[@"country"];
    item.detailText = countryAndCode[@"code"];
    item.accessibilityTraits = UIAccessibilityTraitButton;
    [model addItem:item toSectionWithIdentifier:SectionIdentifierCountrieCodes];
  }];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSIndexPath* previouslySelectedIndexPath = _selectedIndexPath;
  [self.tableView deselectRowAtIndexPath:indexPath animated:NO];

  if (previouslySelectedIndexPath != nil) {
    UITableViewCell* previouslySelectedCell =
        [tableView cellForRowAtIndexPath:previouslySelectedIndexPath];
    previouslySelectedCell.accessoryType = UITableViewCellAccessoryNone;
    if (previouslySelectedIndexPath == indexPath) {
      _selectedIndexPath = nil;
      _selectedCountryCode = @" ";
      _selectedCountry = nil;
      _addButton.enabled = NO;
      _displayedPhoneNumber = [NSString
          stringWithFormat:@"+(%@) %@", _selectedCountryCode, _phoneNumber];
      [self updateTitle];
      return;
    }
  }
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  cell.accessoryType = UITableViewCellAccessoryCheckmark;
  _selectedIndexPath = indexPath;
  _selectedCountry = cell.textLabel.text;
  _selectedCountryCode = [cell.detailTextLabel.text
      substringWithRange:NSMakeRange(1,
                                     [cell.detailTextLabel.text length] - 1)];
  _addButton.enabled = YES;
  _displayedPhoneNumber = [NSString
      stringWithFormat:@"+(%@) %@", _selectedCountryCode, _phoneNumber];

  if (_searchController.active) {
    _searchController.active = NO;
    [self.tableView scrollToRowAtIndexPath:_selectedIndexPath
                          atScrollPosition:UITableViewScrollPositionMiddle
                                  animated:NO];
  }

  [self updateTitle];
}

#pragma mark - UIView

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  [self updateTitle];
}
#endif

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  NSString* searchText = searchController.searchBar.text;

  if (searchText.length == 0 && _searchController.active) {
    [self showScrim];
  } else {
    [self hideScrim];
  }

  if (searchText.length != 0) {
    _searchPredicate = [NSPredicate
        predicateWithFormat:@"country contains[c] %@ OR code contains[c] %@",
                            searchText, searchText];

  } else {
    _searchPredicate = nil;
  }

  [self updateCountriesSection];
}

#pragma mark - Helper methods

// Action when the cancel is tapped.
- (void)cancelButtonTapped:(UIBarButtonItem*)item {
  [self.navigationController dismissViewControllerAnimated:YES completion:nil];
}

// Action when the add button is tapped.
- (void)AddButtonTapped:(UIBarButtonItem*)item {
  PhoneNumberActionsViewController* viewController =
      [[PhoneNumberActionsViewController alloc]
          initWithPhoneNumber:_displayedPhoneNumber];
  viewController.addContactsHandler = self.addContactsHandler;
  [self.navigationController pushViewController:viewController animated:YES];
  return;
}

// Updates the phone number in the title based on the selected country code.
- (void)updateTitle {
  UILabel* descriptionLabel;
  UIView* titleView;
  if (self.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    titleView = [[UIView alloc]
        initWithFrame:CGRectMake(0, 0, kCountryCodeViewTitleWidth,
                                 kPhoneNumberLabelTitleHeight)];
    descriptionLabel = [[UILabel alloc]
        initWithFrame:CGRectMake(0, 0, titleView.frame.size.width,
                                 kCountryCodeLabelTitleHeight)];
  } else {
    titleView = [[UIView alloc]
        initWithFrame:CGRectMake(0, 0, kCountryCodeViewTitleWidth,
                                 kCountryCodeViewTitleHeight)];

    UILabel* titleLabel = [[UILabel alloc]
        initWithFrame:CGRectMake(0, 0, titleView.frame.size.width,
                                 kCountryCodeLabelTitleHeight)];
    titleLabel.text = l10n_util::GetNSString(IDS_IOS_PHONE_NUMBER_COUNTRY_CODE);
    titleLabel.font = [UIFont systemFontOfSize:14 weight:UIFontWeightSemibold];
    titleLabel.textAlignment = NSTextAlignmentCenter;
    [titleView addSubview:titleLabel];

    descriptionLabel = [[UILabel alloc]
        initWithFrame:CGRectMake(0, kPhoneNumberYPos,
                                 titleView.frame.size.width,
                                 kPhoneNumberLabelTitleHeight)];
  }
  descriptionLabel.text = _displayedPhoneNumber;
  descriptionLabel.font = [UIFont systemFontOfSize:11
                                            weight:UIFontWeightRegular];
  descriptionLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  descriptionLabel.textAlignment = NSTextAlignmentCenter;
  [titleView addSubview:descriptionLabel];
  self.navigationItem.titleView = titleView;
}

// Updates the table view controller with the list of filtered countries.
- (void)updateCountriesSection {
  // Update the model.
  [self.tableViewModel
      deleteAllItemsFromSectionWithIdentifier:SectionIdentifierCountrieCodes];
  [self populateCountriesSection];

  // Update the table view.
  NSUInteger index = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierCountrieCodes];
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationAutomatic];
}

// Updates the list of coutries based on the search text.
- (void)populateCountriesSection {
  TableViewModel* model = self.tableViewModel;

  NSArray* filteredCountries = _countriesAndCodes;
  if (_searchPredicate) {
    filteredCountries =
        [_countriesAndCodes filteredArrayUsingPredicate:_searchPredicate];
  }

  __block TableViewDetailIconItem* selectedItem = nil;
  [filteredCountries enumerateObjectsUsingBlock:^(NSDictionary* countryAndCode,
                                                  NSUInteger index,
                                                  BOOL* stop) {
    TableViewDetailIconItem* item =
        [[TableViewDetailIconItem alloc] initWithType:ItemTypeCountryCode];
    item.text = countryAndCode[@"country"];
    item.detailText = countryAndCode[@"code"];

    if (_selectedCountry != nil &&
        [countryAndCode[@"country"] isEqualToString:_selectedCountry]) {
      selectedItem = item;
      item.accessoryType = UITableViewCellAccessoryCheckmark;
    }
    [model addItem:item toSectionWithIdentifier:SectionIdentifierCountrieCodes];
  }];

  if (selectedItem) {
    _selectedIndexPath = [model indexPathForItem:selectedItem];
  }
}

// Shows the scrim overlay.
- (void)showScrim {
  if (self.scrimView.alpha > 1.0f) {
    return;
  }

  self.scrimView.alpha = 0.0f;
  _searchController.searchBar.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  self.navigationController.navigationBar.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  [self.tableView addSubview:self.scrimView];
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

// Hides the scrim overlay.
- (void)hideScrim {
  if (self.scrimView.alpha < 0.0f) {
    return;
  }

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

// Dismisses the search controller when the scrim overlay is tapped.
- (void)dismissSearchController:(UIControl*)sender {
  if (_searchController.active) {
    _searchController.active = NO;
  }
}

@end
