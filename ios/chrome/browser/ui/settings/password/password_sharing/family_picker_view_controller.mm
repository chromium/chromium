// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
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
@end

@implementation FamilyPickerViewController

// Contains information about the potential recipients of the user.
NSArray<RecipientInfoForIOSDisplay*>* _recipients;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UINavigationItem* navigationItem = self.navigationItem;
  navigationItem.title = l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_TITLE);
  navigationItem.rightBarButtonItem = [self createShareButton];

  UITableView* tableView = self.tableView;
  tableView.allowsMultipleSelection = YES;
  tableView.accessibilityIdentifier = kFamilyPickerTableViewID;

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

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_recipients[indexPath.row].isEligible) {
    [tableView cellForRowAtIndexPath:indexPath].accessoryView =
        [self checkmarkCircleIcon];
    [self setShareButtonStatus];
  }
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_recipients[indexPath.row].isEligible) {
    [tableView cellForRowAtIndexPath:indexPath].accessoryView =
        [self circleIcon];
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
    cell.accessoryView =
        [tableView.indexPathsForSelectedRows containsObject:indexPath]
            ? [self checkmarkCircleIcon]
            : [self circleIcon];
  } else {
    UIButton* infoButton = [UIButton buttonWithType:UIButtonTypeInfoLight];
    [infoButton setImage:[self infoCircleIcon] forState:UIControlStateNormal];
    [infoButton addTarget:self
                   action:@selector(infoButtonTapped:)
         forControlEvents:UIControlEventTouchUpInside];
    infoButton.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@ %@", kFamilyPickerInfoButtonID,
                                   _recipients[indexPath.row].email];
    infoButton.accessibilityLabel = [NSString
        stringWithFormat:@"%@, %@", _recipients[indexPath.row].fullName,
                         l10n_util::GetNSString(
                             IDS_IOS_INFO_BUTTON_ACCESSIBILITY_HINT)];
    cell.accessoryView = infoButton;

    // Make the cell a non-accessible element to make the info button accessible
    // to VoiceOver.
    cell.isAccessibilityElement = NO;
    infoButton.isAccessibilityElement = YES;
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
  if (_recipients.count == 1 && _recipients[0].isEligible) {
    [self selectFirstRow];
  }
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
  item.image = CircularImageFromImage(recipient.profileImage,
                                      kAccountProfilePhotoDimension);
  item.accessibilityTraits = UIAccessibilityTraitButton;
  if (!recipient.isEligible) {
    item.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
    item.textColor = [UIColor colorNamed:kTextTertiaryColor];
    item.detailTextColor = [UIColor colorNamed:kTextTertiaryColor];
    item.imageViewAlpha = 0.4f;
  }
  return item;
}

#pragma mark - Public

- (void)setupLeftBackButton {
  UINavigationItem* navigationItem = self.navigationItem;
  navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_PASSWORD_SHARING_FAMILY_PICKER_BACK_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(backButtonTapped)];
  navigationItem.leftBarButtonItem.accessibilityIdentifier =
      kFamilyPickerBackButtonID;
}

- (void)setupLeftCancelButton {
  UINavigationItem* navigationItem = self.navigationItem;
  navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonTapped)];
  navigationItem.leftBarButtonItem.accessibilityIdentifier =
      kFamilyPickerCancelButtonID;
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self.delegate learnMoreLinkWasTapped];
}

#pragma mark - Private

// Creates accessory view for a cell with selected sharing recipient.
- (UIImageView*)checkmarkCircleIcon {
  return [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(kCheckmarkCircleFillSymbol,
                                               kAccessorySymbolSize)];
}

// Creates accessory view for a cell with unselected sharing recipient.
- (UIImageView*)circleIcon {
  UIImageView* circleIcon = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(kCircleSymbol,
                                               kAccessorySymbolSize)];
  circleIcon.tintColor = [UIColor colorNamed:kGrey300Color];
  return circleIcon;
}

// Creates accessory view for a cell with ineligible sharing recipient.
- (UIImage*)infoCircleIcon {
  return DefaultSymbolWithPointSize(kInfoCircleSymbol, kAccessorySymbolSize);
}

// Displays popup with information about password sharing ineligibility.
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

// Notifies the delegate that the previous view should be displayed.
- (void)backButtonTapped {
  [self.delegate familyPickerNavigatedBack:self];
}

// Notifies the delegate that the view should be dismissed.
- (void)cancelButtonTapped {
  [self.delegate familyPickerWasDismissed:self];
}

// Notifies the delegate that the user has confirmed the recipient selection.
- (void)shareButtonTapped {
  UITableView* tableView = self.tableView;
  CHECK(tableView.indexPathsForSelectedRows.count > 0u);

  NSMutableArray<RecipientInfoForIOSDisplay*>* selectedRecipients =
      [NSMutableArray array];
  for (NSIndexPath* indexPath in tableView.indexPathsForSelectedRows) {
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

// Selects first row of the table view.
- (void)selectFirstRow {
  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  UITableView* tableView = self.tableView;
  [tableView selectRowAtIndexPath:indexPath
                         animated:NO
                   scrollPosition:UITableViewScrollPositionNone];
  [self tableView:tableView didSelectRowAtIndexPath:indexPath];
}

// Creates right share button.
- (UIBarButtonItem*)createShareButton {
  UIBarButtonItem* shareButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_PASSWORD_SHARING_SHARE_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(shareButtonTapped)];
  shareButton.enabled = NO;
  shareButton.accessibilityIdentifier = kFamilyPickerShareButtonID;
  return shareButton;
}

@end
