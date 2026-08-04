// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/utils/autofill_and_passwords_item_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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
  BOOL _shouldShowAutofillAIFeatures;

  // Updatable Items.
  TableViewDetailIconItem* _passwordsDetailItem;
  TableViewDetailIconItem* _autofillCreditCardDetailItem;
  TableViewDetailIconItem* _autofillProfileDetailItem;
  TableViewDetailIconItem* _identityDocsDetailItem;
  TableViewDetailIconItem* _travelInfoDetailItem;
  TableViewDetailIconItem* _shoppingDetailItem;
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
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel<TableViewItem*>* model = self.tableViewModel;
  [model addSectionWithIdentifier:SettingsSectionIdentifierBasics];

  _passwordsDetailItem = PasswordsItem(_passwordsEnabled);
  [model addItem:_passwordsDetailItem
      toSectionWithIdentifier:SettingsSectionIdentifierBasics];

  _autofillCreditCardDetailItem =
      AutofillCreditCardItem(_autofillCreditCardEnabled);
  [model addItem:_autofillCreditCardDetailItem
      toSectionWithIdentifier:SettingsSectionIdentifierBasics];

  _autofillProfileDetailItem = AutofillProfileItem(_autofillProfileEnabled);
  [model addItem:_autofillProfileDetailItem
      toSectionWithIdentifier:SettingsSectionIdentifierBasics];

  if (_shouldShowAutofillAIFeatures) {
    _identityDocsDetailItem = IdentityDocsItem(_identityDocsEnabled);
    [model addItem:_identityDocsDetailItem
        toSectionWithIdentifier:SettingsSectionIdentifierBasics];

    _travelInfoDetailItem = TravelInfoItem(_travelInfoEnabled);
    [model addItem:_travelInfoDetailItem
        toSectionWithIdentifier:SettingsSectionIdentifierBasics];

    if (autofill::IsAutofillShoppingEnabled()) {
      _shoppingDetailItem = ShoppingInfoItem(_shoppingEnabled);
      [model addItem:_shoppingDetailItem
          toSectionWithIdentifier:SettingsSectionIdentifierBasics];
    }
  }

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiWithDataSchema)) {
    [model addItem:AutofillSettingsItem()
        toSectionWithIdentifier:SettingsSectionIdentifierBasics];
  }

  [self.delegate autofillAndPasswordsTableViewControllerDidLoadContent:self];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case SettingsItemTypePasswords:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectPasswords:self];
      break;
    case SettingsItemTypeAutofillCreditCard:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectAutofillCreditCard:
              self];
      break;
    case SettingsItemTypeAutofillProfile:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectAutofillProfile:self];
      break;
    case SettingsItemTypeIdentityDocs:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectIdentityDocs:self];
      break;
    case SettingsItemTypeTravelInfo:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectTravelInfo:self];
      break;
    case SettingsItemTypeShoppingInfo:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectShopping:self];
      break;
    case SettingsItemTypeAutofillSettings:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectAutofillSettings:
              self];
      break;
    default:
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
    if (IsYourSavedInfoSettingsPageIosEnabled()) {
      _passwordsDetailItem.trailingDetailText =
          PasswordsItemDetailText(enabled);
    } else {
      _passwordsDetailItem.detailText = PasswordsItemDetailText(enabled);
    }
    [self reconfigureCellsForItems:@[ _passwordsDetailItem ]];
  }
}

- (void)setAutofillCreditCardEnabled:(BOOL)enabled {
  if (_autofillCreditCardEnabled == enabled) {
    return;
  }
  _autofillCreditCardEnabled = enabled;

  if (_autofillCreditCardDetailItem) {
    if (IsYourSavedInfoSettingsPageIosEnabled()) {
      _autofillCreditCardDetailItem.trailingDetailText =
          AutofillCreditCardItemDetailText(enabled);
    } else {
      _autofillCreditCardDetailItem.detailText =
          AutofillCreditCardItemDetailText(enabled);
    }
    [self reconfigureCellsForItems:@[ _autofillCreditCardDetailItem ]];
  }
}

