// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/public/autofill_and_passwords_constants.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

enum SectionIdentifier : NSInteger {
  kSignInSection = kSectionIdentifierEnumZero,
  kBasicsSection,
};

enum ItemType : NSInteger {
  kSigninPromoItem = kItemTypeEnumZero,
  kPasswordsItem,
  kAutofillCreditCardItem,
  kAutofillProfileItem,
  kIdentityDocsItem,
  kTravelInfoItem,
  kShoppingInfoItem,
  kSuggestionsFromGeminiItem,
  kAutofillSettingsItem,
};

// Returns the localized "On" or "Off" detail text for `enabled`.
NSString* DetailTextForEnabledState(bool enabled) {
  return enabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                 : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
}

// Returns a configured `TableViewDetailIconItem` for an Autofill and Passwords
// subpage row.
TableViewDetailIconItem* DetailItemWithType(
    ItemType type,
    NSString* text,
    NSString* detail_text,
    NSString* trailing_detail_text,
    UIImage* symbol,
    NSString* accessibility_identifier) {
  TableViewDetailIconItem* detail_item =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detail_item.text = text;
  detail_item.detailText = detail_text;
  detail_item.trailingDetailText = trailing_detail_text;
  detail_item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detail_item.accessibilityTraits |= UIAccessibilityTraitButton;
  detail_item.accessibilityIdentifier = accessibility_identifier;
  detail_item.iconImage = symbol;
  detail_item.iconTintColor = [UIColor colorNamed:kTextPrimaryColor];
  detail_item.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
  return detail_item;
}

}  // namespace

@implementation AutofillAndPasswordsTableViewController {
  // Presenter for the Level Up Payment Methods walkthrough IPH.
  BubbleViewControllerPresenter* _levelUpPaymentMethodsWalkthroughIPHPresenter;
  // State variables.
  BOOL _passwordsEnabled;
  BOOL _autofillCreditCardEnabled;
  BOOL _autofillProfileEnabled;
  BOOL _identityDocsEnabled;
  BOOL _travelInfoEnabled;
  BOOL _shoppingEnabled;
  BOOL _suggestionsFromGeminiEnabled;
  BOOL _shouldShowSuggestionsFromGemini;
  BOOL _shouldShowAutofillAIFeatures;

  // Updatable Items.
  TableViewDetailIconItem* _passwordsDetailItem;
  TableViewDetailIconItem* _autofillCreditCardDetailItem;
  TableViewDetailIconItem* _autofillProfileDetailItem;
  TableViewDetailIconItem* _identityDocsDetailItem;
  TableViewDetailIconItem* _travelInfoDetailItem;
  TableViewDetailIconItem* _shoppingDetailItem;
  TableViewDetailIconItem* _suggestionsFromGeminiDetailItem;
  BOOL _settingsAreDismissed;
}

- (instancetype)initWithStyle:(UITableViewStyle)style {
  self = [super initWithStyle:style];
  if (self) {
    self.title =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_AUTOFILL_AND_PASSWORDS);
  }
  return self;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [_levelUpPaymentMethodsWalkthroughIPHPresenter dismissAnimated:NO];
    _levelUpPaymentMethodsWalkthroughIPHPresenter = nil;
    [self.delegate autofillAndPasswordsTableViewControllerDidRemove:self];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self maybeShowLevelUpWalkthroughIPH];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier =
      kAutofillAndPasswordsTableViewIdentifier;
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel<TableViewItem*>* model = self.tableViewModel;
  [model addSectionWithIdentifier:kBasicsSection];

  _passwordsDetailItem = [self passwordsItem];
  [model addItem:_passwordsDetailItem toSectionWithIdentifier:kBasicsSection];

  _autofillCreditCardDetailItem = [self autofillCreditCardItem];
  [model addItem:_autofillCreditCardDetailItem
      toSectionWithIdentifier:kBasicsSection];

  _autofillProfileDetailItem = [self autofillProfileItem];
  [model addItem:_autofillProfileDetailItem
      toSectionWithIdentifier:kBasicsSection];

  if (_shouldShowAutofillAIFeatures) {
    _identityDocsDetailItem = [self identityDocsItem];
    [model addItem:_identityDocsDetailItem
        toSectionWithIdentifier:kBasicsSection];

    _travelInfoDetailItem = [self travelInfoItem];
    [model addItem:_travelInfoDetailItem
        toSectionWithIdentifier:kBasicsSection];

    if (autofill::IsAutofillShoppingEnabled()) {
      _shoppingDetailItem = [self shoppingInfoItem];
      [model addItem:_shoppingDetailItem
          toSectionWithIdentifier:kBasicsSection];
    }
  }

  if (_shouldShowSuggestionsFromGemini) {
    _suggestionsFromGeminiDetailItem = [self suggestionsFromGeminiItem];
    [model addItem:_suggestionsFromGeminiDetailItem
        toSectionWithIdentifier:kBasicsSection];
  }

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiWithDataSchema)) {
    [model addItem:[self autofillSettingsItem]
        toSectionWithIdentifier:kBasicsSection];
  }

  [self.delegate autofillAndPasswordsTableViewControllerDidLoadContent:self];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case kPasswordsItem:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectPasswords:self];
      break;
    case kAutofillCreditCardItem:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectAutofillCreditCard:
              self];
      break;
    case kAutofillProfileItem:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectAutofillProfile:self];
      break;
    case kIdentityDocsItem:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectIdentityDocs:self];
      break;
    case kTravelInfoItem:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectTravelInfo:self];
      break;
    case kShoppingInfoItem:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectShopping:self];
      break;
    case kSuggestionsFromGeminiItem:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectSuggestionsFromGemini:
              self];
      break;
    case kAutofillSettingsItem:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectAutofillSettings:
              self];
      break;
    case kSigninPromoItem:
      // The sign-in promo cell handles its own taps through its delegate.
      break;
  }
}

