// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_coordinator.h"

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/feature_list.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/page_info/core/page_info_action.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator_delegate.h"
#import "ios/chrome/browser/page_info/about_this_site_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#import "ios/chrome/browser/ui/page_info/page_info_about_this_site_mediator.h"
#import "ios/chrome/browser/ui/page_info/page_info_history_mediator.h"
#import "ios/chrome/browser/ui/page_info/page_info_last_visited_coordinator.h"
#import "ios/chrome/browser/ui/page_info/page_info_permissions_mediator.h"
#import "ios/chrome/browser/ui/page_info/page_info_security_coordinator.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_mediator.h"
#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

@interface PageInfoCoordinator () <PageInfoPresentationCommands,
                                   HistoryCoordinatorDelegate>

@property(nonatomic, strong) UINavigationController* navigationController;
@property(nonatomic, strong) CommandDispatcher* dispatcher;
@property(nonatomic, strong) PageInfoViewController* viewController;
@property(nonatomic, strong) PageInfoPermissionsMediator* permissionsMediator;
@property(nonatomic, strong)
    PageInfoLastVisitedCoordinator* lastVisitedCoordinator;

@end

@implementation PageInfoCoordinator {
  // Coordinator for the security screen.
  PageInfoSecurityCoordinator* _securityCoordinator;
  PageInfoAboutThisSiteMediator* _aboutThisSiteMediator;
  PageInfoSiteSecurityDescription* _siteSecurityDescription;

  // Mediator for the Last Visited feature.
  PageInfoHistoryMediator* _pageInfoHistoryMediator;
}

@synthesize presentationProvider = _presentationProvider;

#pragma mark - ChromeCoordinator

- (void)start {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();

  _siteSecurityDescription =
      [PageInfoSiteSecurityMediator configurationForWebState:webState];

  self.viewController = [[PageInfoViewController alloc]
      initWithSiteSecurityDescription:_siteSecurityDescription];

  self.viewController.pageInfoPresentationHandler = self;

  self.navigationController =
      IsRevampPageInfoIosEnabled()
          ? [[UINavigationController alloc]
                initWithRootViewController:self.viewController]
          : [[TableViewNavigationController alloc]
                initWithTable:self.viewController];
  self.navigationController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  self.navigationController.presentationController.delegate =
      self.viewController;

  self.dispatcher = self.browser->GetCommandDispatcher();
  self.viewController.pageInfoCommandsHandler =
      HandlerForProtocol(self.dispatcher, PageInfoCommands);

  self.permissionsMediator =
      [[PageInfoPermissionsMediator alloc] initWithWebState:webState];
  self.viewController.permissionsDelegate = self.permissionsMediator;
  self.permissionsMediator.consumer = self.viewController;

  if (IsAboutThisSiteFeatureEnabled()) {
    page_info::AboutThisSiteService* service =
        AboutThisSiteServiceFactory::GetForProfile(self.browser->GetProfile());
    _aboutThisSiteMediator =
        [[PageInfoAboutThisSiteMediator alloc] initWithWebState:webState
                                                        service:service];
    _aboutThisSiteMediator.consumer = self.viewController;
  }

  if (base::FeatureList::IsEnabled(
          feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature)) {
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(
            self.browser->GetProfile());
    tracker->NotifyEvent(
        feature_engagement::events::kEnhancedSafeBrowsingPromoCriterionMet);
  }

  const bool isIncognito = self.browser->GetProfile()->IsOffTheRecord();

  // Create the PageInfoHistoryMediator only if kPageInfoLastVisitedIOS is
  // enabled, the browser is not in incognito mode and the page is neither
  // offline nor a chrome page.
  if (IsPageInfoLastVisitedIOSEnabled() && !isIncognito &&
      !_siteSecurityDescription.isEmpty) {
    history::HistoryService* historyService =
        ios::HistoryServiceFactory::GetForProfile(
            self.browser->GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);

    const GURL& siteURL =
        webState->GetNavigationManager()->GetVisibleItem()->GetURL();

    _pageInfoHistoryMediator =
        [[PageInfoHistoryMediator alloc] initWithHistoryService:historyService
                                                        siteURL:siteURL];

    _pageInfoHistoryMediator.consumer = self.viewController;
  }

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];

  base::RecordAction(base::UserMetricsAction("PageInfo.Opened"));
  base::UmaHistogramEnumeration(page_info::kWebsiteSettingsActionHistogram,
                                page_info::PAGE_INFO_OPENED);
}

