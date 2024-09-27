// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/parcel_tracking/infobar_parcel_tracking_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/parcel_tracking/infobar_parcel_tracking_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/parcel_tracking/infobar_parcel_tracking_presenter.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/annotations/custom_text_checking_result.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

enum SectionIdentifier {
  kContent = kSectionIdentifierEnumZero,
};

enum ItemType {
  kCarrier = kItemTypeEnumZero,
  kTrackingNumber,
  kTrackButton,
};

// Height of the "Report an issue" table footer.
const CGFloat kFooterFrameHeight = 50;

// Width of the empty accessory view added after carrier name.
const CGFloat kEmptyAccessoryViewWidth = 0.5;

// Key used to access carrier info from CustomTextCheckingResult.
NSString* const kCarrierKey = @"carrier";

}  // namespace

@interface InfobarParcelTrackingTableViewController () <
    UIEditMenuInteractionDelegate>

// Used to create and show the actions users can execute when they tap on a row
// in the tableView. These actions are displayed a pop-up.
@property(nonatomic, strong)
    UIEditMenuInteraction* editMenu API_AVAILABLE(ios(16));

@end

@implementation InfobarParcelTrackingTableViewController {
  // List of parcels.
  NSArray<CustomTextCheckingResult*>* parcelList_;
  // Indicates whether the parcels in `parcelList_` are being tracked.
  bool trackingParcels_;
  // Delegate for this view controller.
  id<InfobarModalDelegate, InfobarParcelTrackingModalDelegate> delegate_;
  // Presenter for this view controller.
  id<InfobarParcelTrackingPresenter> presenter_;
}

- (instancetype)
    initWithDelegate:
        (id<InfobarModalDelegate, InfobarParcelTrackingModalDelegate>)delegate
           presenter:(id<InfobarParcelTrackingPresenter>)presenter {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    delegate_ = delegate;
    presenter_ = presenter;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.sectionHeaderHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissInfobarModal)];
  cancelButton.accessibilityIdentifier = kInfobarModalCancelButton;

  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  self.editMenu = [[UIEditMenuInteraction alloc] initWithDelegate:self];
  [self.tableView addInteraction:self.editMenu];

  [self loadModel];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [delegate_ modalInfobarWasDismissed:self];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.tableView.scrollEnabled =
      self.tableView.contentSize.height > self.view.frame.size.height;
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifier::kContent];

  int previousCarrier = -1;

  for (size_t i = 0; i < parcelList_.count; i++) {
    CustomTextCheckingResult* parcel = [parcelList_ objectAtIndex:i];
    // If package has different carrier, add table row with carrier name.
    if (parcel.carrier != previousCarrier) {
      [model addItem:[self itemForCarrier:parcel.carrier]
          toSectionWithIdentifier:SectionIdentifier::kContent];
      previousCarrier = parcel.carrier;
    }
    [model addItem:[self itemForTrackingNumber:parcel.carrierNumber atIndex:i]
        toSectionWithIdentifier:SectionIdentifier::kContent];
  }

  [model addItem:[self trackButtonItem]
      toSectionWithIdentifier:SectionIdentifier::kContent];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case kTrackingNumber: {
      // Present an edit menu.
      CGRect row = [self.tableView rectForRowAtIndexPath:indexPath];
      CGPoint editMenuLocation = CGPointMake(CGRectGetMidX(row), row.origin.y);
      UIEditMenuConfiguration* configuration = [UIEditMenuConfiguration
          configurationWithIdentifier:[NSNumber numberWithInt:itemType]
                          sourcePoint:editMenuLocation];
      [self.editMenu presentEditMenuWithConfiguration:configuration];
    } break;
    default:
      break;
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case ItemType::kTrackButton: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(trackButtonWasPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      // Hide the separator if "Track This Parcel" button is displayed.
      if (!trackingParcels_) {
        tableViewTextButtonCell.separatorInset =
            UIEdgeInsetsMake(0, 0, 0, self.tableView.bounds.size.width);
      }
      break;
    }
    case ItemType::kTrackingNumber: {
      cell.accessoryView = [self copyButton];
      cell.accessoryView.tintColor = [UIColor colorNamed:kBlueColor];
      break;
    }
    case ItemType::kCarrier:
      CGRect frame =
          CGRectMake(0, 0, kEmptyAccessoryViewWidth, kSymbolAccessoryPointSize);
      cell.accessoryView = [[UIImageView alloc] initWithFrame:frame];
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
  }

  return cell;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  return [self reportIssueButton];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return kFooterFrameHeight;
}