- (void)setAutofillProfileEnabled:(BOOL)enabled {
  if (_autofillProfileEnabled == enabled) {
    return;
  }
  _autofillProfileEnabled = enabled;

  if (_autofillProfileDetailItem) {
    if (IsYourSavedInfoSettingsPageIosEnabled()) {
      _autofillProfileDetailItem.trailingDetailText =
          AutofillProfileItemDetailText(enabled);
    } else {
      _autofillProfileDetailItem.detailText =
          AutofillProfileItemDetailText(enabled);
    }
    [self reconfigureCellsForItems:@[ _autofillProfileDetailItem ]];
  }
}

- (void)setIdentityDocsEnabled:(BOOL)enabled {
  if (_identityDocsEnabled == enabled) {
    return;
  }
  _identityDocsEnabled = enabled;

  if (_identityDocsDetailItem) {
    if (IsYourSavedInfoSettingsPageIosEnabled()) {
      _identityDocsDetailItem.trailingDetailText =
          IdentityDocsItemDetailText(enabled);
    } else {
      _identityDocsDetailItem.detailText = IdentityDocsItemDetailText(enabled);
    }
    [self reconfigureCellsForItems:@[ _identityDocsDetailItem ]];
  }
}

- (void)setTravelInfoEnabled:(BOOL)enabled {
  if (_travelInfoEnabled == enabled) {
    return;
  }
  _travelInfoEnabled = enabled;

  if (_travelInfoDetailItem) {
    if (IsYourSavedInfoSettingsPageIosEnabled()) {
      _travelInfoDetailItem.trailingDetailText =
          TravelInfoItemDetailText(enabled);
    } else {
      _travelInfoDetailItem.detailText = TravelInfoItemDetailText(enabled);
    }
    [self reconfigureCellsForItems:@[ _travelInfoDetailItem ]];
  }
}

- (void)setShoppingEnabled:(BOOL)enabled {
  if (_shoppingEnabled == enabled) {
    return;
  }
  _shoppingEnabled = enabled;

  if (_shoppingDetailItem) {
    if (IsYourSavedInfoSettingsPageIosEnabled()) {
      _shoppingDetailItem.trailingDetailText =
          ShoppingInfoItemDetailText(enabled);
    } else {
      _shoppingDetailItem.detailText = ShoppingInfoItemDetailText(enabled);
    }
    [self reconfigureCellsForItems:@[ _shoppingDetailItem ]];
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
  BOOL hasPromo =
      [model hasSectionForSectionIdentifier:SettingsSectionIdentifierSignIn];

  if (promoEnabled == hasPromo) {
    return;
  }

  if (promoEnabled) {
    [model insertSectionWithIdentifier:SettingsSectionIdentifierSignIn
                               atIndex:0];

    TableViewSigninPromoItem* promoItem = [[TableViewSigninPromoItem alloc]
        initWithType:SettingsItemTypeSigninPromo];
    promoItem.configurator = promoConfigurator;
    promoItem.text = promoText;
    promoItem.delegate = self.signinPromoDelegate;

    [model addItem:promoItem
        toSectionWithIdentifier:SettingsSectionIdentifierSignIn];
  } else {
    [model removeSectionWithIdentifier:SettingsSectionIdentifierSignIn];
  }

  [self.tableView reloadData];
}

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)promoConfigurator
                             identityChanged:(BOOL)identityChanged {
  TableViewModel* model = self.tableViewModel;
  if (![model hasSectionForSectionIdentifier:SettingsSectionIdentifierSignIn]) {
    return;
  }

  NSIndexPath* path =
      [model indexPathForItemType:SettingsItemTypeSigninPromo
                sectionIdentifier:SettingsSectionIdentifierSignIn];
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

// Presents the Level Up Payment Methods walkthrough IPH if needed.
- (void)maybeShowLevelUpWalkthroughIPH {
  if (!self.shouldShowLevelUpPaymentMethodsWalkthroughIPH ||
      _settingsAreDismissed) {
    return;
  }

  UIView* targetView = self.view;
  CHECK(targetView.window);

  NSIndexPath* targetIndexPath = [self.tableViewModel
      indexPathForItemType:SettingsItemTypeAutofillCreditCard
         sectionIdentifier:SettingsSectionIdentifierBasics];

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
          totalPageControlPages:4
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
