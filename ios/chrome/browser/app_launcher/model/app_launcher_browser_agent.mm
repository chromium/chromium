// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/model/app_launcher_browser_agent.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service_factory.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/app_launcher_overlay.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"
#import "url/gurl.h"

BROWSER_USER_DATA_KEY_IMPL(AppLauncherBrowserAgent)

using app_launcher_overlays::AllowAppLaunchResponse;
using app_launcher_overlays::AppLaunchConfirmationRequest;

namespace {
// Records histogram metric on the user's response when prompted to open another
// application. `user_accepted` should be YES if the user accepted the prompt to
// launch another application. This call is extracted to a separate function to
// reduce macro code expansion.
void RecordAppLaunchRequestMetrics(
    app_launcher_overlays::AppLaunchConfirmationRequestCause cause,
    BOOL user_accepted) {
  switch (cause) {
    case app_launcher_overlays::AppLaunchConfirmationRequestCause::kOther:
      UMA_HISTOGRAM_BOOLEAN("Tab.ExternalApplicationOpened.Generic",
                            user_accepted);
      break;
    case app_launcher_overlays::AppLaunchConfirmationRequestCause::
        kRepeatedRequest:
      UMA_HISTOGRAM_BOOLEAN("Tab.ExternalApplicationOpened.Repeated",
                            user_accepted);
      break;
    case app_launcher_overlays::AppLaunchConfirmationRequestCause::
        kOpenFromIncognito:
      UMA_HISTOGRAM_BOOLEAN("Tab.ExternalApplicationOpened.FromIncognito",
                            user_accepted);
      break;
    case app_launcher_overlays::AppLaunchConfirmationRequestCause::
        kNoUserInteraction:
      UMA_HISTOGRAM_BOOLEAN("Tab.ExternalApplicationOpened.NoUserInteraction",
                            user_accepted);
      break;
    case app_launcher_overlays::AppLaunchConfirmationRequestCause::
        kAppLaunchFailed:
      UMA_HISTOGRAM_BOOLEAN("Tab.ExternalApplicationOpened.Failed",
                            user_accepted);
      break;
  }
}

// Callback for the app launcher alert overlay.
void AppLauncherOverlayCallback(
    base::OnceCallback<void(bool)> completion,
    app_launcher_overlays::AppLaunchConfirmationRequestCause cause,
    OverlayResponse* response) {
  // Check whether the user has allowed the navigation.
  bool user_accepted = response && response->GetInfo<AllowAppLaunchResponse>();

  RecordAppLaunchRequestMetrics(cause, user_accepted);

  // Execute the completion with the response.
  DCHECK(!completion.is_null());
  std::move(completion).Run(user_accepted);
}

// Launches the app for `url` if `user_accepted` is true.
void LaunchExternalApp(const GURL url,
                       base::OnceCallback<void(bool)> completion,
                       bool user_accepted = true) {
  if (!user_accepted) {
    std::move(completion).Run(false);
    return;
  }
  [[UIApplication sharedApplication]
                openURL:net::NSURLWithGURL(url)
                options:@{}
      completionHandler:base::CallbackToBlock(std::move(completion))];
}

app_launcher_overlays::AppLaunchConfirmationRequestCause
RequestCauseFromActionCause(AppLauncherAlertCause cause) {
  switch (cause) {
    case AppLauncherAlertCause::kOther:
      return app_launcher_overlays::AppLaunchConfirmationRequestCause::kOther;
    case AppLauncherAlertCause::kRepeatedLaunchDetected:
      return app_launcher_overlays::AppLaunchConfirmationRequestCause::
          kRepeatedRequest;
    case AppLauncherAlertCause::kOpenFromIncognito:
      return app_launcher_overlays::AppLaunchConfirmationRequestCause::
          kOpenFromIncognito;
    case AppLauncherAlertCause::kNoUserInteraction:
      return app_launcher_overlays::AppLaunchConfirmationRequestCause::
          kNoUserInteraction;
    case AppLauncherAlertCause::kAppLaunchFailed:
      return app_launcher_overlays::AppLaunchConfirmationRequestCause::
          kAppLaunchFailed;
  }
}

}  // namespace

#pragma mark - AppLauncherBrowserAgent

AppLauncherBrowserAgent::AppLauncherBrowserAgent(Browser* browser)
    : tab_helper_delegate_(browser),
      tab_helper_delegate_installer_(&tab_helper_delegate_, browser) {}

AppLauncherBrowserAgent::~AppLauncherBrowserAgent() = default;

#pragma mark - AppLauncherBrowserAgent::TabHelperDelegate

AppLauncherBrowserAgent::TabHelperDelegate::TabHelperDelegate(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_);
  DCHECK(browser_->GetWebStateList());
}

AppLauncherBrowserAgent::TabHelperDelegate::~TabHelperDelegate() = default;

#pragma mark AppLauncherTabHelperDelegate

void AppLauncherBrowserAgent::TabHelperDelegate::LaunchAppForTabHelper(
    AppLauncherTabHelper* tab_helper,
    const GURL& url,
    base::OnceCallback<void(bool)> completion) {
  // Don't open application if chrome is not active.
  if ([[UIApplication sharedApplication] applicationState] !=
      UIApplicationStateActive) {
    std::move(completion).Run(false);
    return;
  }

  // Uses a Mailto Handler to open the appropriate app.
  if (url.SchemeIs(url::kMailToScheme)) {
    MailtoHandlerServiceFactory::GetForBrowserState(browser_->GetBrowserState())
        ->HandleMailtoURL(net::NSURLWithGURL(url),
                          base::BindOnce(std::move(completion), true));
    return;
  }

  LaunchExternalApp(url, std::move(completion));
}

void AppLauncherBrowserAgent::TabHelperDelegate::ShowAppLaunchAlert(
    AppLauncherTabHelper* tab_helper,
    AppLauncherAlertCause cause,
    base::OnceCallback<void(bool)> completion) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<AppLaunchConfirmationRequest>(
          RequestCauseFromActionCause(cause));
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&AppLauncherOverlayCallback, std::move(completion),
                     RequestCauseFromActionCause(cause)));
  GetQueueForAppLaunchDialog(tab_helper->web_state())
      ->AddRequest(std::move(request));
}

#pragma mark Private

OverlayRequestQueue*
AppLauncherBrowserAgent::TabHelperDelegate::GetQueueForAppLaunchDialog(
    web::WebState* web_state) {
  web::WebState* queue_web_state = web_state;
  // If an app launch navigation is occurring in a new tab, the tab will be
  // closed immediately after the navigation fails, cancelling the app launcher
  // dialog before it gets a chance to be shown.  When this occurs, use the
  // OverlayRequestQueue for the tab's opener instead.
  if (!web_state->GetNavigationItemCount() && web_state->HasOpener()) {
    WebStateList* web_state_list = browser_->GetWebStateList();
    const int index = web_state_list->GetIndexOfWebState(web_state);
    queue_web_state =
        web_state_list->GetOpenerOfWebStateAt(index).opener ?: queue_web_state;
  }
  return OverlayRequestQueue::FromWebState(queue_web_state,
                                           OverlayModality::kWebContentArea);
}
