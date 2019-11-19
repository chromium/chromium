// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"

#include "base/mac/foundation_util.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/favicon/large_icon_cache.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/voice/voice_search_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsCoordinator ()<
    ContentSuggestionsViewControllerAudience,
    OverscrollActionsControllerDelegate>

@property(nonatomic, strong)
    ContentSuggestionsViewController* suggestionsViewController;
@property(nonatomic, strong)
    ContentSuggestionsMediator* contentSuggestionsMediator;
@property(nonatomic, strong)
    ContentSuggestionsHeaderSynchronizer* headerCollectionInteractionHandler;
@property(nonatomic, strong) ContentSuggestionsMetricsRecorder* metricsRecorder;
@property(nonatomic, strong) NTPHomeMediator* NTPMediator;

// Redefined as readwrite.
@property(nonatomic, strong, readwrite)
    ContentSuggestionsHeaderViewController* headerController;

@end

@implementation ContentSuggestionsCoordinator

@synthesize browserState = _browserState;

- (void)start {
  if (self.visible || !self.browserState) {
    // Prevent this coordinator from being started twice in a row or without a
    // browser state.
    return;
  }

  _visible = YES;

  ntp_snippets::ContentSuggestionsService* contentSuggestionsService =
      IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
          self.browserState);
  contentSuggestionsService->remote_suggestions_scheduler()
      ->OnSuggestionsSurfaceOpened();
  contentSuggestionsService->user_classifier()->OnEvent(
      ntp_snippets::UserClassifier::Metric::NTP_OPENED);
  contentSuggestionsService->user_classifier()->OnEvent(
      ntp_snippets::UserClassifier::Metric::SUGGESTIONS_SHOWN);
  PrefService* prefs =
      ios::ChromeBrowserState::FromBrowserState(self.browserState)->GetPrefs();
  bool contentSuggestionsEnabled =
      prefs->GetBoolean(prefs::kArticlesForYouEnabled);
  bool contentSuggestionsVisible =
      prefs->GetBoolean(ntp_snippets::prefs::kArticlesListVisible);
  if (contentSuggestionsEnabled) {
    if (contentSuggestionsVisible) {
      ntp_home::RecordNTPImpression(ntp_home::REMOTE_SUGGESTIONS);
    } else {
      ntp_home::RecordNTPImpression(ntp_home::REMOTE_COLLAPSED);
    }
  } else {
    ntp_home::RecordNTPImpression(ntp_home::LOCAL_SUGGESTIONS);
  }

  UrlLoadingService* urlLoadingService =
      UrlLoadingServiceFactory::GetForBrowserState(self.browserState);

  self.NTPMediator = [[NTPHomeMediator alloc]
        initWithWebState:self.webState
      templateURLService:ios::TemplateURLServiceFactory::GetForBrowserState(
                             self.browserState)
       urlLoadingService:urlLoadingService
             authService:AuthenticationServiceFactory::GetForBrowserState(
                             self.browserState)
         identityManager:IdentityManagerFactory::GetForBrowserState(
                             self.browserState)
              logoVendor:ios::GetChromeBrowserProvider()->CreateLogoVendor(
                             self.browserState)];

  BOOL voiceSearchEnabled = ios::GetChromeBrowserProvider()
                                ->GetVoiceSearchProvider()
                                ->IsVoiceSearchEnabled();
  self.headerController = [[ContentSuggestionsHeaderViewController alloc]
      initWithVoiceSearchEnabled:voiceSearchEnabled];
  self.headerController.dispatcher = self.dispatcher;
  self.headerController.commandHandler = self.NTPMediator;
  self.headerController.delegate = self.NTPMediator;
  self.headerController.readingListModel =
      ReadingListModelFactory::GetForBrowserState(self.browserState);
  self.headerController.toolbarDelegate = self.toolbarDelegate;

  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(self.browserState);
  LargeIconCache* cache =
      IOSChromeLargeIconCacheFactory::GetForBrowserState(self.browserState);
  std::unique_ptr<ntp_tiles::MostVisitedSites> mostVisitedFactory =
      IOSMostVisitedSitesFactory::NewForBrowserState(self.browserState);
  ReadingListModel* readingListModel =
      ReadingListModelFactory::GetForBrowserState(self.browserState);
  self.contentSuggestionsMediator = [[ContentSuggestionsMediator alloc]
      initWithContentService:contentSuggestionsService
            largeIconService:largeIconService
              largeIconCache:cache
             mostVisitedSite:std::move(mostVisitedFactory)
            readingListModel:readingListModel
                 prefService:prefs];
  self.contentSuggestionsMediator.commandHandler = self.NTPMediator;
  self.contentSuggestionsMediator.headerProvider = self.headerController;
  self.contentSuggestionsMediator.contentArticlesExpanded =
      [[PrefBackedBoolean alloc]
          initWithPrefService:prefs
                     prefName:ntp_snippets::prefs::kArticlesListVisible];

  self.headerController.promoCanShow =
      [self.contentSuggestionsMediator notificationPromo]->CanShow();

  self.metricsRecorder = [[ContentSuggestionsMetricsRecorder alloc] init];
  self.metricsRecorder.delegate = self.contentSuggestionsMediator;

  self.suggestionsViewController = [[ContentSuggestionsViewController alloc]
      initWithStyle:CollectionViewControllerStyleDefault];
  [self.suggestionsViewController
      setDataSource:self.contentSuggestionsMediator];
  self.suggestionsViewController.suggestionCommandHandler = self.NTPMediator;
  self.suggestionsViewController.audience = self;
  self.suggestionsViewController.overscrollDelegate = self;
  self.suggestionsViewController.metricsRecorder = self.metricsRecorder;
  self.suggestionsViewController.dispatcher = self.dispatcher;

  self.NTPMediator.consumer = self.headerController;
  self.NTPMediator.dispatcher = self.dispatcher;
  self.NTPMediator.NTPMetrics =
      [[NTPHomeMetrics alloc] initWithBrowserState:self.browserState];
  self.NTPMediator.metricsRecorder = self.metricsRecorder;
  self.NTPMediator.suggestionsViewController = self.suggestionsViewController;
  self.NTPMediator.suggestionsMediator = self.contentSuggestionsMediator;
  self.NTPMediator.suggestionsService = contentSuggestionsService;
  [self.NTPMediator setUp];

  [self.suggestionsViewController addChildViewController:self.headerController];
  [self.headerController
      didMoveToParentViewController:self.suggestionsViewController];

  self.headerCollectionInteractionHandler =
      [[ContentSuggestionsHeaderSynchronizer alloc]
          initWithCollectionController:self.suggestionsViewController
                      headerController:self.headerController];
  self.NTPMediator.headerCollectionInteractionHandler =
      self.headerCollectionInteractionHandler;
}

