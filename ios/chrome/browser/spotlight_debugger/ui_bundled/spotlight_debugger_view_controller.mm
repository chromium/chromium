// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/spotlight_debugger/ui_bundled/spotlight_debugger_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"
#import "ios/chrome/app/spotlight/open_tabs_spotlight_manager.h"
#import "ios/chrome/app/spotlight/reading_list_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/app/spotlight/topsites_spotlight_manager.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/highlight_button.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "url/gurl.h"

typedef NS_ENUM(NSUInteger, Sections) {
  StatusSection = 0,
  DebugCommandsSection,
  SectionCount,
};

typedef NS_ENUM(NSUInteger, StatusSectionRows) {
  AvailabilityRow = 0,
  LastIndexDateRow,
  DonatedItemsRow,
  StatusSectionRowsCount,
};

typedef NS_ENUM(NSUInteger, DebugCommandsRows) {
  ClearAllRow = 0,
  ReindexBookmarks,
  ReindexReadingList,
  ReindexOpenTabs,
  ReindexTopSites,
  DebugCommandsRowsCount,
};

@interface SpotlightDebuggerViewController ()

@property(nonatomic, strong) UIActivityIndicatorView* spinner;

@property(nonatomic, readonly) SpotlightInterface* spotlightInterface;

@end

@implementation SpotlightDebuggerViewController {
  // PrefService per a profile.
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super initWithStyle:UITableViewStyleInsetGrouped];
  if (self) {
    _spinner = [[UIActivityIndicatorView alloc]
        initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleLarge];
    _spinner.translatesAutoresizingMaskIntoConstraints = NO;
    _spotlightInterface = [SpotlightInterface defaultInterface];
    _prefService = prefService;
  }
  return self;
}

- (void)dealloc {
  [self.bookmarksManager shutdown];
  [self.readingListSpotlightManager shutdown];
  [self.openTabsSpotlightManager shutdown];
  [self.topSitesSpotlightManager shutdown];
}

