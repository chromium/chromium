// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_error_view_controller.h"

#include "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_error_view_controller_actions.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kPaymentRequestErrorCollectionViewID =
    @"kPaymentRequestErrorCollectionViewID";

namespace {

const CGFloat kSeparatorEdgeInset = 14;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMessage = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeMessage = kItemTypeEnumZero,
};

}  // namespace

@interface PaymentRequestErrorViewController ()<
    PaymentRequestErrorViewControllerActions> {
  UIBarButtonItem* _okButton;
}

@end

@implementation PaymentRequestErrorViewController

@synthesize errorMessage = _errorMessage;
@synthesize delegate = _delegate;

- (instancetype)init {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  if ((self = [super initWithLayout:layout
                              style:CollectionViewControllerStyleAppBar])) {
    [self setTitle:l10n_util::GetNSString(IDS_PAYMENTS_TITLE)];

    // Set up trailing (ok) button.
    _okButton =
        [[UIBarButtonItem alloc] initWithTitle:l10n_util::GetNSString(IDS_OK)
                                         style:UIBarButtonItemStylePlain
                                        target:self
                                        action:@selector(onOk)];
    [_okButton setTitleTextAttributes:@{
      NSForegroundColorAttributeName : [UIColor colorNamed:kDisabledTintColor]
    }
                             forState:UIControlStateDisabled];
    [_okButton setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_OK)];
    [self navigationItem].rightBarButtonItem = _okButton;
  }
  return self;
}

- (void)onOk {
  [_delegate paymentRequestErrorViewControllerDidDismiss:self];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  // Message section.
  [model addSectionWithIdentifier:SectionIdentifierMessage];

  PaymentsTextItem* messageItem =
      [[PaymentsTextItem alloc] initWithType:ItemTypeMessage];
  messageItem.text = _errorMessage;
  [model addItem:messageItem toSectionWithIdentifier:SectionIdentifierMessage];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier =
      kPaymentRequestErrorCollectionViewID;

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
    case ItemTypeMessage: {
      PaymentsTextCell* messageCell =
          base::mac::ObjCCastStrict<PaymentsTextCell>(cell);
      messageCell.textLabel.textColor =
          [UIColor colorNamed:kTextSecondaryColor];
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

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self onOk];
  return YES;
}

@end
