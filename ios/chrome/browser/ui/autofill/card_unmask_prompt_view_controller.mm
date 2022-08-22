// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/ui/autofill/cells/cvc_header_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kCardUnmaskPromptTableViewAccessibilityID =
    @"CardUnmaskPromptTableViewAccessibilityID";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierHeader = kSectionIdentifierEnumZero,
  SectionIdentifierInputs,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeCVCInput,
  ItemTypeFooter,
};

// Empty space on top of the input section. This value added up to the gPay
// badge bottom padding achieves the mock's vertical spacing between the gPay
// badge in the header.
const CGFloat kInputsSectionTopSpacing = 18;
// When the inputs section doesn't display a footer, an empty one is displayed
// with this height to provide spacing to the bottom of the tableView.
const CGFloat kEmptyFooterHeight = 10;
// Estimated height of the header/footer, used to speed the constraints.
const CGFloat kEstimatedHeaderFooterHeight = 50;
// Dummy URL used as target of the link in the footer.
const char kFooterDummyLinkTarget[] = "about:blank";

}  // namespace

@interface CardUnmaskPromptViewController () <
    TableViewTextEditItemDelegate,
    TableViewLinkHeaderFooterItemDelegate> {
  // Button displayed on the right side of the navigation bar.
  // Tapping it sends the data in the prompt for verification.
  UIBarButtonItem* _confirmButton;
  // Owns `self`.
  autofill::CardUnmaskPromptViewBridge* _bridge;  // weak
  // Model of the cvc input cell.
  TableViewTextEditItem* _CVCInputItem;
  // Model of the footer.
  TableViewLinkHeaderFooterItem* _footerItem;
}

@end

@implementation CardUnmaskPromptViewController

- (instancetype)initWithBridge:(autofill::CardUnmaskPromptViewBridge*)bridge {
  self = [super initWithStyle:ChromeTableViewStyle()];

  if (self) {
    _bridge = bridge;
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier =
      kCardUnmaskPromptTableViewAccessibilityID;

  self.title =
      base::SysUTF16ToNSString(_bridge->GetController()->GetWindowTitle());

  // Disable selection.
  self.tableView.allowsSelection = NO;

  self.tableView.estimatedSectionFooterHeight = kEstimatedHeaderFooterHeight;
  self.tableView.estimatedSectionHeaderHeight = kEstimatedHeaderFooterHeight;

  self.navigationItem.leftBarButtonItem = [self createCancelButton];

  _confirmButton = [self createConfirmButton];
  self.navigationItem.rightBarButtonItem = _confirmButton;

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierHeader];

  [model setHeader:[self createHeaderItem]
      forSectionWithIdentifier:SectionIdentifierHeader];

  [model addSectionWithIdentifier:SectionIdentifierInputs];

  _CVCInputItem = [self createCVCInputItem];
  [self.tableViewModel addItem:_CVCInputItem
       toSectionWithIdentifier:SectionIdentifierInputs];

  _footerItem = [self createFooterItem];
  [self.tableViewModel setFooter:_footerItem
        forSectionWithIdentifier:SectionIdentifierInputs];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // Notify bridge that UI was dismissed.
  _bridge->NavigationControllerDismissed();
  _bridge = nullptr;
}

#pragma mark - Actions

- (void)onCancelTapped {
  _bridge->PerformClose();
}

- (void)onVerifyTapped {
  NOTIMPLEMENTED();
}

#pragma mark - Private

// Returns a newly created item for the header of the section.
- (CVCHeaderItem*)createHeaderItem {
  autofill::CardUnmaskPromptController* controller = _bridge->GetController();
  NSString* instructions =
      base::SysUTF16ToNSString(controller->GetInstructionsMessage());

  CVCHeaderItem* header = [[CVCHeaderItem alloc] initWithType:ItemTypeHeader];
  header.instructionsText = instructions;

  return header;
}

// Returns a new cancel button for the navigation bar.
- (UIBarButtonItem*)createCancelButton {
  UIBarButtonItem* cancelButton =
      [[UIBarButtonItem alloc] initWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(onCancelTapped)];

  return cancelButton;
}

// Returns a new confirm button for the navigation bar.
- (UIBarButtonItem*)createConfirmButton {
  NSString* confirmButtonText =
      base::SysUTF16ToNSString(_bridge->GetController()->GetOkButtonLabel());
  UIBarButtonItem* confirmButton =
      [[UIBarButtonItem alloc] initWithTitle:confirmButtonText
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(onVerifyTapped)];
  [confirmButton setTitleTextAttributes:@{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]
  }
                               forState:UIControlStateNormal];
  [confirmButton setTitleTextAttributes:@{
    NSForegroundColorAttributeName : [UIColor colorNamed:kDisabledTintColor]
  }
                               forState:UIControlStateDisabled];

  return confirmButton;
}

// Returns the model for the cvc input cell.
- (TableViewTextEditItem*)createCVCInputItem {
  autofill::CardUnmaskPromptController* controller = _bridge->GetController();

  TableViewTextEditItem* cvcInputItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeCVCInput];
  cvcInputItem.delegate = self;
  cvcInputItem.textFieldName =
      l10n_util::GetNSString(IDS_AUTOFILL_CARD_UNMASK_PROMPT_CVC_FIELD_TITLE);
  cvcInputItem.keyboardType = UIKeyboardTypeNumberPad;
  cvcInputItem.hideIcon = YES;
  cvcInputItem.textFieldEnabled = YES;
  cvcInputItem.identifyingIcon = NativeImage(controller->GetCvcImageRid());

  return cvcInputItem;
}

// Returns a newly created item for the footer of the section.
- (TableViewLinkHeaderFooterItem*)createFooterItem {
  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text = l10n_util::GetNSString(
      IDS_AUTOFILL_CARD_UNMASK_PROMPT_UPDATE_CARD_MESSAGE_LINK);
  // Using a dummy target for the link in the footer.
  // The link target is ignored and taps on it are handled by `didTapLinkURL`.
  footer.urls = @[ [[CrURL alloc] initWithGURL:GURL(kFooterDummyLinkTarget)] ];
  return footer;
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  NOTIMPLEMENTED();
}

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  NOTIMPLEMENTED();
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  NOTIMPLEMENTED();
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  // Adding space on top of the inputs section to match the mocks' spacing.
  NSInteger inputsSection =
      [self.tableViewModel sectionForSectionIdentifier:SectionIdentifierInputs];
  if (section == inputsSection) {
    return kInputsSectionTopSpacing;
  }
  return UITableViewAutomaticDimension;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  // The header section doesn't need a footer, settings its height to zero to
  // avoid extra spacing between sections.
  NSInteger headerSection =
      [self.tableViewModel sectionForSectionIdentifier:SectionIdentifierHeader];
  if (section == headerSection) {
    return 0;
  }
  // Let Autolayout calculate calculate the footer's height if any.
  if ([self.tableViewModel footerForSectionIndex:section]) {
    return UITableViewAutomaticDimension;
  }
  // Default spacing when no footer.
  return kEmptyFooterHeight;
}

#pragma mark - UITableViewDataSource

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  // Set `self` as delegate for the inputs section footer to handle taps on the
  // Update Card link.
  if (sectionIdentifier == SectionIdentifierInputs) {
    TableViewLinkHeaderFooterView* footerView =
        base::mac::ObjCCast<TableViewLinkHeaderFooterView>(view);
    footerView.delegate = self;
  }

  return view;
}

#pragma mark - TableViewLinkHeaderFooterDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  // Handle taps on the Update Card link.
  NOTIMPLEMENTED();
}

@end
