// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_selector_view_controller.h"

#include "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/payments/cells/payments_is_selectable.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_selector_view_controller_actions.h"
#import "ios/chrome/browser/ui/payments/payment_request_selector_view_controller_data_source.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kPaymentRequestSelectorCollectionViewAccessibilityID =
    @"kPaymentRequestSelectorCollectionViewAccessibilityID";

const CGFloat kSeparatorEdgeInset = 14;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierItems = kSectionIdentifierEnumZero,
  SectionIdentifierAddButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeSelectableItem,  // This is a repeated item type.
  ItemTypeSpinner,
  ItemTypeAddItem,
};

}  // namespace

@interface PaymentRequestSelectorViewController ()<
    PaymentRequestSelectorViewControllerActions>

@end

@implementation PaymentRequestSelectorViewController

@synthesize delegate = _delegate;
@synthesize dataSource = _dataSource;
@synthesize editing = _editing;

- (instancetype)init {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  if ((self = [super initWithLayout:layout
                              style:CollectionViewControllerStyleAppBar])) {
    _editing = NO;

    // Set up leading (back) button.
    UIBarButtonItem* backButton =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:self
                                            action:@selector(onBack)];
    backButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
    self.navigationItem.leftBarButtonItem = backButton;
  }
  return self;
}

// Returns a custom edit bar button item.
- (UIBarButtonItem*)editButton {
  UIBarButtonItem* button = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(editButtonPressed)];
  return button;
}

// Returns a custom done bar button item.
- (UIBarButtonItem*)doneButton {
  return [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(editButtonPressed)];
}

- (void)editButtonPressed {
  // Toggle the editing mode and notify the delegate.
  self.editing = !self.editing;
  if ([self.delegate
          respondsToSelector:@selector
          (paymentRequestSelectorViewControllerDidToggleEditingMode:)]) {
    [self.delegate
        paymentRequestSelectorViewControllerDidToggleEditingMode:self];
  }
}

#pragma mark - PaymentRequestSelectorViewControllerActions

- (void)onBack {
  [self.delegate paymentRequestSelectorViewControllerDidFinish:self];
}

#pragma mark - CollectionViewController methods

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  self.title = [_dataSource title];

  // Set up trailing (edit or done) button.
  if (self.dataSource.state == PaymentRequestSelectorStateNormal &&
      [self.dataSource allowsEditMode]) {
    self.navigationItem.rightBarButtonItem =
        self.editing ? [self doneButton] : [self editButton];
  } else {
    self.navigationItem.rightBarButtonItem = nil;
  }

  [model addSectionWithIdentifier:SectionIdentifierItems];

  // If the view controller is in the pending state, only display a spinner and
  // a message indicating the pending state.
  if (self.dataSource.state == PaymentRequestSelectorStatePending) {
    StatusItem* statusItem = [[StatusItem alloc] initWithType:ItemTypeSpinner];
    statusItem.state = StatusItemState::VERIFYING;
    statusItem.text = l10n_util::GetNSString(IDS_PAYMENTS_CHECKING_OPTION);
    [model addItem:statusItem toSectionWithIdentifier:SectionIdentifierItems];
    return;
  }

  if (!self.editing) {
    CollectionViewItem* headerItem = [self.dataSource headerItem];
    if (headerItem) {
      headerItem.type = ItemTypeHeader;
      [model addItem:headerItem toSectionWithIdentifier:SectionIdentifierItems];
    }
  }

  [[self.dataSource selectableItems]
      enumerateObjectsUsingBlock:^(
          CollectionViewItem<PaymentsIsSelectable>* item, NSUInteger index,
          BOOL* stop) {
        DCHECK([item respondsToSelector:@selector(accessoryType)]);
        item.type = ItemTypeSelectableItem;
        item.accessibilityTraits |= UIAccessibilityTraitButton;
        if (self.editing) {
          item.accessoryType =
              MDCCollectionViewCellAccessoryDisclosureIndicator;
        } else {
          item.accessoryType = (index == self.dataSource.selectedItemIndex)
                                   ? MDCCollectionViewCellAccessoryCheckmark
                                   : MDCCollectionViewCellAccessoryNone;
        }
        [model addItem:item toSectionWithIdentifier:SectionIdentifierItems];
      }];

  if (!self.editing) {
    CollectionViewItem* addButtonItem = [self.dataSource addButtonItem];
    if (addButtonItem) {
      [model addSectionWithIdentifier:SectionIdentifierAddButton];
      addButtonItem.type = ItemTypeAddItem;
      addButtonItem.accessibilityTraits |= UIAccessibilityTraitButton;
      [model addItem:addButtonItem
          toSectionWithIdentifier:SectionIdentifierAddButton];
    }
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.accessibilityIdentifier =
      kPaymentRequestSelectorCollectionViewAccessibilityID;

  // Customize collection view settings.
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.separatorInset =
      UIEdgeInsetsMake(0, kSeparatorEdgeInset, 0, kSeparatorEdgeInset);
  self.styler.separatorColor = [UIColor colorNamed:kSeparatorColor];
}

