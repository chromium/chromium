// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/spotlight_debugger/spotlight_debugger_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "base/time/time.h"
#import "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/highlight_button.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  DebugCommandsRowsCount,
};

@interface SpotlightDebuggerViewController ()

@property(nonatomic, strong) UIActivityIndicatorView* spinner;

@end

@implementation SpotlightDebuggerViewController

- (instancetype)init {
  self = [super initWithStyle:UITableViewStyleInsetGrouped];
  if (self) {
    _spinner = [[UIActivityIndicatorView alloc]
        initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleLarge];
    _spinner.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return self;
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
      NOTREACHED();
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
          content.secondaryText = [[self class] timeSinceLastReindexAsString];
          content.image = DefaultSymbolWithPointSize(
              @"arrow.counterclockwise.icloud", kSymbolAccessoryPointSize);
          break;
        case DonatedItemsRow:
          content.text = @"Donated items";
          content.secondaryText =
              [NSString stringWithFormat:@"Total count: %ld",
                                         [SpotlightLogger sharedLogger]
                                             .knownIndexedItems.count];
          content.image = DefaultSymbolWithPointSize(
              @"square.stack.3d.down.right", kSymbolAccessoryPointSize);
          cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
          break;
        default:
          NOTREACHED();
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
        default:
          NOTREACHED();
          break;
      }
      break;
    default:
      NOTREACHED();
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
          NOTREACHED();
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
        default:
          NOTREACHED();
          break;
      }
      break;
    default:
      NOTREACHED();
      break;
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - actions

- (void)clearAllSpotlightEntries {
  [self showSpinner];
  spotlight::ClearSpotlightIndexWithCompletion(^(NSError* error) {
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
  });
}

- (void)clearAndReindexBookmarks {
  base::Time startTime = base::Time::Now();

  [self.bookmarksManager
      clearAndReindexModelWithCompletionBlock:^(NSError* error) {
        base::Time endTime = base::Time::Now();
        base::TimeDelta duration = endTime - startTime;

        dispatch_async(dispatch_get_main_queue(), ^{
          UIAlertController* controller = [UIAlertController
              alertControllerWithTitle:
                  [NSString stringWithFormat:
                                @"Clearing and Reindexing complete in %lld ms",
                                duration.InMilliseconds()]
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

+ (NSString*)timeSinceLastReindexAsString {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];

  NSDate* date = base::mac::ObjCCast<NSDate>(
      [userDefaults objectForKey:@(spotlight::kSpotlightLastIndexingDateKey)]);
  if (!date) {
    return @"Never";
  }

  NSTimeInterval timeSinceReindexing =
      [[NSDate date] timeIntervalSinceDate:date];
  NSDateComponentsFormatter* formatter =
      [[NSDateComponentsFormatter alloc] init];
  formatter.unitsStyle = NSDateComponentsFormatterUnitsStyleBrief;
  formatter.allowedUnits = NSCalendarUnitNanosecond | NSCalendarUnitSecond |
                           NSCalendarUnitMinute | NSCalendarUnitHour;
  return [formatter stringFromTimeInterval:timeSinceReindexing];
}
@end
