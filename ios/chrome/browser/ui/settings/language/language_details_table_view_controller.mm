// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/language/language_details_table_view_controller.h"

#import "base/feature_list.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/settings/language/cells/language_item.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_data_source.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_histograms.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_ui_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierOptions = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeNeverTranslate = kItemTypeEnumZero,
  ItemTypeOfferTranslate,
};

}  // namespace

@interface LanguageDetailsTableViewController () {
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

// The model data passed to this instance.
@property(nonatomic, strong) LanguageItem* languageItem;

// The delegate passed to this instance.
@property(nonatomic, weak) id<LanguageDetailsTableViewControllerDelegate>
    delegate;

@end

@implementation LanguageDetailsTableViewController

- (instancetype)initWithLanguageItem:(LanguageItem*)languageItem
                            delegate:
                                (id<LanguageDetailsTableViewControllerDelegate>)
                                    delegate {
  DCHECK(languageItem);
  DCHECK(delegate);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _languageItem = languageItem;
    _delegate = delegate;

    UMA_HISTOGRAM_ENUMERATION(kLanguageSettingsPageImpressionHistogram,
                              LanguageSettingsPages::PAGE_LANGUAGE_DETAILS);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = self.languageItem.text;
  self.shouldHideDoneButton = YES;
  self.tableView.accessibilityIdentifier =
      kLanguageDetailsTableViewAccessibilityIdentifier;

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierOptions];

  // Never translate item.
  TableViewTextItem* neverTranslateItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeNeverTranslate];
  neverTranslateItem.text =
      l10n_util::GetNSString(IDS_IOS_LANGUAGE_SETTINGS_NEVER_TRANSLATE_TITLE);
  neverTranslateItem.accessibilityTraits |= UIAccessibilityTraitButton;
  if ([self.languageItem isBlocked]) {
    neverTranslateItem.accessibilityTraits |= UIAccessibilityTraitSelected;
    neverTranslateItem.accessoryType = UITableViewCellAccessoryCheckmark;
  }
  [model addItem:neverTranslateItem
      toSectionWithIdentifier:SectionIdentifierOptions];

  // Offer to translate item.
  TableViewTextItem* offerTranslateItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeOfferTranslate];
  offerTranslateItem.text = l10n_util::GetNSString(
      IDS_IOS_LANGUAGE_SETTINGS_OFFER_TO_TRANSLATE_TITLE);
  offerTranslateItem.accessibilityTraits |= UIAccessibilityTraitButton;
  if (![self.languageItem isBlocked]) {
    offerTranslateItem.accessibilityTraits |= UIAccessibilityTraitSelected;
    offerTranslateItem.accessoryType = UITableViewCellAccessoryCheckmark;
  }
  if (!self.languageItem.canOfferTranslate) {
    offerTranslateItem.enabled = NO;
    offerTranslateItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  [model addItem:offerTranslateItem
      toSectionWithIdentifier:SectionIdentifierOptions];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.tableViewModel itemTypeForIndexPath:indexPath];
  [self.delegate
      languageDetailsTableViewController:self
                 didSelectOfferTranslate:(type == ItemTypeOfferTranslate)
                            languageCode:self.languageItem.languageCode];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileLanguageDetailsSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileLanguageDetailsSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  _settingsAreDismissed = YES;
}

@end
