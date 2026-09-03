// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activity_service_mediator.h"

#import <MobileCoreServices/MobileCoreServices.h>

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_generation_commands.h"
#import "ios/chrome/browser/shared/public/commands/send_tab_to_self_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/bookmark_activity.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/chrome_activity.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/copy_activity.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/find_in_page_activity.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/generate_qr_code_activity.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/print_activity.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/reading_list_activity.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/request_desktop_or_mobile_site_activity.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/send_tab_to_self_activity.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activity_service_histograms.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activity_type_util.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/chrome_activity_file_source.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/chrome_activity_image_source.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/chrome_activity_item_source.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/chrome_activity_text_source.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/chrome_activity_url_source.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/share_file_data.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/share_image_data.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/share_to_data.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"

@interface ActivityServiceMediator () {
  // The custom activities created by the mediator.
  NSMutableArray<ChromeActivity*>* _activities;
  // The user's given name used to format target device titles in Share Sheet.
  NSString* _userGivenName;
}

@property(nonatomic, weak) id<BrowserCoordinatorCommands> browserHandler;

@property(nonatomic, weak) id<FindInPageCommands> findInPageHandler;

@property(nonatomic, weak) id<SendTabToSelfCommands> sendTabToSelfHandler;

@property(nonatomic, weak) id<BookmarksCommands> bookmarksHandler;

@property(nonatomic, weak) id<HelpCommands> helpHandler;

@property(nonatomic, weak) id<QRGenerationCommands> qrGenerationHandler;

@property(nonatomic, assign) PrefService* prefService;

@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;

@property(nonatomic, weak) UIViewController* baseViewController;

// The navigation agent.
@property(nonatomic, readonly) WebNavigationBrowserAgent* navigationAgent;

@property(nonatomic, readonly) ReadingListBrowserAgent* readingListBrowserAgent;

@property(nonatomic, assign)
    send_tab_to_self::SendTabToSelfSyncService* sendTabToSelfSyncService;

@end

@implementation ActivityServiceMediator

#pragma mark - Public

- (instancetype)
      initWithBrowserHandler:(id<BrowserCoordinatorCommands>)browserHandler
           findInPageHandler:(id<FindInPageCommands>)findInPageHandler
        sendTabToSelfHandler:(id<SendTabToSelfCommands>)sendTabToSelfHandler
            bookmarksHandler:(id<BookmarksCommands>)bookmarksHandler
                 helpHandler:(id<HelpCommands>)helpHandler
         qrGenerationHandler:(id<QRGenerationCommands>)qrGenerationHandler
                 prefService:(PrefService*)prefService
               bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
          baseViewController:(UIViewController*)baseViewController
             navigationAgent:(WebNavigationBrowserAgent*)navigationAgent
     readingListBrowserAgent:(ReadingListBrowserAgent*)readingListBrowserAgent
    sendTabToSelfSyncService:
        (send_tab_to_self::SendTabToSelfSyncService*)sendTabToSelfSyncService
               userGivenName:(NSString*)userGivenName {
  if ((self = [super init])) {
    _browserHandler = browserHandler;
    _findInPageHandler = findInPageHandler;
    _sendTabToSelfHandler = sendTabToSelfHandler;
    _bookmarksHandler = bookmarksHandler;
    _helpHandler = helpHandler;
    _qrGenerationHandler = qrGenerationHandler;
    _prefService = prefService;
    _bookmarkModel = bookmarkModel;
    _baseViewController = baseViewController;
    _navigationAgent = navigationAgent;
    _readingListBrowserAgent = readingListBrowserAgent;
    _sendTabToSelfSyncService = sendTabToSelfSyncService;
    _userGivenName = userGivenName;
    _activities = [[NSMutableArray alloc] init];
  }
  return self;
}

- (NSArray<id<ChromeActivityItemSource>>*)activityItemsForDataItems:
    (NSArray<ShareToData*>*)dataItems {
  NSMutableArray* items = [[NSMutableArray alloc] init];

  // The `additionalText` is not added when sharing multiple URLs since items
  // are not associated with each other and the `additionalText` is not likely
  // be meaningful without the context of the page it came from.
  if (dataItems.count == 1 && dataItems.firstObject.additionalText) {
    [items addObject:[[ChromeActivityTextSource alloc]
                         initWithText:dataItems.firstObject.additionalText]];
  }

  for (ShareToData* data in dataItems) {
    ChromeActivityURLSource* activityURLSource =
        [[ChromeActivityURLSource alloc] initWithShareURL:data.shareNSURL
                                                  subject:data.title];
    activityURLSource.thumbnail = data.thumbnail;
    activityURLSource.linkMetadata = data.linkMetadata;
    [items addObject:activityURLSource];
  }

  return items;
}

