// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_items_display_view_controller.h"

#include "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/payments/cells/price_item.h"
#import "ios/chrome/browser/ui/payments/payment_items_display_view_controller_actions.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kPaymentItemsDisplayCollectionViewID =
    @"kPaymentItemsDisplayCollectionViewID";
NSString* const kPaymentItemsDisplayItemID = @"kPaymentItemsDisplayItemID";

namespace {

const CGFloat kButtonEdgeInset = 9;
const CGFloat kSeparatorEdgeInset = 14;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPayment = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypePaymentItemTotal = kItemTypeEnumZero,
  ItemTypePaymentItem,  // This is a repeated item type.
};

}  // namespace

@interface PaymentItemsDisplayViewController ()<
    PaymentItemsDisplayViewControllerActions> {
  MDCButton* _payButton;
}

@end

@implementation PaymentItemsDisplayViewController

@synthesize delegate = _delegate;
@synthesize dataSource = _dataSource;

- (instancetype)init {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  if ((self = [super initWithLayout:layout
                              style:CollectionViewControllerStyleAppBar])) {
    [self setTitle:l10n_util::GetNSString(IDS_PAYMENTS_ORDER_SUMMARY_LABEL)];

    // Set up leading (return) button.
    UIBarButtonItem* returnButton =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:self
                                            action:@selector(onReturn)];
    [returnButton
        setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_BACK)];
    [self navigationItem].leftBarButtonItem = returnButton;

    // Set up trailing (pay) button.
    _payButton = [[MDCButton alloc] init];
    [_payButton setTitle:l10n_util::GetNSString(IDS_PAYMENTS_PAY_BUTTON)
                forState:UIControlStateNormal];
    [_payButton setBackgroundColor:[UIColor colorNamed:kBlueColor]];
    [_payButton setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                     forState:UIControlStateNormal];
    [_payButton setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                     forState:UIControlStateDisabled];
    [_payButton setInkColor:[UIColor colorNamed:kMDCInkColor]];
    [_payButton addTarget:self
                   action:@selector(onConfirm)
         forControlEvents:UIControlEventTouchUpInside];
    [_payButton sizeToFit];
    [_payButton setTranslatesAutoresizingMaskIntoConstraints:NO];

    // The navigation bar will set the rightBarButtonItem's height to the full
    // height of the bar. We don't want that for the button so we use a UIView
    // here to contain the button where the button is vertically centered inside
    // the full bar height. Also navigation bar button items are aligned with
    // the trailing edge of the screen. Make the enclosing view larger and align
    // the pay button with the leading edge of the enclosing view leaving an
    // inset on the trailing edge.
    UIView* buttonView = [[UIView alloc]
        initWithFrame:CGRectMake(0, 0,
                                 _payButton.frame.size.width + kButtonEdgeInset,
                                 _payButton.frame.size.height)];
    [buttonView addSubview:_payButton];
    [NSLayoutConstraint activateConstraints:@[
      [_payButton.leadingAnchor
          constraintEqualToAnchor:buttonView.leadingAnchor],
      [_payButton.centerYAnchor
          constraintEqualToAnchor:buttonView.centerYAnchor],
      [_payButton.trailingAnchor
          constraintEqualToAnchor:buttonView.trailingAnchor
                         constant:-kButtonEdgeInset],
    ]];

    UIBarButtonItem* payButtonItem =
        [[UIBarButtonItem alloc] initWithCustomView:buttonView];
    [self navigationItem].rightBarButtonItem = payButtonItem;
  }
  return self;
}

- (void)onReturn {
  [_delegate paymentItemsDisplayViewControllerDidReturn:self];
}

- (void)onConfirm {
  [_delegate paymentItemsDisplayViewControllerDidConfirm:self];
}

#pragma mark - Setters

- (void)setDataSource:
    (id<PaymentItemsDisplayViewControllerDataSource>)dataSource {
  _dataSource = dataSource;
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [_payButton setEnabled:[_dataSource canPay]];

  // Add the total entry.
  [model addSectionWithIdentifier:SectionIdentifierPayment];
  CollectionViewItem* totalItem = [_dataSource totalItem];
  if (totalItem) {
    totalItem.type = ItemTypePaymentItemTotal;
    totalItem.accessibilityIdentifier = kPaymentItemsDisplayItemID;
    [model addItem:totalItem toSectionWithIdentifier:SectionIdentifierPayment];
  }

  // Add the line item entries.
  [[_dataSource lineItems]
      enumerateObjectsUsingBlock:^(CollectionViewItem* item, NSUInteger index,
                                   BOOL* stop) {
        item.type = ItemTypePaymentItem;
        item.accessibilityIdentifier = kPaymentItemsDisplayItemID;
        [model addItem:item toSectionWithIdentifier:SectionIdentifierPayment];
      }];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier =
      kPaymentItemsDisplayCollectionViewID;

  // Customize collection view settings.
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.separatorInset =
      UIEdgeInsetsMake(0, kSeparatorEdgeInset, 0, kSeparatorEdgeInset);
  self.styler.separatorColor = [UIColor colorNamed:kSeparatorColor];
}

#pragma mark UICollectionViewDataSource
- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(nonnull NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypePaymentItemTotal: {
      if ([cell isKindOfClass:[PriceCell class]]) {
        PriceCell* priceCell = base::mac::ObjCCastStrict<PriceCell>(cell);
        SetUILabelScaledFont(priceCell.priceLabel, [MDCTypography body2Font]);
      }
      break;
    }
    case ItemTypePaymentItem: {
      if ([cell isKindOfClass:[PriceCell class]]) {
        PriceCell* priceCell = base::mac::ObjCCastStrict<PriceCell>(cell);
        SetUILabelScaledFont(priceCell.itemLabel, [MDCTypography body1Font]);
      }
      break;
    }
    default:
      break;
  }
  return cell;
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];

  UIEdgeInsets inset = [self collectionView:collectionView
                                     layout:collectionView.collectionViewLayout
                     insetForSectionAtIndex:indexPath.section];

  return [MDCCollectionViewCell
      cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds) -
                                 inset.left - inset.right
                         forItem:item];
}

// There are no effects from touching the payment items so there should not be
// an ink ripple.
- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  return YES;
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self onReturn];
  return YES;
}

@end
