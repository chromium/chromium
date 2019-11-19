// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/user_activity_handler.h"

#import <CoreSpotlight/CoreSpotlight.h>
#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/handoff/handoff_utility.h"
#include "components/search_engines/template_url_service.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#include "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#include "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/metrics/first_user_action_recorder.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/u2f/u2f_tab_helper.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/url_loading/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {
// Constants for 3D touch application static shortcuts.
NSString* const kShortcutNewSearch = @"OpenNewSearch";
NSString* const kShortcutNewIncognitoSearch = @"OpenIncognitoSearch";
NSString* const kShortcutVoiceSearch = @"OpenVoiceSearch";
NSString* const kShortcutQRScanner = @"OpenQRScanner";

}  // namespace

@interface UserActivityHandler ()
// Handles the 3D touch application static items. Does nothing if in first run.
+ (BOOL)handleShortcutItem:(UIApplicationShortcutItem*)shortcutItem
        startupInformation:(id<StartupInformation>)startupInformation;
// Routes Universal 2nd Factor (U2F) callback to the correct Tab.
+ (void)routeU2FURL:(const GURL&)URL
    interfaceProvider:(id<BrowserInterfaceProvider>)interfaceProvider;
@end

@implementation UserActivityHandler

#pragma mark - Public methods.

+ (BOOL)continueUserActivity:(NSUserActivity*)userActivity
         applicationIsActive:(BOOL)applicationIsActive
                   tabOpener:(id<TabOpening>)tabOpener
          startupInformation:(id<StartupInformation>)startupInformation {
  NSURL* webpageURL = userActivity.webpageURL;

  if ([userActivity.activityType
          isEqualToString:handoff::kChromeHandoffActivityType]) {
    // App was launched by iOS as a result of Handoff.
    NSString* originString = base::mac::ObjCCast<NSString>(
        userActivity.userInfo[handoff::kOriginKey]);
    handoff::Origin origin = handoff::OriginFromString(originString);
    UMA_HISTOGRAM_ENUMERATION("IOS.Handoff.Origin", origin,
                              handoff::ORIGIN_COUNT);
  } else if ([userActivity.activityType
                 isEqualToString:NSUserActivityTypeBrowsingWeb]) {
    // App was launched as the result of a Universal Link navigation.
    GURL gurl = net::GURLWithNSURL(webpageURL);
    AppStartupParameters* startupParams =
        [[AppStartupParameters alloc] initWithUniversalLink:gurl];
    [startupInformation setStartupParameters:startupParams];
    base::RecordAction(base::UserMetricsAction("IOSLaunchedByUniversalLink"));

    if (startupParams)
      webpageURL = net::NSURLWithGURL([startupParams externalURL]);

    // Don't call continueUserActivityURL if the completePaymentRequest flag
    // is set since the startup parameters need to be handled in
    // -handleStartupParametersWithTabOpener:
    if (startupParams && startupParams.completePaymentRequest)
      return YES;
  } else if (spotlight::IsSpotlightAvailable() &&
             [userActivity.activityType
                 isEqualToString:CSSearchableItemActionType]) {
    // App was launched by iOS as the result of a tap on a Spotlight Search
    // result.
    NSString* itemID =
        [userActivity.userInfo objectForKey:CSSearchableItemActivityIdentifier];
    spotlight::Domain domain = spotlight::SpotlightDomainFromString(itemID);
    UMA_HISTOGRAM_ENUMERATION("IOS.Spotlight.Origin", domain,
                              spotlight::DOMAIN_COUNT);

    if (!itemID) {
      return NO;
    }
    if (domain == spotlight::DOMAIN_ACTIONS) {
      webpageURL =
          [NSURL URLWithString:base::SysUTF8ToNSString(kChromeUINewTabURL)];
      AppStartupParameters* startupParams = [[AppStartupParameters alloc]
          initWithExternalURL:GURL(kChromeUINewTabURL)
                  completeURL:GURL(kChromeUINewTabURL)];
      BOOL startupParamsSet = spotlight::SetStartupParametersForSpotlightAction(
          itemID, startupParams);
      if (!startupParamsSet) {
        return NO;
      }
      [startupInformation setStartupParameters:startupParams];
    } else if (!webpageURL) {
      spotlight::GetURLForSpotlightItemID(itemID, ^(NSURL* contentURL) {
        if (!contentURL) {
          return;
        }
        dispatch_async(dispatch_get_main_queue(), ^{
          // Update the isActive flag as it may have changed during the async
          // calls.
          BOOL isActive = [[UIApplication sharedApplication]
                              applicationState] == UIApplicationStateActive;
          [self continueUserActivityURL:contentURL
                    applicationIsActive:isActive
                              tabOpener:tabOpener
                     startupInformation:startupInformation];
        });
      });
      return YES;
    }
  } else if ([userActivity.activityType
                 isEqualToString:@"SearchInChromeIntent"]) {
    base::RecordAction(UserMetricsAction("IOSLaunchedBySearchInChromeIntent"));
    AppStartupParameters* startupParams = [[AppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
                completeURL:GURL(kChromeUINewTabURL)];
    [startupParams setPostOpeningAction:FOCUS_OMNIBOX];
    [startupInformation setStartupParameters:startupParams];
    return YES;
  } else {
    // Do nothing for unknown activity type.
    return NO;
  }

  return [self continueUserActivityURL:webpageURL
                   applicationIsActive:applicationIsActive
                             tabOpener:tabOpener
                    startupInformation:startupInformation];
}

+ (BOOL)continueUserActivityURL:(NSURL*)webpageURL
            applicationIsActive:(BOOL)applicationIsActive
                      tabOpener:(id<TabOpening>)tabOpener
             startupInformation:(id<StartupInformation>)startupInformation {
  if (!webpageURL)
    return NO;

  GURL webpageGURL(net::GURLWithNSURL(webpageURL));
  if (!webpageGURL.is_valid())
    return NO;

  if (applicationIsActive && ![startupInformation isPresentingFirstRunUI]) {
    // The app is already active so the applicationDidBecomeActive: method will
    // never be called. Open the requested URL immediately.
    ApplicationModeForTabOpening targetMode =
        [[startupInformation startupParameters] launchInIncognito]
            ? ApplicationModeForTabOpening::INCOGNITO
            : ApplicationModeForTabOpening::NORMAL;
    UrlLoadParams params = UrlLoadParams::InNewTab(webpageGURL);
    if (![[startupInformation startupParameters] launchInIncognito] &&
        [tabOpener URLIsOpenedInRegularMode:webpageGURL]) {
      // Record metric.
    }
    [tabOpener dismissModalsAndOpenSelectedTabInMode:targetMode
                                   withUrlLoadParams:params
                                      dismissOmnibox:YES
                                          completion:^{
                                            [startupInformation
                                                setStartupParameters:nil];
                                          }];
    return YES;
  }

  // Don't record the first action as a user action, since it will not be
  // initiated by the user.
  [startupInformation resetFirstUserActionRecorder];

  if (![startupInformation startupParameters]) {
    AppStartupParameters* startupParams =
        [[AppStartupParameters alloc] initWithExternalURL:webpageGURL
                                              completeURL:webpageGURL];
    [startupInformation setStartupParameters:startupParams];
  }
  return YES;
}

+ (void)performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                   completionHandler:(void (^)(BOOL succeeded))completionHandler
                           tabOpener:(id<TabOpening>)tabOpener
                  startupInformation:(id<StartupInformation>)startupInformation
                   interfaceProvider:
                       (id<BrowserInterfaceProvider>)interfaceProvider {
  BOOL handledShortcutItem =
      [UserActivityHandler handleShortcutItem:shortcutItem
                           startupInformation:startupInformation];
  BOOL isActive = [[UIApplication sharedApplication] applicationState] ==
                  UIApplicationStateActive;
  if (handledShortcutItem && isActive) {
    [UserActivityHandler
        handleStartupParametersWithTabOpener:tabOpener
                          startupInformation:startupInformation
                           interfaceProvider:interfaceProvider];
  }
  completionHandler(handledShortcutItem);
}

+ (BOOL)willContinueUserActivityWithType:(NSString*)userActivityType {
  return
      [userActivityType isEqualToString:handoff::kChromeHandoffActivityType] ||
      (spotlight::IsSpotlightAvailable() &&
       [userActivityType isEqualToString:CSSearchableItemActionType]);
}

+ (void)handleStartupParametersWithTabOpener:(id<TabOpening>)tabOpener
                          startupInformation:
                              (id<StartupInformation>)startupInformation
                           interfaceProvider:
                               (id<BrowserInterfaceProvider>)interfaceProvider {
  DCHECK([startupInformation startupParameters]);
  // Do not load the external URL if the user has not accepted the terms of
  // service. This corresponds to the case when the user installed Chrome,
  // has never launched it and attempts to open an external URL in Chrome.
  if ([startupInformation isPresentingFirstRunUI])
    return;

  GURL externalURL = startupInformation.startupParameters.externalURL;
  // Check if it's an U2F call. If so, route it to correct tab.
  // If not, open or reuse tab in main BVC.
  if (U2FTabHelper::IsU2FUrl(externalURL)) {
    [UserActivityHandler routeU2FURL:externalURL
                   interfaceProvider:interfaceProvider];
    // It's OK to clear startup parameters here because routeU2FURL works
    // synchronously.
    [startupInformation setStartupParameters:nil];
  } else {
    // Depending on the startup parameters the user may need to stay on the
    // current tab rather than open a new one in order to complete a Payment
    // Request. This attempts to complete any Payment Request instances on
    // the current tab, and returns if successful.
    if ([tabOpener shouldCompletePaymentRequestOnCurrentTab:startupInformation])
      return;

    // TODO(crbug.com/935019): Exacly the same copy of this code is present in
    // +[URLOpener
    // openURL:applicationActive:options:tabOpener:startupInformation:]

    // The app is already active so the applicationDidBecomeActive: method
    // will never be called. Open the requested URL after all modal UIs have
    // been dismissed. |_startupParameters| must be retained until all deferred
    // modal UIs are dismissed and tab opened with requested URL.
    ApplicationModeForTabOpening targetMode =
        [[startupInformation startupParameters] launchInIncognito]
            ? ApplicationModeForTabOpening::INCOGNITO
            : ApplicationModeForTabOpening::NORMAL;
    GURL URL;
    GURL virtualURL;
    GURL completeURL = startupInformation.startupParameters.completeURL;
    if (completeURL.SchemeIsFile()) {
      // External URL will be loaded by WebState, which expects |completeURL|.
      // Omnibox however suppose to display |externalURL|, which is used as
      // virtual URL.
      URL = completeURL;
      virtualURL = externalURL;
    } else {
      URL = externalURL;
    }
    UrlLoadParams params = UrlLoadParams::InNewTab(URL, virtualURL);

    if (startupInformation.startupParameters.imageSearchData) {
      TemplateURLService* templateURLService =
          ios::TemplateURLServiceFactory::GetForBrowserState(
              interfaceProvider.mainInterface.browserState);

      NSData* imageData = startupInformation.startupParameters.imageSearchData;
      web::NavigationManager::WebLoadParams webLoadParams =
          ImageSearchParamGenerator::LoadParamsForImageData(imageData, GURL(),
                                                            templateURLService);

      params.web_params = webLoadParams;
    } else if (startupInformation.startupParameters.textQuery) {
      NSString* query = startupInformation.startupParameters.textQuery;

      TemplateURLService* templateURLService =
          ios::TemplateURLServiceFactory::GetForBrowserState(
              interfaceProvider.mainInterface.browserState);

      const TemplateURL* defaultURL =
          templateURLService->GetDefaultSearchProvider();
      DCHECK(!defaultURL->url().empty());
      DCHECK(defaultURL->url_ref().IsValid(
          templateURLService->search_terms_data()));
      base::string16 queryString = base::SysNSStringToUTF16(query);
      TemplateURLRef::SearchTermsArgs search_args(queryString);

      GURL result(defaultURL->url_ref().ReplaceSearchTerms(
          search_args, templateURLService->search_terms_data()));
      params.web_params.url = result;
    }

    if (![[startupInformation startupParameters] launchInIncognito] &&
        [tabOpener URLIsOpenedInRegularMode:params.web_params.url]) {
      // Record metric.
    }

    [tabOpener dismissModalsAndOpenSelectedTabInMode:targetMode
                                   withUrlLoadParams:params
                                      dismissOmnibox:[[startupInformation
                                                         startupParameters]
                                                         postOpeningAction] !=
                                                     FOCUS_OMNIBOX
                                          completion:^{
                                            [startupInformation
                                                setStartupParameters:nil];
                                          }];
  }
}

#pragma mark - Internal methods.

+ (BOOL)handleShortcutItem:(UIApplicationShortcutItem*)shortcutItem
        startupInformation:(id<StartupInformation>)startupInformation {
  if ([startupInformation isPresentingFirstRunUI])
    return NO;

  AppStartupParameters* startupParams = [[AppStartupParameters alloc]
      initWithExternalURL:GURL(kChromeUINewTabURL)
              completeURL:GURL(kChromeUINewTabURL)];

  if ([shortcutItem.type isEqualToString:kShortcutNewSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.NewSearchPressed"));
    startupParams.postOpeningAction = FOCUS_OMNIBOX;
    startupInformation.startupParameters = startupParams;
    return YES;

  } else if ([shortcutItem.type isEqualToString:kShortcutNewIncognitoSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.NewIncognitoSearchPressed"));
    startupParams.launchInIncognito = YES;
    startupParams.postOpeningAction = FOCUS_OMNIBOX;
    startupInformation.startupParameters = startupParams;
    return YES;

  } else if ([shortcutItem.type isEqualToString:kShortcutVoiceSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.VoiceSearchPressed"));
    startupParams.postOpeningAction = START_VOICE_SEARCH;
    startupInformation.startupParameters = startupParams;
    return YES;

  } else if ([shortcutItem.type isEqualToString:kShortcutQRScanner]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.ScanQRCodePressed"));
    startupParams.postOpeningAction = START_QR_CODE_SCANNER;
    startupInformation.startupParameters = startupParams;
    return YES;
  }

  NOTREACHED();
  return NO;
}

+ (void)routeU2FURL:(const GURL&)URL
    interfaceProvider:(id<BrowserInterfaceProvider>)interfaceProvider {
  // Retrieve the designated TabID from U2F URL.
  NSString* tabID = U2FTabHelper::GetTabIdFromU2FUrl(URL);
  if (!tabID) {
    return;
  }

  // Iterate through mainTabModel and OTRTabModel to find the corresponding tab.
  NSArray* tabModels = @[
    interfaceProvider.mainInterface.tabModel,
    interfaceProvider.incognitoInterface.tabModel
  ];
  for (TabModel* tabModel in tabModels) {
    WebStateList* webStateList = tabModel.webStateList;
    for (int index = 0; index < webStateList->count(); ++index) {
      web::WebState* webState = webStateList->GetWebStateAt(index);
      NSString* currentTabID = TabIdTabHelper::FromWebState(webState)->tab_id();
      if ([currentTabID isEqualToString:tabID]) {
        U2FTabHelper::FromWebState(webState)->EvaluateU2FResult(URL);
      }
    }
  }
}

@end
