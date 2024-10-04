// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_last_visited_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/page_info/core/page_info_action.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/history/ui_bundled/base_history_view_controller+subclassing.h"
#import "ios/chrome/browser/history/ui_bundled/history_entry_item.h"
#import "ios/chrome/browser/history/ui_bundled/history_table_view_controller_delegate.h"
#import "ios/chrome/browser/history/ui_bundled/history_ui_constants.h"
#import "ios/chrome/browser/ui/page_info/page_info_last_visited_view_controller_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using history::BrowsingHistoryService;

namespace {
enum ItemType : NSInteger {
  kItemTypeHistoryEntry = kItemTypeEnumZero,
};
}  // namespace

@interface PageInfoLastVisitedViewController () {
  // Only history entries matching "_hostName" will be displayed.
  NSString* _hostName;
}

// NavigationController UIToolbar Buttons.
@property(nonatomic, strong) UIBarButtonItem* showFullHistoryButton;
@end

@implementation PageInfoLastVisitedViewController

#pragma mark - Public

- (instancetype)initWithHostName:(NSString*)hostName {
  _hostName = hostName;
  return [super init];
}

#pragma mark - Superclass overrides

- (void)filterResults:(NSMutableArray*)resultsItems
          searchQuery:(NSString*)searchQuery {
}

- (void)updateEntriesStatusMessageWithMessage:(NSString*)message
                       messageWillContainLink:(BOOL)messageWillContainLink {
  if (self.shouldShowNoticeAboutOtherFormsOfBrowsingHistory) {
    message = l10n_util::GetNSString(IDS_IOS_HISTORY_OTHER_FORMS_OF_HISTORY);
    messageWillContainLink = YES;
  }
  [super updateEntriesStatusMessageWithMessage:message
                        messageWillContainLink:messageWillContainLink];
}

#pragma mark - TableViewModel

- (void)viewDidLoad {
  [super viewDidLoad];

  [self showHistoryMatchingQuery:_hostName];

  // Configures NavigationController Toolbar buttons.
  [self setToolbarItems:[self toolbarButtons] animated:YES];

  // NavigationController configuration.
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  self.title = l10n_util::GetNSString(IDS_PAGE_INFO_HISTORY);
  self.navigationItem.prompt = _hostName;

  // Adds the "Done" button and hooks it up to `dismissLastVisited`.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissLastVisited)];
  [dismissButton setAccessibilityIdentifier:
                     kHistoryNavigationControllerDoneButtonIdentifier];
  self.navigationItem.rightBarButtonItem = dismissButton;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  // Only record metrics if a ItemTypeHistoryEntry was selected.
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  if (item.type == kItemTypeHistoryEntry) {
    base::RecordAction(
        base::UserMetricsAction("PageInfo.History.EntryClicked"));
    base::UmaHistogramEnumeration(page_info::kWebsiteSettingsActionHistogram,
                                  page_info::PAGE_INFO_HISTORY_ENTRY_CLICKED);
  }
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
}

#pragma mark - HistoryEntryItemDelegate

- (void)historyEntryItemDidRequestDelete:(HistoryEntryItem*)item {
  // Only record metrics if the selected history entry is valid.
  NSInteger sectionIdentifier =
      [self.entryInserter sectionIdentifierForTimestamp:item.timestamp];
  if ([self.tableViewModel hasSectionForSectionIdentifier:sectionIdentifier] &&
      [self.tableViewModel hasItem:item
           inSectionWithIdentifier:sectionIdentifier]) {
    base::RecordAction(
        base::UserMetricsAction("PageInfo.History.EntryRemoved"));
    base::UmaHistogramEnumeration(page_info::kWebsiteSettingsActionHistogram,
                                  page_info::PAGE_INFO_HISTORY_ENTRY_REMOVED);
  }
  [super historyEntryItemDidRequestDelete:item];
}

#pragma mark - Private methods

// Displays the full history.
- (void)showFullHistory {
  [self.lastVisitedDelegate displayFullHistory];
}

// Creates button displaying "Show Full History".
- (UIBarButtonItem*)showFullHistoryButton {
  if (!_showFullHistoryButton) {
    NSString* titleString =
        l10n_util::GetNSString(IDS_HISTORY_SHOWFULLHISTORY_LINK);
    _showFullHistoryButton =
        [[UIBarButtonItem alloc] initWithTitle:titleString
                                         style:UIBarButtonItemStylePlain
                                        target:self
                                        action:@selector(showFullHistory)];
    _showFullHistoryButton.accessibilityIdentifier =
        kHistoryToolbarShowFullHistoryButtonIdentifier;
    _showFullHistoryButton.tintColor = [UIColor colorNamed:kBlueColor];
  }
  return _showFullHistoryButton;
}

// Creates an empty button.
- (UIBarButtonItem*)createSpacerButton {
  return [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
}

// Returns the toolbar buttons for the current state.
- (NSArray<UIBarButtonItem*>*)toolbarButtons {
  // TODO(crbug.com/364862099): Decide what buttons to display if there is no
  // history.
  return @[
    [self createSpacerButton], self.showFullHistoryButton,
    [self createSpacerButton]
  ];
}

// Dismisses the Last Visited VC.
- (void)dismissLastVisited {
  // TODO(crbug.com/364824898): Record that Last Visited was dismissed.
  [self.pageInfoCommandsHandler hidePageInfo];
}

@end