- (void)stop {
  [self.NTPMediator shutdown];
  self.NTPMediator = nil;
  self.contentSuggestionsMediator = nil;
  self.headerController = nil;
  _visible = NO;
}

- (UIViewController*)viewController {
  return self.suggestionsViewController;
}

#pragma mark - ContentSuggestionsViewControllerAudience

- (void)promoShown {
  NotificationPromoWhatsNew* notificationPromo =
      [self.contentSuggestionsMediator notificationPromo];
  notificationPromo->HandleViewed();
  [self.headerController setPromoCanShow:notificationPromo->CanShow()];
}

#pragma mark - OverscrollActionsControllerDelegate

- (void)overscrollActionsController:(OverscrollActionsController*)controller
                   didTriggerAction:(OverscrollAction)action {
  switch (action) {
    case OverscrollAction::NEW_TAB: {
      [_dispatcher openURLInNewTab:[OpenNewTabCommand command]];
    } break;
    case OverscrollAction::CLOSE_TAB: {
      [_dispatcher closeCurrentTab];
    } break;
    case OverscrollAction::REFRESH:
      [self reload];
      break;
    case OverscrollAction::NONE:
      NOTREACHED();
      break;
  }
}

- (BOOL)shouldAllowOverscrollActions {
  return YES;
}

- (UIView*)toolbarSnapshotView {
  return
      [[self.headerController toolBarView] snapshotViewAfterScreenUpdates:NO];
}

- (UIView*)headerView {
  return self.suggestionsViewController.view;
}

- (CGFloat)overscrollActionsControllerHeaderInset:
    (OverscrollActionsController*)controller {
  return 0;
}

- (CGFloat)overscrollHeaderHeight {
  CGFloat height = [self.headerController toolBarView].bounds.size.height;
  CGFloat topInset = self.suggestionsViewController.view.safeAreaInsets.top;
  return height + topInset;
}

#pragma mark - Public methods

- (UIView*)view {
  return self.suggestionsViewController.view;
}

- (void)dismissModals {
  [self.NTPMediator dismissModals];
}

- (UIEdgeInsets)contentInset {
  return self.suggestionsViewController.collectionView.contentInset;
}

- (CGPoint)contentOffset {
  CGPoint collectionOffset =
      self.suggestionsViewController.collectionView.contentOffset;
  collectionOffset.y -=
      self.headerCollectionInteractionHandler.collectionShiftingOffset;
  return collectionOffset;
}

- (void)willUpdateSnapshot {
  [self.suggestionsViewController clearOverscroll];
}

- (void)reload {
  [self.contentSuggestionsMediator.dataSink reloadAllData];
}

@end
