// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierRecipients = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeRecipient = kItemTypeEnumZero,
  ItemTypeHeader,
};

// Size of the accessory view symbol.
const CGFloat kAccessorySymbolSize = 22;

}  // namespace

@interface FamilyPickerViewController () <PopoverLabelViewControllerDelegate>

@property(nonatomic, strong) NSArray<RecipientInfoForIOSDisplay*>* recipients;

@end

@implementation FamilyPickerViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.navigationItem.title =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_TITLE);
  UIBarButtonItem* shareButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_PASSWORD_SHARING_SHARE_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(shareButtonTapped)];
  shareButton.enabled = NO;
  self.navigationItem.rightBarButtonItem = shareButton;
  self.navigationItem.rightBarButtonItem.accessibilityIdentifier =
      kFamilyPickerShareButtonId;

  self.tableView.allowsMultipleSelection = YES;
  self.tableView.accessibilityIdentifier = kFamilyPickerTableViewId;

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierRecipients];
  [model setHeader:[self headerItem]
      forSectionWithIdentifier:SectionIdentifierRecipients];
  for (RecipientInfoForIOSDisplay* recipient in _recipients) {
    [model addItem:[self recipientItem:recipient]
        toSectionWithIdentifier:SectionIdentifierRecipients];
  }
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.delegate familyPickerWasDismissed:self];
  [super viewDidDisappear:animated];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_recipients[indexPath.row].isEligible) {
    [tableView cellForRowAtIndexPath:indexPath].accessoryView =
        [[UIImageView alloc] initWithImage:[self checkmarkCircleIcon]];
    [self setShareButtonStatus];
  }
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_recipients[indexPath.row].isEligible) {
    [tableView cellForRowAtIndexPath:indexPath].accessoryView =
        [[UIImageView alloc] initWithImage:[self circleIcon]];
    [self setShareButtonStatus];
  }
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  return _recipients[indexPath.row].isEligible;
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _recipients.count;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)theTableView {
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.userInteractionEnabled = YES;
  cell.textLabel.numberOfLines = 1;
  cell.detailTextLabel.numberOfLines = 1;
  cell.accessibilityIdentifier = _recipients[indexPath.row].email;
  if (_recipients[indexPath.row].isEligible) {
    cell.accessoryView = [[UIImageView alloc]
        initWithImage:cell.isSelected ? [self checkmarkCircleIcon]
                                      : [self circleIcon]];
  } else {
    UIButton* infoButton = [UIButton buttonWithType:UIButtonTypeInfoLight];
    [infoButton setImage:[self infoCircleIcon] forState:UIControlStateNormal];
    [infoButton addTarget:self
                   action:@selector(infoButtonTapped:)
         forControlEvents:UIControlEventTouchUpInside];
    infoButton.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@ %@", kFamilyPickerInfoButtonId,
                                   _recipients[indexPath.row].email];
    cell.accessoryView = infoButton;
  }

  return cell;
}

#pragma mark - FamilyPickerConsumer

- (void)setRecipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients {
  NSSortDescriptor* eligibility =
      [[NSSortDescriptor alloc] initWithKey:@"isEligible" ascending:NO];
  NSSortDescriptor* fullName = [[NSSortDescriptor alloc] initWithKey:@"fullName"
                                                           ascending:YES];
  _recipients =
      [recipients sortedArrayUsingDescriptors:@[ eligibility, fullName ]];

  [self loadModel];
  [self.tableView reloadData];
}

#pragma mark - Items

- (TableViewLinkHeaderFooterItem*)headerItem {
  TableViewLinkHeaderFooterItem* header =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FAMILY_PICKER_SUBTITLE);
  return header;
}

- (SettingsImageDetailTextItem*)recipientItem:
    (RecipientInfoForIOSDisplay*)recipient {
  SettingsImageDetailTextItem* item =
      [[SettingsImageDetailTextItem alloc] initWithType:ItemTypeRecipient];
  item.text = recipient.fullName;
  item.detailText = recipient.email;
  item.image = recipient.profileImage;
  return item;
}

#pragma mark - Public

- (void)setupLeftBackButton {
  UIBarButtonItem* backButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_PASSWORD_SHARING_FAMILY_PICKER_BACK_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(backButtonTapped)];
  self.navigationItem.leftBarButtonItem = backButton;
  self.navigationItem.leftBarButtonItem.accessibilityIdentifier =
      kFamilyPickerBackButtonId;
}

- (void)setupLeftCancelButton {
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonTapped)];
  self.navigationItem.leftBarButtonItem.accessibilityIdentifier =
      kFamilyPickerCancelButtonId;
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self.delegate learnMoreLinkWasTapped];
}

#pragma mark - Private

- (UIImage*)checkmarkCircleIcon {
  return DefaultSymbolWithPointSize(kCheckmarkCircleFillSymbol,
                                    kAccessorySymbolSize);
}

- (UIImage*)circleIcon {
  return DefaultSymbolWithPointSize(kCircleSymbol, kAccessorySymbolSize);
}

- (UIImage*)infoCircleIcon {
  return DefaultSymbolWithPointSize(kInfoCircleSymbol, kAccessorySymbolSize);
}

- (void)infoButtonTapped:(UIButton*)button {
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FAMILY_MEMBER_INELIGIBLE);

  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };

  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    // Opening HC article is handled by the delegate.
    NSLinkAttributeName : @"",
  };

  PopoverLabelViewController* popoverViewController =
      [[PopoverLabelViewController alloc]
          initWithPrimaryAttributedString:AttributedStringFromStringWithLink(
                                              text, textAttributes,
                                              linkAttributes)
                secondaryAttributedString:nil];
  popoverViewController.delegate = self;
  popoverViewController.popoverPresentationController.sourceView = button;
  popoverViewController.popoverPresentationController.sourceRect =
      button.bounds;
  popoverViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;

  [self presentViewController:popoverViewController
                     animated:YES
                   completion:nil];
}

- (void)backButtonTapped {
  [self.delegate familyPickerNavigatedBack:self];
}

- (void)cancelButtonTapped {
  [self.delegate familyPickerWasDismissed:self];
}

- (void)shareButtonTapped {
  CHECK(self.tableView.indexPathsForSelectedRows.count > 0u);

  NSMutableArray<RecipientInfoForIOSDisplay*>* selectedRecipients =
      [NSMutableArray array];
  for (NSIndexPath* indexPath in self.tableView.indexPathsForSelectedRows) {
    [selectedRecipients addObject:_recipients[indexPath.row]];
  }
  [self.delegate familyPickerClosed:self
             withSelectedRecipients:selectedRecipients];
}

// Enables share button if any row is selected or disables it otherwise.
- (void)setShareButtonStatus {
  self.navigationItem.rightBarButtonItem.enabled =
      self.tableView.indexPathsForSelectedRows.count > 0;
}

@end