#pragma mark - Public

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = @"Spotlight Debugger";
  [self.tableView registerClass:[UITableViewCell class]
         forCellReuseIdentifier:@"Cell"];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return SectionCount;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  switch (section) {
    case StatusSection:
      return StatusSectionRowsCount;
    case DebugCommandsSection:
      return DebugCommandsRowsCount;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return 0;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [tableView dequeueReusableCellWithIdentifier:@"Cell"];
  UIListContentConfiguration* content = cell.defaultContentConfiguration;

  switch (indexPath.section) {
    case StatusSection:
      switch (indexPath.row) {
        case AvailabilityRow:
          content.text = @"Spotlight Status";
          content.secondaryText = spotlight::IsSpotlightAvailable()
                                      ? @"Available"
                                      : @"Not Available";
          content.image =
              spotlight::IsSpotlightAvailable()
                  ? DefaultSymbolWithPointSize(@"checkmark.circle.fill",
                                               kSymbolAccessoryPointSize)
                  : DefaultSymbolWithPointSize(@"exclamationmark.triangle",
                                               kSymbolAccessoryPointSize);
          break;
        case LastIndexDateRow:
          content.text = @"Time since last reindexing";
          content.secondaryText = [self timeSinceLastReindexAsString];
          content.image = DefaultSymbolWithPointSize(
              @"arrow.counterclockwise.icloud", kSymbolAccessoryPointSize);
          break;
        case DonatedItemsRow:
          content.text = @"Donated items";
          content.image = DefaultSymbolWithPointSize(
              @"square.stack.3d.down.right", kSymbolAccessoryPointSize);
          cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      break;

    case DebugCommandsSection:
      switch (indexPath.row) {
        case ClearAllRow: {
          content.text = @"Clear all Spotlight entries";
          content.image = DefaultSymbolWithPointSize(@"bin.xmark",
                                                     kSymbolAccessoryPointSize);
          break;
        }
        case ReindexBookmarks: {
          content.text = @"Clear and Reindex Bookmarks";
          content.image = DefaultSymbolWithPointSize(@"bin.xmark",
                                                     kSymbolAccessoryPointSize);
          break;
        }
        case ReindexReadingList: {
          content.text = @"Clear and Reindex reading list";
          content.image = DefaultSymbolWithPointSize(@"bin.xmark",
                                                     kSymbolAccessoryPointSize);
          break;
        }
        case ReindexOpenTabs: {
          content.text = @"Clear and Reindex open tabs";
          content.image = DefaultSymbolWithPointSize(@"bin.xmark",
                                                     kSymbolAccessoryPointSize);
          break;
        }
        case ReindexTopSites: {
          content.text = @"Clear and Reindex Top sites";
          content.image = DefaultSymbolWithPointSize(@"bin.xmark",
                                                     kSymbolAccessoryPointSize);
          break;
        }
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  cell.contentConfiguration = content;
  return cell;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  switch (indexPath.section) {
    case StatusSection:
      switch (indexPath.row) {
        case AvailabilityRow:
          break;
        case LastIndexDateRow:
          break;
        case DonatedItemsRow:
          [self.delegate showAllItems];
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      break;

    case DebugCommandsSection:
      switch (indexPath.row) {
        case ClearAllRow:
          [self clearAllSpotlightEntries];
          break;
        case ReindexBookmarks:
          [self clearAndReindexBookmarks];
          break;
        case ReindexReadingList:
          [self clearAndReindexReadingList];
          break;
        case ReindexOpenTabs:
          [self clearAndReindexOpenTabs];
          break;
        case ReindexTopSites:
          [self clearAndReindexTopSites];
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - actions

- (void)clearAllSpotlightEntries {
  [self showSpinner];
  [self.spotlightInterface deleteAllSearchableItemsWithCompletionHandler:^(
                               NSError* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      UIAlertController* controller = [UIAlertController
          alertControllerWithTitle:@"Clear Entries"
                           message:error ? error.localizedDescription
                                         : @"Success"
                    preferredStyle:UIAlertControllerStyleAlert];
      [controller
          addAction:[UIAlertAction actionWithTitle:@"OK"
                                             style:UIAlertActionStyleDefault
                                           handler:nil]];
      [self presentViewController:controller animated:YES completion:nil];

      [self removeSpinner];
      [self.tableView reloadData];
    });
  }];
  _prefService->ClearPref(spotlight::kSpotlightLastIndexingDateKey);
}

- (void)clearAndReindexBookmarks {
  [self.bookmarksManager clearAndReindexModel];
  [self.tableView reloadData];
}

- (void)clearAndReindexReadingList {
  [self.readingListSpotlightManager clearAndReindexReadingList];
  [self.tableView reloadData];
}

- (void)clearAndReindexOpenTabs {
  [self.openTabsSpotlightManager clearAndReindexOpenTabs];
  [self.tableView reloadData];
}

- (void)clearAndReindexTopSites {
  [self.topSitesSpotlightManager reindexTopSites];
  [self.tableView reloadData];
}

#pragma mark - private

- (void)showSpinner {
  [self.view addSubview:self.spinner];
  [self.spinner startAnimating];

  [self.spinner.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor]
      .active = YES;
  [self.spinner.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor]
      .active = YES;
}

- (void)removeSpinner {
  [self.spinner stopAnimating];
  [self.spinner removeFromSuperview];
}

- (NSString*)timeSinceLastReindexAsString {
  const base::Time date =
      _prefService->GetTime(spotlight::kSpotlightLastIndexingDateKey);
  if (date == base::Time()) {
    return @"Never";
  }

  NSTimeInterval timeSinceReindexing =
      [[NSDate date] timeIntervalSinceDate:date.ToNSDate()];
  NSDateComponentsFormatter* formatter =
      [[NSDateComponentsFormatter alloc] init];
  formatter.unitsStyle = NSDateComponentsFormatterUnitsStyleBrief;
  formatter.allowedUnits = NSCalendarUnitNanosecond | NSCalendarUnitSecond |
                           NSCalendarUnitMinute | NSCalendarUnitHour;
  return [formatter stringFromTimeInterval:timeSinceReindexing];
}
@end