#pragma mark - AutofillAndPasswordsConsumer

- (void)setPasswordsEnabled:(BOOL)enabled {
  if (_passwordsEnabled == enabled) {
    return;
  }
  _passwordsEnabled = enabled;

  if (_passwordsDetailItem) {
    _passwordsDetailItem.trailingDetailText =
        DetailTextForEnabledState(enabled);
    [self reconfigureCellsForItems:@[ _passwordsDetailItem ]];
  }
}

- (void)setAutofillCreditCardEnabled:(BOOL)enabled {
  if (_autofillCreditCardEnabled == enabled) {
    return;
  }
  _autofillCreditCardEnabled = enabled;

  if (_autofillCreditCardDetailItem) {
    _autofillCreditCardDetailItem.trailingDetailText =
        DetailTextForEnabledState(enabled);
    [self reconfigureCellsForItems:@[ _autofillCreditCardDetailItem ]];
  }
}

- (void)setAutofillProfileEnabled:(BOOL)enabled {
  if (_autofillProfileEnabled == enabled) {
    return;
  }
  _autofillProfileEnabled = enabled;

  if (_autofillProfileDetailItem) {
    _autofillProfileDetailItem.trailingDetailText =
        DetailTextForEnabledState(enabled);
    [self reconfigureCellsForItems:@[ _autofillProfileDetailItem ]];
  }
}

- (void)setIdentityDocsEnabled:(BOOL)enabled {
  if (_identityDocsEnabled == enabled) {
    return;
  }
  _identityDocsEnabled = enabled;

  if (_identityDocsDetailItem) {
    _identityDocsDetailItem.trailingDetailText =
        DetailTextForEnabledState(enabled);
    [self reconfigureCellsForItems:@[ _identityDocsDetailItem ]];
  }
}

- (void)setTravelInfoEnabled:(BOOL)enabled {
  if (_travelInfoEnabled == enabled) {
    return;
  }
  _travelInfoEnabled = enabled;

  if (_travelInfoDetailItem) {
    _travelInfoDetailItem.trailingDetailText =
        DetailTextForEnabledState(enabled);
    [self reconfigureCellsForItems:@[ _travelInfoDetailItem ]];
  }
}

- (void)setShoppingEnabled:(BOOL)enabled {
  if (_shoppingEnabled == enabled) {
    return;
  }
  _shoppingEnabled = enabled;

  if (_shoppingDetailItem) {
    _shoppingDetailItem.trailingDetailText = DetailTextForEnabledState(enabled);
    [self reconfigureCellsForItems:@[ _shoppingDetailItem ]];
  }
}

- (void)setSuggestionsFromGeminiEnabled:(BOOL)enabled {
  if (_suggestionsFromGeminiEnabled == enabled) {
    return;
  }
  _suggestionsFromGeminiEnabled = enabled;

  if (_suggestionsFromGeminiDetailItem) {
    _suggestionsFromGeminiDetailItem.trailingDetailText =
        DetailTextForEnabledState(enabled);
    [self reconfigureCellsForItems:@[ _suggestionsFromGeminiDetailItem ]];
  }
}

- (void)setShouldShowSuggestionsFromGemini:(BOOL)shouldShow {
  if (_shouldShowSuggestionsFromGemini == shouldShow) {
    return;
  }
  _shouldShowSuggestionsFromGemini = shouldShow;
  if (self.isViewLoaded) {
    [self reloadData];
  }
}

- (void)setShouldShowAutofillAIFeatures:(BOOL)shouldShow {
  if (_shouldShowAutofillAIFeatures == shouldShow) {
    return;
  }
  _shouldShowAutofillAIFeatures = shouldShow;
  if (self.isViewLoaded) {
    [self reloadData];
  }
}