#pragma mark - InfobarParcelTrackingModalConsumer

- (void)setParcelList:(NSArray<CustomTextCheckingResult*>*)parcels
    withTrackingStatus:(bool)tracking {
  NSSortDescriptor* sortDescriptor =
      [[NSSortDescriptor alloc] initWithKey:kCarrierKey ascending:NO];
  parcelList_ = [parcels sortedArrayUsingDescriptors:@[ sortDescriptor ]];
  trackingParcels_ = tracking;
}

#pragma mark - UIEditMenuInteractionDelegate

- (UIMenu*)editMenuInteraction:(UIEditMenuInteraction*)interaction
          menuForConfiguration:(UIEditMenuConfiguration*)configuration
              suggestedActions:(NSArray<UIMenuElement*>*)suggestedActions
    API_AVAILABLE(ios(16)) {
  UITableViewCell* cell = [self.tableView
      cellForRowAtIndexPath:self.tableView.indexPathForSelectedRow];
  TableViewMultiDetailTextCell* tableViewMultiDetailTextCell =
      base::apple::ObjCCastStrict<TableViewMultiDetailTextCell>(cell);
  UIAction* copy = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_COPY_BUTTON)
                image:nil
           identifier:nil
              handler:^(__kindof UIAction* _Nonnull action) {
                StoreTextInPasteboard(
                    tableViewMultiDetailTextCell.trailingDetailTextLabel.text);
              }];
  [self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow
                                animated:YES];
  return [UIMenu menuWithChildren:@[ copy ]];
}

#pragma mark - Private

// Dismisses the modal.
- (void)dismissInfobarModal {
  [delegate_ dismissInfobarModal:self];
}

// Creates and returns the "Report an issue" button.
- (UIButton*)reportIssueButton {
  UIButton* button = [[UIButton alloc] init];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.attributedTitle = [self reportIssueString];
  button.configuration = buttonConfiguration;
  [button addTarget:self
                action:@selector(reportIssueButtonWasPressed:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Presents the "Send Feeback" page.
- (void)reportIssueButtonWasPressed:(UIButton*)sender {
  [presenter_ showReportIssueView];
}

// Creates and returns the "copy" button.
- (UIButton*)copyButton {
  UIImage* copyImage = DefaultSymbolTemplateWithPointSize(
      kCopyActionSymbol, kSymbolAccessoryPointSize);
  UIButton* copyButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [copyButton addTarget:self
                 action:@selector(copyButtonPressed:)
       forControlEvents:UIControlEventTouchUpInside];
  [copyButton
      setFrame:CGRectMake(0, 0, copyImage.size.width, copyImage.size.height)];
  [copyButton setImage:copyImage forState:UIControlStateNormal];
  return copyButton;
}

// Handles touch events for the "copy" button. Stores the tracking number of the
// associated cell in the pasteboard.
- (void)copyButtonPressed:(UIButton*)sender {
  UITableViewCell* cell = (UITableViewCell*)sender.superview;
  TableViewMultiDetailTextCell* tableViewMultiDetailTextCell =
      base::apple::ObjCCastStrict<TableViewMultiDetailTextCell>(cell);
  StoreTextInPasteboard(
      tableViewMultiDetailTextCell.trailingDetailTextLabel.text);
}

// Returns the TableViewItem for the given carrier.
- (TableViewMultiDetailTextItem*)itemForCarrier:(int)carrier {
  TableViewMultiDetailTextItem* carrierItem =
      [[TableViewMultiDetailTextItem alloc] initWithType:ItemType::kCarrier];
  carrierItem.text = l10n_util::GetNSString(
      IDS_IOS_PARCEL_TRACKING_MODAL_INFOBAR_DELIVERED_BY);
  carrierItem.trailingDetailText = [self stringForCarrier:carrier];
  return carrierItem;
}

// Creates and returns the TableViewItem for the tracking number at row number
// `index` in the table.
- (TableViewMultiDetailTextItem*)itemForTrackingNumber:(NSString*)trackingNumber
                                               atIndex:(size_t)index {
  TableViewMultiDetailTextItem* trackingNumberItem =
      [[TableViewMultiDetailTextItem alloc]
          initWithType:ItemType::kTrackingNumber];
  if (parcelList_.count == 1) {
    trackingNumberItem.text = l10n_util::GetNSString(
        IDS_IOS_PARCEL_TRACKING_MODAL_INFOBAR_PACKAGE_NUMBER);
  } else {
    trackingNumberItem.text =
        [NSString stringWithFormat:
                      @"%@ %lu",
                      l10n_util::GetNSString(
                          IDS_IOS_PARCEL_TRACKING_MODAL_INFOBAR_PACKAGE_LABEL),
                      index + 1];
  }
  trackingNumberItem.trailingDetailText = trackingNumber;
  return trackingNumberItem;
}

// Creates and returns the TableViewItem for the (un)track button.
- (TableViewTextButtonItem*)trackButtonItem {
  TableViewTextButtonItem* button =
      [[TableViewTextButtonItem alloc] initWithType:ItemType::kTrackButton];
  if (trackingParcels_) {
    button.buttonText =
        base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
            IDS_IOS_PARCEL_TRACKING_MODAL_INFOBAR_UNTRACK_BUTTON,
            parcelList_.count));
    button.buttonTextColor = [UIColor colorNamed:kBlueColor];
    button.buttonBackgroundColor = [UIColor clearColor];
  } else {
    button.buttonText =
        base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
            IDS_IOS_PARCEL_TRACKING_MODAL_INFOBAR_TRACK_BUTTON,
            parcelList_.count));
    button.buttonTextColor = [UIColor colorNamed:kSolidButtonTextColor];
    button.buttonBackgroundColor = [UIColor colorNamed:kBlueColor];
  }
  button.disableButtonIntrinsicWidth = YES;
  return button;
}

