// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/debugger/ui/aim_debugger_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/aim/debugger/ui/aim_debugger_mutator.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

typedef NS_ENUM(NSUInteger, AimDebuggerSection) {
  AimDebuggerSectionStatus = 0,
  AimDebuggerSectionDetails,
};

typedef NS_ENUM(NSUInteger, AimDebuggerItemType) {
  AimDebuggerItemStatusEligibility = 0,
  AimDebuggerItemPolicy,
  AimDebuggerItemDSE,
  AimDebuggerItemServer,
  AimDebuggerItemSource,
  AimDebuggerItemResponse,
  AimDebuggerItemActionRequest,
  AimDebuggerItemActionApply,
  AimDebuggerItemActionCopy,
  AimDebuggerItemActionView,
  AimDebuggerItemActionDraft,
};

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item_delegate.h"

@interface AimDebuggerViewController () <TableViewMultiLineTextEditItemDelegate>
@end

@implementation AimDebuggerViewController {
  AimEligibilitySet _eligibilityStatus;
  NSString* _serverResponseSource;
  NSString* _base64Response;
  UITableViewDiffableDataSource<NSNumber*, TableViewItem*>* _dataSource;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"AIM Eligibility Debugger";
  self.tableView.allowsSelection = YES;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  [self loadModel];
}

#pragma mark - AimDebuggerConsumer

- (void)setEligibilityStatus:(AimEligibilitySet)status {
  _eligibilityStatus = status;
  [self updateSnapshot];
}

- (void)setServerResponse:(NSString*)base64Response {
  _base64Response = base64Response;
  [self updateSnapshot];
}

- (void)setResponseSource:(NSString*)source {
  _serverResponseSource = source;
  [self updateSnapshot];
}

#pragma mark - Actions

- (void)didTapRequestButton {
  [self.mutator didTapRequestServerEligibility];
}

- (void)didTapApplyButton {
  [self.mutator didTapApplyResponse:_base64Response];
}

- (void)didTapCopyButton {
  [self.mutator didTapCopyResponse:_base64Response];
}

- (void)didTapCopyViewLinkButton {
  [self.mutator didTapCopyViewLink:_base64Response];
}

- (void)didTapCopyDraftLinkButton {
  [self.mutator didTapCopyDraftLink];
}

#pragma mark - TableViewMultiLineTextEditItemDelegate

- (void)textViewItemDidChange:(TableViewMultiLineTextEditItem*)tableViewItem {
  _base64Response = tableViewItem.text;
}

#pragma mark - Private

- (void)loadModel {
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          TableViewItem* item) {
             return [weakSelf cellForTableView:tableView
                                          item:item
                                     indexPath:indexPath];
           }];

  RegisterTableViewCell<LegacyTableViewCell>(self.tableView);
  RegisterTableViewCell<TableViewMultiLineTextEditCell>(self.tableView);
  RegisterTableViewCell<TableViewTextButtonCell>(self.tableView);

  [self updateSnapshot];
}