#pragma mark UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(nonnull NSIndexPath*)indexPath {
  CollectionViewModel* model = self.collectionViewModel;

  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeHeader: {
      if ([cell isKindOfClass:[PaymentsTextCell class]]) {
        PaymentsTextCell* textCell =
            base::mac::ObjCCastStrict<PaymentsTextCell>(cell);
        textCell.textLabel.textColor =
            self.dataSource.state == PaymentRequestSelectorStateError
                ? [UIColor colorNamed:kRedColor]
                : [UIColor colorNamed:kTextSecondaryColor];
      }
      break;
    }
    case ItemTypeAddItem: {
      if ([cell isKindOfClass:[PaymentsTextCell class]]) {
        PaymentsTextCell* paymentsTextCell =
            base::mac::ObjCCastStrict<PaymentsTextCell>(cell);
        // Style call to action cells.
        if (paymentsTextCell.cellType == PaymentsTextCellTypeCallToAction) {
          paymentsTextCell.textLabel.textColor =
              [UIColor colorNamed:kBlueColor];
        }
      }
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  CollectionViewModel* model = self.collectionViewModel;

  CollectionViewItem* item = [model itemAtIndexPath:indexPath];

  if (self.editing) {
    DCHECK(item.type == ItemTypeSelectableItem);
    // Notify the delegate of the selection.
    NSUInteger index =
        [self.collectionViewModel indexInItemTypeForIndexPath:indexPath];
    DCHECK_LT(index, [[self.dataSource selectableItems] count]);
    if ([self.delegate
            respondsToSelector:@selector
            (paymentRequestSelectorViewController:didSelectItemAtIndexForEditing
                                                    :)]) {
      [self.delegate paymentRequestSelectorViewController:self
                           didSelectItemAtIndexForEditing:index];
    }
    return;
  }

  switch (item.type) {
    case ItemTypeSelectableItem: {
      NSUInteger currentlySelectedItemIndex = self.dataSource.selectedItemIndex;
      // Notify the delegate of the selection. Update the currently selected and
      // the newly selected cells only if the selection can actually be made.
      NSUInteger index =
          [self.collectionViewModel indexInItemTypeForIndexPath:indexPath];
      DCHECK_LT(index, [[self.dataSource selectableItems] count]);
      if ([self.delegate paymentRequestSelectorViewController:self
                                         didSelectItemAtIndex:index]) {
        // Update the currently selected cell, if any.
        if (currentlySelectedItemIndex != NSUIntegerMax) {
          DCHECK(currentlySelectedItemIndex <
                 [[self.dataSource selectableItems] count]);
          CollectionViewItem<PaymentsIsSelectable>* oldSelectedItem =
              [[self.dataSource selectableItems]
                  objectAtIndex:currentlySelectedItemIndex];
          oldSelectedItem.accessoryType = MDCCollectionViewCellAccessoryNone;
          [self reconfigureCellsForItems:@[ oldSelectedItem ]];
        }
        // Update the newly selected cell.
        CollectionViewItem<PaymentsIsSelectable>* newSelectedItem =
            reinterpret_cast<CollectionViewItem<PaymentsIsSelectable>*>(item);
        newSelectedItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
        [self reconfigureCellsForItems:@[ newSelectedItem ]];
      }
      break;
    }
    case ItemTypeAddItem: {
      if ([self.delegate
              respondsToSelector:@selector
              (paymentRequestSelectorViewControllerDidSelectAddItem:)]) {
        [self.delegate
            paymentRequestSelectorViewControllerDidSelectAddItem:self];
      }
      break;
    }
    default:
      break;
  }
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

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  if (type == ItemTypeHeader) {
    return YES;
  } else {
    return NO;
  }
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self onBack];
  return YES;
}

@end