// (Un)tracks all packages.
- (void)trackButtonWasPressed:(UIButton*)sender {
  if (trackingParcels_) {
    [delegate_ parcelTrackingTableViewControllerDidTapUntrackAllButton];
  } else {
    [delegate_ parcelTrackingTableViewControllerDidTapTrackAllButton];
  }
  [self dismissInfobarModal];
}

// Returns a string with the name of the given carrier.
- (NSString*)stringForCarrier:(int)carrier {
  switch (carrier) {
    case 1:
      return l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_CARRIER_FEDEX);
    case 2:
      return l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_CARRIER_UPS);
    case 4:
      return l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_CARRIER_USPS);
    default:
      // Currently unsupported carriers.
      NOTREACHED_IN_MIGRATION();
      return @"";
  }
}

// Creates and returns the "Report an issue" string.
- (NSAttributedString*)reportIssueString {
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
  };

  // Add a space to have a distance with the leading icon.
  NSAttributedString* attributedString = [[NSAttributedString alloc]
      initWithString:[@" " stringByAppendingString:
                               l10n_util::GetNSString(
                                   IDS_IOS_PARCEL_TRACKING_REPORT_AN_ISSUE)]
          attributes:textAttributes];

  // Create the leading icon.
  NSTextAttachment* attachment = [[NSTextAttachment alloc] init];
  UIImage* icon = DefaultSymbolWithPointSize(kExclamationMarkBubbleSymbol,
                                             kInfobarSymbolPointSize);
  attachment.image = [icon imageWithTintColor:[UIColor colorNamed:kBlueColor]];

  // Making sure the image is well centered vertically relative to the text,
  // and also that the image scales with the text size.
  CGFloat height = attributedString.size.height;
  CGFloat capHeight =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote].capHeight;
  CGFloat verticalOffset = roundf(capHeight - height) / 2.f;
  attachment.bounds = CGRectMake(0, verticalOffset, height, height);

  NSMutableAttributedString* outputString = [[NSAttributedString
      attributedStringWithAttachment:attachment] mutableCopy];
  [outputString appendAttributedString:attributedString];
  return outputString;
}

@end
