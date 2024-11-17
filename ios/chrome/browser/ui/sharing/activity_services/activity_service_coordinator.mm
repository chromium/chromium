// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activity_service_coordinator.h"

#import <LinkPresentation/LinkPresentation.h>

#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activity_service_mediator.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activity_service_presentation.h"
#import "ios/chrome/browser/ui/sharing/activity_services/canonical_url_retriever.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_file_source.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_image_source.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_item_source.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_text_source.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_url_source.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_file_data.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_image_data.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data_builder.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/sharing/sharing_positioner.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// MIME type of PDF.
const char kMimeTypePDF[] = "application/pdf";

// Point size to use when getting custom symbol for app icon.
constexpr CGFloat kAppIconPointSize = 80;

}  // namespace

@interface ActivityServiceCoordinator ()

@property(nonatomic, weak) id<BrowserCoordinatorCommands, FindInPageCommands>
    handler;

@property(nonatomic, strong) ActivityServiceMediator* mediator;

@property(nonatomic, strong) UIActivityViewController* viewController;

// Parameters determining the activity flow and values.
@property(nonatomic, strong) SharingParams* params;

// Whether the coordinator is linked to an incognito browser.
@property(nonatomic, assign) BOOL incognito;

@end

@implementation ActivityServiceCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                    params:(SharingParams*)params {
  DCHECK(params);
  if ((self = [super initWithBaseViewController:baseViewController
                                        browser:browser])) {
    _params = params;
  }
  return self;
}

#pragma mark - Public methods

- (void)start {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(applicationDidEnterBackground:)
                        name:UIApplicationDidEnterBackgroundNotification
                      object:nil];

  self.handler =
      static_cast<id<BrowserCoordinatorCommands, FindInPageCommands>>(
          self.browser->GetCommandDispatcher());

  ProfileIOS* profile = self.browser->GetProfile();
  self.incognito = profile->IsOffTheRecord();
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForProfile(profile);
  id<BookmarksCommands> bookmarksHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BookmarksCommands);
  id<HelpCommands> helpHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), HelpCommands);
  WebNavigationBrowserAgent* agent =
      WebNavigationBrowserAgent::FromBrowser(self.browser);
  ReadingListBrowserAgent* readingListBrowserAgent =
      ReadingListBrowserAgent::FromBrowser(self.browser);
  self.mediator =
      [[ActivityServiceMediator alloc] initWithHandler:self.handler
                                      bookmarksHandler:bookmarksHandler
                                           helpHandler:helpHandler
                                   qrGenerationHandler:self.scopedHandler
                                           prefService:profile->GetPrefs()
                                         bookmarkModel:bookmarkModel
                                    baseViewController:self.baseViewController
                                       navigationAgent:agent
                               readingListBrowserAgent:readingListBrowserAgent];

  SceneState* sceneState = self.browser->GetSceneState();
  self.mediator.promoScheduler = [NonModalDefaultBrowserPromoSchedulerSceneAgent
      agentFromScene:sceneState];

  [self.mediator shareStartedWithScenario:self.params.scenario];

  // Image item
  if (self.params.image) {
    [self shareImage];
    return;
  }

  if (self.params.filePath &&
      [[NSFileManager defaultManager]
          isReadableFileAtPath:self.params.filePath.path]) {
    [self shareFile];
    return;
  }

  if (self.params.URLs.count > 0) {
    // If at least one valid URL is found, share the URLs in `_params`.
    for (URLWithTitle* urlWithTitle in self.params.URLs) {
      if (!urlWithTitle.URL.is_empty()) {
        [self shareURLs];
        return;
      }
    }
  }

  // Default to sharing the current page
  [self shareCurrentPage];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;

  self.mediator = nil;
}

#pragma mark - Private Methods