- (NSArray*)applicationActivitiesForDataItems:
    (NSArray<ShareToData*>*)dataItems {
  NSMutableArray* applicationActivities = [NSMutableArray array];

  [applicationActivities
      addObject:[[CopyActivity alloc] initWithDataItems:dataItems]];

  if (dataItems.count != 1) {
    return applicationActivities;
  }

  // The following acitivites only support a single item.
  ShareToData* data = dataItems.firstObject;

  if (data.shareURL.SchemeIsHTTPOrHTTPS()) {
    NSArray<UIActivity*>* sendTabToSelfActivities = [SendTabToSelfActivity
        sendTabToSelfActivitiesForData:data
                           syncService:self.sendTabToSelfSyncService
                               handler:self.sendTabToSelfHandler
                         userGivenName:_userGivenName];
    [applicationActivities addObjectsFromArray:sendTabToSelfActivities];

    ReadingListActivity* readingListActivity =
        [[ReadingListActivity alloc] initWithURL:data.shareURL
                                           title:data.title
                         readingListBrowserAgent:self.readingListBrowserAgent];
    [applicationActivities addObject:readingListActivity];

    BookmarkActivity* bookmarkActivity =
        [[BookmarkActivity alloc] initWithURL:data.visibleURL
                                        title:data.title
                                bookmarkModel:self.bookmarkModel
                                      handler:self.bookmarksHandler
                                  prefService:self.prefService];
    [applicationActivities addObject:bookmarkActivity];

    GenerateQrCodeActivity* generateQrCodeActivity =
        [[GenerateQrCodeActivity alloc] initWithURL:data.shareURL
                                              title:data.title
                                            handler:self.qrGenerationHandler];
    [applicationActivities addObject:generateQrCodeActivity];

    FindInPageActivity* findInPageActivity =
        [[FindInPageActivity alloc] initWithData:data
                                         handler:self.findInPageHandler];
    [applicationActivities addObject:findInPageActivity];

    RequestDesktopOrMobileSiteActivity* requestActivity =
        [[RequestDesktopOrMobileSiteActivity alloc]
            initWithUserAgent:data.userAgent
                  helpHandler:self.helpHandler
              navigationAgent:self.navigationAgent];
    [applicationActivities addObject:requestActivity];
  } else if (UrlIsDownloadedFile(data.shareURL) ||
             UrlIsExternalFileReference(data.shareURL)) {
    FindInPageActivity* findInPageActivity =
        [[FindInPageActivity alloc] initWithData:data
                                         handler:self.findInPageHandler];
    [applicationActivities addObject:findInPageActivity];
  }

  if (self.prefService->GetBoolean(prefs::kPrintingEnabled)) {
    PrintActivity* printActivity =
        [[PrintActivity alloc] initWithData:data
                                    handler:self.browserHandler
                         baseViewController:self.baseViewController];
    [applicationActivities addObject:printActivity];
  }

  [_activities addObjectsFromArray:applicationActivities];
  return applicationActivities;
}

- (NSArray<ChromeActivityFileSource*>*)activityItemsForFileData:
    (ShareFileData*)data {
  return @[ [[ChromeActivityFileSource alloc] initWithFilePath:data.filePath] ];
}

- (NSArray<ChromeActivityImageSource*>*)activityItemsForImageData:
    (ShareImageData*)data {
  return @[ [[ChromeActivityImageSource alloc] initWithImage:data.image
                                                       title:data.title] ];
}

- (NSArray*)applicationActivitiesForImageData:(ShareImageData*)data {
  // For images, we only customize the print activity. Other activities use
  // the native ones.
  PrintActivity* printActivity =
      [[PrintActivity alloc] initWithImageData:data
                                       handler:self.browserHandler
                            baseViewController:self.baseViewController];

  [_activities addObject:printActivity];
  return @[ printActivity ];
}

- (NSSet*)excludedActivityTypesForItems:
    (NSArray<id<ChromeActivityItemSource>>*)items {
  NSMutableSet* mutableSet = [[NSMutableSet alloc] init];
  for (id<ChromeActivityItemSource> item in items) {
    [mutableSet unionSet:item.excludedActivityTypes];
  }
  return mutableSet;
}

- (void)shareStartedWithScenario:(SharingScenario)scenario {
  RecordScenarioInitiated(scenario);
}

- (void)recordShareChromeFinishedInPrefs {
  PrefService* prefs = self.prefService;
  DCHECK(prefs);
  int count = prefs->GetInteger(prefs::kIosShareChromeCount);
  prefs->SetInteger(prefs::kIosShareChromeCount, count + 1);
  prefs->SetTime(prefs::kIosShareChromeLastShare, base::Time::Now());
}

- (void)shareFinishedWithScenario:(SharingScenario)scenario
                     activityType:(NSString*)activityType
                        completed:(BOOL)completed {
  if (activityType && completed) {
    activity_type_util::ActivityType type =
        activity_type_util::TypeFromString(activityType);
    activity_type_util::RecordMetricForActivity(type);
    RecordActivityForScenario(type, scenario);
    [self.promoScheduler logUserFinishedActivityFlow];
    if (SharingScenario::ShareChrome == scenario) {
      [self recordShareChromeFinishedInPrefs];
    }
  } else {
    // Share action was cancelled.
    base::RecordAction(base::UserMetricsAction("MobileShareMenuCancel"));
    RecordCancelledScenario(scenario);
  }
}

- (void)disconnect {
  for (ChromeActivity* activity in _activities) {
    [activity disconnect];
  }
  [_activities removeAllObjects];
}

@end