- (void)stop {
  [self.permissionsMediator disconnect];
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  [self.dispatcher stopDispatchingToTarget:self];
  self.navigationController = nil;
  self.viewController = nil;

  if (IsPageInfoLastVisitedIOSEnabled()) {
    [_pageInfoHistoryMediator disconnect];
    _pageInfoHistoryMediator = nil;
  }

  [_securityCoordinator stop];
  _securityCoordinator.pageInfoPresentationHandler = nil;
  _securityCoordinator = nil;

  [self.lastVisitedCoordinator stop];
  self.lastVisitedCoordinator = nil;

  base::RecordAction(base::UserMetricsAction("PageInfo.Closed"));
}

#pragma mark - PageInfoPresentationCommands

- (void)showSecurityPage {
  base::RecordAction(base::UserMetricsAction("PageInfo.Security.Opened"));
  base::UmaHistogramEnumeration(page_info::kWebsiteSettingsActionHistogram,
                                page_info::PAGE_INFO_SECURITY_DETAILS_OPENED);

  _securityCoordinator = [[PageInfoSecurityCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:self.browser
               siteSecurityDescription:_siteSecurityDescription];
  _securityCoordinator.pageInfoPresentationHandler = self;
  [_securityCoordinator start];
}

- (void)showSecurityHelpPage {
  base::RecordAction(
      base::UserMetricsAction("PageInfo.Security.ConnectionHelp.Opened"));
  base::UmaHistogramEnumeration(page_info::kWebsiteSettingsActionHistogram,
                                page_info::PAGE_INFO_CONNECTION_HELP_OPENED);

  UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kPageInfoHelpCenterURL));
  params.in_incognito = self.browser->GetProfile()->IsOffTheRecord();
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  id<PageInfoCommands> pageInfoCommandsHandler =
      HandlerForProtocol(self.dispatcher, PageInfoCommands);
  [pageInfoCommandsHandler hidePageInfo];
}

- (void)showAboutThisSitePage:(GURL)URL {
  CHECK(IsAboutThisSiteFeatureEnabled());
  base::RecordAction(base::UserMetricsAction("PageInfo.AboutThisSite.Opened"));
  base::UmaHistogramEnumeration(
      page_info::kWebsiteSettingsActionHistogram,
      page_info::PAGE_INFO_ABOUT_THIS_SITE_PAGE_OPENED);

  web::NavigationManager::WebLoadParams webParams =
      web::NavigationManager::WebLoadParams(URL);
  bool in_incognito = self.browser->GetProfile()->IsOffTheRecord();

  // Add X-Client-Data header.
  NSMutableDictionary<NSString*, NSString*>* combinedExtraHeaders =
      [web_navigation_util::VariationHeadersForURL(URL, in_incognito)
          mutableCopy];
  [combinedExtraHeaders addEntriesFromDictionary:webParams.extra_headers];
  webParams.extra_headers = [combinedExtraHeaders copy];
  UrlLoadParams params = UrlLoadParams::InNewTab(webParams);
  params.in_incognito = in_incognito;

  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  id<PageInfoCommands> pageInfoCommandsHandler =
      HandlerForProtocol(self.dispatcher, PageInfoCommands);
  [pageInfoCommandsHandler hidePageInfo];
}

- (PageInfoSiteSecurityDescription*)updatedSiteSecurityDescription {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  return [PageInfoSiteSecurityMediator configurationForWebState:webState];
}

- (void)showLastVisitedPage {
  CHECK(IsPageInfoLastVisitedIOSEnabled());
  base::RecordAction(base::UserMetricsAction("PageInfo.History.Opened"));
  base::UmaHistogramEnumeration(page_info::kWebsiteSettingsActionHistogram,
                                page_info::PAGE_INFO_HISTORY_OPENED);
  self.lastVisitedCoordinator = [[PageInfoLastVisitedCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:self.browser
                              hostName:_siteSecurityDescription.siteURL];
  self.lastVisitedCoordinator.delegate = self;
  [self.lastVisitedCoordinator start];
}

#pragma mark - HistoryCoordinatorDelegate

- (void)closeHistoryWithCompletion:(ProceduralBlock)completion {
  if (completion) {
    completion();
  }

  // Stop this coordinator.
  id<PageInfoCommands> pageInfoCommandsHandler =
      HandlerForProtocol(self.dispatcher, PageInfoCommands);
  [pageInfoCommandsHandler hidePageInfo];
}

- (void)closeHistory {
  [self closeHistoryWithCompletion:nil];
}

@end
