// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/magic_stack_parcel_list_half_sheet_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const CGFloat kEllipsisButtonPointSize = 24;
const CGFloat kParcelCellMargin = 16;

enum SectionIdentifier : NSInteger {
  SectionIdentifierParcelList = kSectionIdentifierEnumZero,
};

enum ItemType : NSInteger {
  ItemTypeParcelItem = kItemTypeEnumZero,
};

}  // namespace

// Delegate for ParcelListCell.
@protocol ParcelListCellDelegate

// Indicates to the delegate to untrack `parcelID`.
- (void)untrackParcel:(NSString*)parcelID carrier:(ParcelType)carrier;

@end

// Cell displaying a tracked parcel with an option button.
@interface ParcelListCell : TableViewCell

// Configures this cell with the `config`.
- (void)configureCell:(ParcelTrackingItem*)config;

// Delegate for this cell.
@property(nonatomic, weak) id<ParcelListCellDelegate> delegate;

// Tracking number for this cell.
@property(nonatomic, copy) NSString* parcelID;

// Button with more options for this cell.
@property(nonatomic, strong) UIButton* ellipsisButton;

@end

@implementation ParcelListCell {
  ParcelTrackingModuleView* _parcelTrackingView;
  ParcelType _carrier;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _parcelTrackingView =
        [[ParcelTrackingModuleView alloc] initWithFrame:CGRectZero];

    // Add empty view to serve as spacing between the parcel contents and the
    // options button that dynamically expands horizontally to fill space.
    UIView* emptySpaceFiller = [[UIView alloc] init];
    [emptySpaceFiller
        setContentHuggingPriority:UILayoutPriorityDefaultLow
                          forAxis:UILayoutConstraintAxisHorizontal];
    self.ellipsisButton = [[UIButton alloc] init];

    UIImage* ellipsisImage = DefaultSymbolWithPointSize(
        kEllipsisCircleFillSymbol, kEllipsisButtonPointSize);
    UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
        configurationWithPointSize:kEllipsisButtonPointSize
                            weight:UIImageSymbolWeightMedium
                             scale:UIImageSymbolScaleMedium];
    UIImageSymbolConfiguration* colorConfig =
        [UIImageSymbolConfiguration configurationWithPaletteColors:@[
          [UIColor colorNamed:kBlue600Color],
          [UIColor colorNamed:kGroupedPrimaryBackgroundColor]
        ]];
    config = [config configurationByApplyingConfiguration:colorConfig];
    ellipsisImage = [ellipsisImage imageByApplyingSymbolConfiguration:config];
    [self.ellipsisButton setImage:ellipsisImage forState:UIControlStateNormal];

    __weak __typeof(self) weakSelf = self;
    UIAction* untrackAction = [UIAction
        actionWithTitle:l10n_util::GetNSString(
                            IDS_IOS_PARCEL_TRACKING_CONTEXT_MENU_UNTRACK)
                  image:[UIImage imageNamed:@"untrack_parcel"]
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf untrackParcel];
                }];
    untrackAction.attributes = UIMenuElementAttributesDestructive;
    self.ellipsisButton.menu = [UIMenu menuWithChildren:@[ untrackAction ]];
    self.ellipsisButton.showsMenuAsPrimaryAction = YES;

    UIStackView* containerStackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          _parcelTrackingView, emptySpaceFiller, self.ellipsisButton
        ]];
    containerStackView.axis = UILayoutConstraintAxisHorizontal;
    containerStackView.alignment = UIStackViewAlignmentCenter;
    containerStackView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:containerStackView];
    AddSameConstraintsWithInsets(
        containerStackView, self.contentView,
        NSDirectionalEdgeInsetsMake(kParcelCellMargin, kParcelCellMargin,
                                    kParcelCellMargin, kParcelCellMargin));
  }
  return self;
}

- (void)configureCell:(ParcelTrackingItem*)config {
  self.parcelID = config.parcelID;
  _carrier = config.parcelType;
  [_parcelTrackingView configureView:config];
  self.accessibilityElements = @[ _parcelTrackingView, self.ellipsisButton ];
}

- (void)untrackParcel {
  [self.delegate untrackParcel:self.parcelID carrier:_carrier];
}

@end

// Model class for ParcelListCell.
@interface ParcelListItem : TableViewItem
// Config for the displayed Parcel
@property(nonatomic, strong) ParcelTrackingItem* config;
@end

@implementation ParcelListItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ParcelListCell class];
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(ParcelListCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell configureCell:self.config];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
}

@end

@interface MagicStackParcelListHalfSheetTableViewController () <
    ParcelListCellDelegate>
@end

@implementation MagicStackParcelListHalfSheetTableViewController {
  NSMutableArray<ParcelTrackingItem*>* _parcels;
}

- (instancetype)initWithParcels:(NSArray<ParcelTrackingItem*>*)parcels {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _parcels = [parcels mutableCopy];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE);
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self.delegate
                           action:@selector(dismissParcelListHalfSheet)];
  self.navigationItem.rightBarButtonItem = dismissButton;

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierParcelList];

  for (ParcelTrackingItem* item in _parcels) {
    ParcelListItem* parcelListItem =
        [[ParcelListItem alloc] initWithType:ItemTypeParcelItem];
    parcelListItem.config = item;
    [self.tableViewModel addItem:parcelListItem
         toSectionWithIdentifier:SectionIdentifierParcelList];
  }
}

#pragma mark - ParcelListCellDelegate

- (void)untrackParcel:(NSString*)parcelID carrier:(ParcelType)carrier {
  NSIndexPath* parcelItemIndexPath = [self indexPathOfParcelWithID:parcelID];
  CHECK(parcelItemIndexPath);
  [self.tableViewModel removeItemWithType:ItemTypeParcelItem
                fromSectionWithIdentifier:SectionIdentifierParcelList
                                  atIndex:parcelItemIndexPath.row];
  [self.tableView deleteRowsAtIndexPaths:@[ parcelItemIndexPath ]
                        withRowAnimation:UITableViewRowAnimationAutomatic];
  [_parcels removeObjectAtIndex:parcelItemIndexPath.row];
  [self.delegate untrackParcel:parcelID carrier:carrier];
}

#pragma mark UITableViewDelegate

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ParcelListCell* parcelListCell =
      base::apple::ObjCCastStrict<ParcelListCell>(cell);
  parcelListCell.delegate = self;
  parcelListCell.ellipsisButton.accessibilityHint = [l10n_util::GetNSString(
      IDS_IOS_PARCEL_TRACKING_MODAL_INFOBAR_PACKAGE_NUMBER)
      stringByAppendingString:parcelListCell.parcelID];
  return cell;
}

#pragma mark Private

// Returns the indexPath location of the item with a matching `parcelID`.
- (NSIndexPath*)indexPathOfParcelWithID:(NSString*)parcelID {
  for (NSUInteger index = 0; index < [_parcels count]; index++) {
    ParcelTrackingItem* item = _parcels[index];
    if ([item.parcelID isEqualToString:parcelID]) {
      return
          [self.tableViewModel indexPathForItemType:ItemTypeParcelItem
                                  sectionIdentifier:SectionIdentifierParcelList
                                            atIndex:index];
    }
  }
  return nil;
}

@end