#pragma mark - AutofillAndPasswordsSigninPromoConsumer

- (void)promoStateChanged:(BOOL)promoEnabled
        promoConfigurator:(SigninPromoViewConfigurator*)promoConfigurator
                promoText:(NSString*)promoText {
  if (!self.tableViewModel) {
    return;
  }
  TableViewModel* model = self.tableViewModel;
  BOOL hasPromo = [model hasSectionForSectionIdentifier:kSignInSection];

  if (promoEnabled == hasPromo) {
    return;
  }

  if (promoEnabled) {
    [model insertSectionWithIdentifier:kSignInSection atIndex:0];

    TableViewSigninPromoItem* promoItem =
        [[TableViewSigninPromoItem alloc] initWithType:kSigninPromoItem];
    promoItem.configurator = promoConfigurator;
    promoItem.text = promoText;
    promoItem.delegate = self.signinPromoDelegate;

    [model addItem:promoItem toSectionWithIdentifier:kSignInSection];
  } else {
    [model removeSectionWithIdentifier:kSignInSection];
  }

  [self.tableView reloadData];
}

- (void)configureSigninPromoWithConfigurator:
    (SigninPromoViewConfigurator*)promoConfigurator {
  TableViewModel* model = self.tableViewModel;
  if (![model hasSectionForSectionIdentifier:kSignInSection]) {
    return;
  }

  NSIndexPath* path = [model indexPathForItemType:kSigninPromoItem
                                sectionIdentifier:kSignInSection];
  if (!path) {
    return;
  }

  TableViewSigninPromoItem* item =
      base::apple::ObjCCast<TableViewSigninPromoItem>(
          [model itemAtIndexPath:path]);

  if (item) {
    item.configurator = promoConfigurator;
    [self reconfigureCellsForItems:@[ item ]];
  }
}

- (void)promoProgressStateDidChange {
  [self.delegate
      autofillAndPasswordsTableViewControllerPromoProgressStateDidChange:self];
}

- (void)signinPromoViewMediatorCloseButtonWasTapped:
    (SigninPromoViewMediator*)mediator {
  [self.delegate
      autofillAndPasswordsTableViewControllerDidTapSigninPromoClose:self];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileAutofillAndPasswordsSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileAutofillAndPasswordsSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  [_levelUpPaymentMethodsWalkthroughIPHPresenter dismissAnimated:NO];
  _levelUpPaymentMethodsWalkthroughIPHPresenter = nil;
  _settingsAreDismissed = YES;
}

#pragma mark - Private

// Returns the passwords item.
- (TableViewDetailIconItem*)passwordsItem {
  return DetailItemWithType(
      kPasswordsItem, l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER),
      l10n_util::GetNSString(
          IDS_AUTOFILL_AND_PASSWORDS_PASSWORD_MANAGER_SUMMARY),
      DetailTextForEnabledState(_passwordsEnabled),
      SettingsRootSymbol(SymbolPassword), kAutofillAndPasswordsPasswordsCellId);
}

// Returns the autofill credit card item.
- (TableViewDetailIconItem*)autofillCreditCardItem {
  return DetailItemWithType(
      kAutofillCreditCardItem,
      l10n_util::GetNSString(IDS_AUTOFILL_PAYMENTS_TITLE),
      l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_PAYMENTS_SUMMARY),
      DetailTextForEnabledState(_autofillCreditCardEnabled),
      SettingsRootSymbol(SymbolCreditCard),
      kAutofillAndPasswordsPaymentMethodsCellId);
}

// Returns the autofill profile item.
- (TableViewDetailIconItem*)autofillProfileItem {
  return DetailItemWithType(
      kAutofillProfileItem,
      l10n_util::GetNSString(IDS_AUTOFILL_CONTACT_INFO_TITLE),
      l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_CONTACT_INFO_SUMMARY),
      DetailTextForEnabledState(_autofillProfileEnabled),
      SettingsRootSymbol(SymbolEnvelope),
      kAutofillAndPasswordsContactInfoCellId);
}

// Returns the identity docs item.
- (TableViewDetailIconItem*)identityDocsItem {
  return DetailItemWithType(
      kIdentityDocsItem,
      l10n_util::GetNSString(IDS_AUTOFILL_IDENTITY_DOCS_TITLE),
      l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_IDENTITY_DOCS_SUMMARY),
      DetailTextForEnabledState(_identityDocsEnabled),
      SettingsRootSymbol(SymbolPersonTextRectangle),
      kAutofillAndPasswordsIdentityDocsCellId);
}

// Returns the travel info item.
- (TableViewDetailIconItem*)travelInfoItem {
  return DetailItemWithType(
      kTravelInfoItem, l10n_util::GetNSString(IDS_AUTOFILL_TRAVEL_TITLE),
      l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_TRAVEL_SUMMARY),
      DetailTextForEnabledState(_travelInfoEnabled),
      SettingsRootSymbol(SymbolSuitcase),
      kAutofillAndPasswordsTravelInfoCellId);
}