// Sets up the activity ViewController with the given `items` and `activities`.
- (void)shareItems:(NSArray<id<ChromeActivityItemSource>>*)items
        activities:(NSArray*)activities
         extraItem:(id)extraItem {
  NSArray* itemsToShare = items;
  if (extraItem) {
    NSMutableArray* itemsWithExtra = [items mutableCopy];
    [itemsWithExtra addObject:extraItem];
    itemsToShare = itemsWithExtra;
  }
  self.viewController =
      [[UIActivityViewController alloc] initWithActivityItems:itemsToShare
                                        applicationActivities:activities];

  [self.viewController setModalPresentationStyle:UIModalPresentationPopover];

  NSSet* excludedActivityTypes =
      [self.mediator excludedActivityTypesForItems:items];
  [self.viewController
      setExcludedActivityTypes:[excludedActivityTypes allObjects]];

  // Set-up popover positioning (for iPad).
  DCHECK(self.positionProvider);
  if ([self.positionProvider respondsToSelector:@selector(barButtonItem)] &&
      self.positionProvider.barButtonItem) {
    self.viewController.popoverPresentationController.barButtonItem =
        self.positionProvider.barButtonItem;
  } else {
    self.viewController.popoverPresentationController.sourceView =
        self.positionProvider.sourceView;
    self.viewController.popoverPresentationController.sourceRect =
        self.positionProvider.sourceRect;
  }

  // Set completion callback.
  __weak __typeof(self) weakSelf = self;
  [self.viewController setCompletionWithItemsHandler:^(
                           NSString* activityType, BOOL completed,
                           NSArray* returnedItems, NSError* activityError) {
    ActivityServiceCoordinator* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    // Delegate post-activity processing to the mediator.
    [strongSelf.mediator shareFinishedWithScenario:strongSelf.params.scenario
                                      activityType:activityType
                                         completed:completed];

    // If it is completed by finishing a scenario or if the view been closed by
    // the user without selecting a service.
    BOOL isActivityViewControllerDismissed =
        completed || (!activityType && !completed);
    if (isActivityViewControllerDismissed) {
      // Signal the presentation provider that our scenario is over.
      [strongSelf.presentationProvider activityServiceDidEndPresenting];
    }
  }];

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - Private Methods: Current Page

// Fetches the current tab's URL, configures activities and items, and shows
// an activity view.
- (void)shareCurrentPage {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();

  // In some cases it seems that the share sheet is triggered while no tab is
  // present (probably due to a timing issue).
  if (!currentWebState) {
    return;
  }

  // Retrieve the current page's URL.
  __weak __typeof(self) weakSelf = self;
  activity_services::RetrieveCanonicalUrl(
      currentWebState,
      base::BindOnce(
          ^(base::WeakPtr<web::WebState> weak_web_state, const GURL& url) {
            [weakSelf sharePageWithCanonicalURL:url
                                       webState:weak_web_state.get()];
          },
          currentWebState->GetWeakPtr()));
}

// Shares the current page using its `canonicalURL`.
- (void)sharePageWithCanonicalURL:(const GURL&)canonicalURL
                         webState:(web::WebState*)webState {
  if (!webState) {
    return;
  }

  if (webState != self.browser->GetWebStateList()->GetActiveWebState()) {
    return;
  }

  ShareToData* data =
      activity_services::ShareToDataForWebState(webState, canonicalURL);

  NSArray<ChromeActivityURLSource*>* items =
      [self.mediator activityItemsForDataItems:@[ data ]];
  NSArray* activities =
      [self.mediator applicationActivitiesForDataItems:@[ data ]];

  id extraItem = nil;
  if (@available(iOS 16.4, *)) {
    extraItem = webState->GetActivityItem();
  }
  [self shareItems:items activities:activities extraItem:extraItem];
}

#pragma mark - Private Methods: Share Image

// Configures activities and items for an image and its title, and shows
// an activity view.
- (void)shareImage {
  ShareImageData* data =
      [[ShareImageData alloc] initWithImage:self.params.image
                                      title:self.params.imageTitle];

  NSArray<ChromeActivityImageSource*>* items =
      [self.mediator activityItemsForImageData:data];
  NSArray* activities = [self.mediator applicationActivitiesForImageData:data];

  [self shareItems:items activities:activities extraItem:nil];
}

#pragma mark - Private Methods: Share File

// Configures activities and items, and shows an activity view.
- (void)shareFile {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();

  // In some cases it seems that the share sheet is triggered while no tab is
  // present (probably due to a timing issue).
  if (!currentWebState) {
    return;
  }

  // Retrieve the current page's URL.
  __weak __typeof(self) weakSelf = self;
  activity_services::RetrieveCanonicalUrl(
      currentWebState,
      base::BindOnce(
          ^(base::WeakPtr<web::WebState> weak_web_state, const GURL& url) {
            [weakSelf shareFileWithCanonicalURL:url
                                       webState:weak_web_state.get()];
          },
          currentWebState->GetWeakPtr()));
}

// Shares the current PDF using its `canonicalURL`.
- (void)shareFileWithCanonicalURL:(const GURL&)canonicalURL
                         webState:(web::WebState*)webState {
  if (!webState) {
    return;
  }

  if (webState != self.browser->GetWebStateList()->GetActiveWebState()) {
    return;
  }

  ShareToData* URLData =
      activity_services::ShareToDataForWebState(webState, canonicalURL);

  // As giving a PDF file to the UIActivityViewController will add the "Print"
  // activity from Apple, Chrome's print activity is disabled to avoid
  // duplicate.
  const BOOL isPDF = webState->GetContentsMimeType() == kMimeTypePDF;
  if (isPDF) {
    URLData.isPagePrintable = NO;
  }

  ShareFileData* fileData =
      [[ShareFileData alloc] initWithFilePath:self.params.filePath];

  NSArray<ChromeActivityFileSource*>* items =
      [self.mediator activityItemsForFileData:fileData];
  NSArray* activities =
      [self.mediator applicationActivitiesForDataItems:@[ URLData ]];
  id extraItem = nil;
  if (@available(iOS 16.4, *)) {
    extraItem = webState->GetActivityItem();
  }
  [self shareItems:items activities:activities extraItem:extraItem];
}

#pragma mark - Private Methods: Share URL

// Configures activities and items for a URL and its title, and shows
// an activity view. Also adds another activity item for additional text, if
// there is any.
- (void)shareURLs {
  NSMutableArray* dataItems = [[NSMutableArray alloc] init];
  SharingParams* params = self.params;

  // If only given a single URL, include additionalText in shared payload.
  if (params.URLs.count == 1) {
    URLWithTitle* url = params.URLs[0];
    LPLinkMetadata* metadata = [self linkMetadata:url];
    ShareToData* data = activity_services::ShareToDataForURL(
        url.URL, url.title, params.additionalText, metadata);
    [dataItems addObject:data];
  } else {
    for (URLWithTitle* urlWithTitle in params.URLs) {
      ShareToData* data =
          activity_services::ShareToDataForURLWithTitle(urlWithTitle);
      [dataItems addObject:data];
    }
  }

  NSArray<id<ChromeActivityItemSource>>* items =
      [self.mediator activityItemsForDataItems:dataItems];
  NSArray* activities =
      [self.mediator applicationActivitiesForDataItems:dataItems];

  [self shareItems:items activities:activities extraItem:nil];
}

// Returns some basic metadata for the Chrome App's app store link. If we do
// not supply this metadata, UIActivityViewController will only display a
// generic website icon and the hostname when given an app store link.
- (LPLinkMetadata*)linkMetadata:(URLWithTitle*)url {
  if (self.params.scenario != SharingScenario::ShareChrome) {
    // For non app store links, we will allow UIActivityViewController to choose
    // how to display.
    return nil;
  }

  LPLinkMetadata* metadata = [[LPLinkMetadata alloc] init];
  metadata.originalURL = net::NSURLWithGURL(url.URL);
  metadata.title = url.title;
  metadata.iconProvider = [self appIconProvider];
  return metadata;
}

- (NSItemProvider*)appIconProvider {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* image = MakeSymbolMulticolor(CustomSymbolWithPointSize(
      kMulticolorChromeballSymbol, kAppIconPointSize));
#else
  UIImage* image = DefaultSymbolTemplateWithPointSize(kDefaultBrowserSymbol,
                                                      kAppIconPointSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return [[NSItemProvider alloc] initWithObject:image];
}

#pragma mark - Notification callback

- (void)applicationDidEnterBackground:(NSNotification*)note {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

@end