- (void)updateSnapshot {
  NSDiffableDataSourceSnapshot<NSNumber*, TableViewItem*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  [snapshot appendSectionsWithIdentifiers:@[
    @(AimDebuggerSectionStatus),
    @(AimDebuggerSectionDetails),
  ]];

  // Status Section
  TableViewDetailIconItem* statusItem = [[TableViewDetailIconItem alloc]
      initWithType:AimDebuggerItemStatusEligibility];
  statusItem.text = @"Overall Status";
  BOOL eligible = _eligibilityStatus.Has(AimEligibilityCheck::kIsEligible);
  statusItem.detailText = eligible ? @"Eligible" : @"Not Eligible";
  statusItem.iconImage =
      eligible
          ? DefaultSymbolTemplateWithPointSize(kCheckmarkCircleFillSymbol, 18)
          : DefaultSymbolTemplateWithPointSize(kXMarkCircleFillSymbol, 18);
  statusItem.iconTintColor = eligible ? [UIColor colorNamed:kGreenColor]
                                      : [UIColor colorNamed:kRedColor];
  [snapshot appendItemsWithIdentifiers:@[ statusItem ]
             intoSectionWithIdentifier:@(AimDebuggerSectionStatus)];

  if (!_eligibilityStatus.empty()) {
    TableViewDetailIconItem* policyItem =
        [[TableViewDetailIconItem alloc] initWithType:AimDebuggerItemPolicy];
    policyItem.text = @"AiModeSettings Policy";
    BOOL allowed =
        _eligibilityStatus.Has(AimEligibilityCheck::kIsEligibleByPolicy);
    policyItem.detailText = allowed ? @"Allowed" : @"Not Allowed";
    policyItem.iconImage =
        allowed ? DefaultSymbolTemplateWithPointSize(kCheckmarkSymbol, 18)
                : DefaultSymbolTemplateWithPointSize(kXMarkSymbol, 18);
    policyItem.iconTintColor = allowed ? [UIColor colorNamed:kGreenColor]
                                       : [UIColor colorNamed:kRedColor];

    TableViewDetailIconItem* dseItem =
        [[TableViewDetailIconItem alloc] initWithType:AimDebuggerItemDSE];
    dseItem.text = @"Default Search Engine";
    BOOL google = _eligibilityStatus.Has(AimEligibilityCheck::kIsEligibleByDse);
    dseItem.detailText = google ? @"Google" : @"Other";
    dseItem.iconImage =
        google ? DefaultSymbolTemplateWithPointSize(kCheckmarkSymbol, 18)
               : DefaultSymbolTemplateWithPointSize(kXMarkSymbol, 18);
    dseItem.iconTintColor = google ? [UIColor colorNamed:kGreenColor]
                                   : [UIColor colorNamed:kRedColor];

    TableViewDetailIconItem* serverItem =
        [[TableViewDetailIconItem alloc] initWithType:AimDebuggerItemServer];
    serverItem.text = @"Server Eligibility";
    BOOL serverEligible =
        _eligibilityStatus.Has(AimEligibilityCheck::kIsEligibleByServer);
    serverItem.detailText = serverEligible ? @"Eligible" : @"Not Eligible";
    serverItem.iconImage =
        serverEligible
            ? DefaultSymbolTemplateWithPointSize(kCheckmarkSymbol, 18)
            : DefaultSymbolTemplateWithPointSize(kXMarkSymbol, 18);
    serverItem.iconTintColor = serverEligible ? [UIColor colorNamed:kGreenColor]
                                              : [UIColor colorNamed:kRedColor];

    [snapshot appendItemsWithIdentifiers:@[ policyItem, dseItem, serverItem ]
               intoSectionWithIdentifier:@(AimDebuggerSectionStatus)];
  }

  // Details Section
  TableViewDetailIconItem* sourceItem =
      [[TableViewDetailIconItem alloc] initWithType:AimDebuggerItemSource];
  sourceItem.text = @"Eligibility Response Source";
  sourceItem.detailText = _serverResponseSource ?: @"Unknown";

  TableViewMultiLineTextEditItem* responseItem =
      [[TableViewMultiLineTextEditItem alloc]
          initWithType:AimDebuggerItemResponse];
  responseItem.label = @"Eligibility Response";
  responseItem.text = _base64Response;
  responseItem.delegate = self;
  responseItem.editingEnabled = YES;

  TableViewTextButtonItem* requestItem = [[TableViewTextButtonItem alloc]
      initWithType:AimDebuggerItemActionRequest];
  requestItem.buttonText = @"Request";

  TableViewTextButtonItem* saveItem =
      [[TableViewTextButtonItem alloc] initWithType:AimDebuggerItemActionApply];
  saveItem.buttonText = @"Apply";

  TableViewTextButtonItem* copyItem =
      [[TableViewTextButtonItem alloc] initWithType:AimDebuggerItemActionCopy];
  copyItem.buttonText = @"Copy Response";

  TableViewTextButtonItem* viewItem =
      [[TableViewTextButtonItem alloc] initWithType:AimDebuggerItemActionView];
  viewItem.buttonText = @"Copy View Link";

  TableViewTextButtonItem* draftItem =
      [[TableViewTextButtonItem alloc] initWithType:AimDebuggerItemActionDraft];
  draftItem.buttonText = @"Copy Draft Link";

  [snapshot appendItemsWithIdentifiers:@[
    sourceItem, responseItem, requestItem, saveItem, copyItem, viewItem,
    draftItem
  ]
             intoSectionWithIdentifier:@(AimDebuggerSectionDetails)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                                item:(TableViewItem*)item
                           indexPath:(NSIndexPath*)indexPath {
  AimDebuggerItemType itemType = (AimDebuggerItemType)item.type;
  switch (itemType) {
    case AimDebuggerItemResponse: {
      TableViewMultiLineTextEditCell* cell =
          DequeueTableViewCell<TableViewMultiLineTextEditCell>(tableView);
      [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
      return cell;
    }
    case AimDebuggerItemActionRequest:
    case AimDebuggerItemActionCopy:
    case AimDebuggerItemActionView:
    case AimDebuggerItemActionDraft:
    case AimDebuggerItemActionApply: {
      TableViewTextButtonCell* cell =
          DequeueTableViewCell<TableViewTextButtonCell>(tableView);
      [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
      cell.button.userInteractionEnabled = NO;
      return cell;
    }
    case AimDebuggerItemStatusEligibility:
    case AimDebuggerItemPolicy:
    case AimDebuggerItemDSE:
    case AimDebuggerItemServer:
    case AimDebuggerItemSource:
    default: {
      LegacyTableViewCell* cell =
          DequeueTableViewCell<LegacyTableViewCell>(tableView);
      [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
      return cell;
    }
  }
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  TableViewItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];
  AimDebuggerItemType itemType = (AimDebuggerItemType)item.type;

  switch (itemType) {
    case AimDebuggerItemActionRequest:
      [self didTapRequestButton];
      break;
    case AimDebuggerItemActionCopy:
      [self didTapCopyButton];
      break;
    case AimDebuggerItemActionView:
      [self didTapCopyViewLinkButton];
      break;
    case AimDebuggerItemActionDraft:
      [self didTapCopyDraftLinkButton];
      break;
    case AimDebuggerItemActionApply:
      [self didTapApplyButton];
      break;
    default:
      break;
  }
}

@end