// Returns the shopping info item.
- (TableViewDetailIconItem*)shoppingInfoItem {
  return DetailItemWithType(
      kShoppingInfoItem, l10n_util::GetNSString(IDS_AUTOFILL_SHOPPING_TITLE),
      l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_SHOPPING_SUMMARY),
      DetailTextForEnabledState(_shoppingEnabled),
      SettingsRootSymbol(SymbolCart), kAutofillAndPasswordsShoppingInfoCellId);
}

// Returns the Suggestions from Gemini item.
- (TableViewDetailIconItem*)suggestionsFromGeminiItem {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  UIImage* image = SettingsRootSymbol(SymbolGeminiBrandedLogo);
#else
  UIImage* image = SettingsRootSymbol(SymbolGeminiNonBrandedLogo);
#endif
  return DetailItemWithType(
      kSuggestionsFromGeminiItem,
      l10n_util::GetNSString(IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_TITLE),
      l10n_util::GetNSString(
          IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_SUBPAGE_SUMMARY),
      DetailTextForEnabledState(_suggestionsFromGeminiEnabled), image,
      kAutofillAndPasswordsSuggestionsFromGeminiCellId);
}

// Returns the autofill settings item.
- (TableViewDetailIconItem*)autofillSettingsItem {
  return DetailItemWithType(
      kAutofillSettingsItem,
      l10n_util::GetNSString(IDS_IOS_SETTINGS_AUTOFILL_SETTINGS), nil, nil,
      SettingsRootSymbol(SymbolSettings),
      kAutofillAndPasswordsAutofillSettingsCellId);
}

// Presents the Level Up Payment Methods walkthrough IPH if needed.
- (void)maybeShowLevelUpWalkthroughIPH {
  if (!self.shouldShowLevelUpPaymentMethodsWalkthroughIPH ||
      _settingsAreDismissed) {
    return;
  }

  UIView* targetView = self.view;
  CHECK(targetView.window);

  NSIndexPath* targetIndexPath =
      [self.tableViewModel indexPathForItemType:kAutofillCreditCardItem
                              sectionIdentifier:kBasicsSection];

  if (!targetIndexPath) {
    return;
  }

  CGPoint anchorPoint = CGPointZero;
  BubbleArrowDirection arrowDirection = BubbleArrowDirectionDown;

  UITableViewCell* cell =
      [self.tableView cellForRowAtIndexPath:targetIndexPath];
  if (cell.window) {
    CGPoint anchorPointInCell =
        CGPointMake(CGRectGetMidX(cell.bounds), CGRectGetMaxY(cell.bounds));
    anchorPoint = [cell convertPoint:anchorPointInCell toView:cell.window];
    arrowDirection = BubbleArrowDirectionUp;
  } else {
    anchorPoint = CGPointMake(0.5 * CGRectGetWidth(targetView.bounds),
                              0.5 * CGRectGetHeight(targetView.bounds));
  }

  NSString* text =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_WALKTHROUGH_OPEN_PAYMENT_METHODS);

  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType reason) {
        [weakSelf levelUpWalkthroughStep4DidDismissWithReason:reason];
      };

  BubbleViewControllerPresenter* presenter =
      [[BubbleViewControllerPresenter alloc]
                   initWithText:text
                          title:nil
                 arrowDirection:arrowDirection
                      alignment:BubbleAlignmentBottomOrTrailing
                     bubbleType:BubbleViewTypeRichWithNext
                pageControlPage:BubblePageControlPageFourth
          totalPageControlPages:5
          customNextButtonTitle:l10n_util::GetNSString(IDS_IOS_IPH_BUBBLE_NEXT)
              dismissalCallback:dismissalCallback];
  presenter.dismissalTimerDisabled = YES;

  if ([presenter canPresentInView:targetView anchorPoint:anchorPoint]) {
    self.shouldShowLevelUpPaymentMethodsWalkthroughIPH = NO;
    _levelUpPaymentMethodsWalkthroughIPHPresenter = presenter;
    [presenter presentInViewController:self anchorPoint:anchorPoint];
  }
}

// Handles dismissal of the Level Up Payment Methods walkthrough IPH.
- (void)levelUpWalkthroughStep4DidDismissWithReason:
    (IPHDismissalReasonType)reason {
  _levelUpPaymentMethodsWalkthroughIPHPresenter = nil;
  switch (reason) {
    case IPHDismissalReasonType::kTappedNext:
    case IPHDismissalReasonType::kTappedAnchorView:
    case IPHDismissalReasonType::kTappedIPH:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectAutofillCreditCard:
              self];
      break;
    default:
      break;
  }
}

@end
