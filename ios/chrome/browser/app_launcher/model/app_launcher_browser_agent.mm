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
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

BROWSER_USER_DATA_KEY_IMPL(AppLauncherBrowserAgent)

using app_launcher_overlays::AllowAppLaunchResponse;
using app_launcher_overlays::AppLaunchConfirmationRequest;

// A bridge class to observe the SceneState transitions.
@interface AppLauncherSceneStateObserver : NSObject <SceneStateObserver>
- (instancetype)initWithTransitionCallback:(base::RepeatingClosure)callback;
@end

@implementation AppLauncherSceneStateObserver {
  base::RepeatingClosure _transitionCallback;
}

- (instancetype)initWithTransitionCallback:(base::RepeatingClosure)callback {
  self = [super init];
  if (self) {
    _transitionCallback = callback;
  }
  return self;
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  _transitionCallback.Run();
}

@end

namespace {
namespace {
// After a successfull launch, will wait for this delay before checking the
// application status. If the application is backgrounded or active at this
// point, call the final completion handler.
// This delay will avoid staying in "no navigation" mode if we miss the
// activation signal (or if the app was never inactivated like opening another
// app in another window on iPad).
static constexpr base::TimeDelta kCheckSceneStatusDelay = base::Seconds(1);
}  // namespace

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
      tab_helper_delegate_installer_(&tab_helper_delegate_, browser) {
  browser->AddObserver(this);
  app_launcher_scene_state_observer_ = [[AppLauncherSceneStateObserver alloc]
      initWithTransitionCallback:
          base::BindRepeating(&AppLauncherBrowserAgent::TabHelperDelegate::
                                  SceneActivationLevelChanged,
                              tab_helper_delegate_.AsWeakPtr())];
  [browser->GetSceneState() addObserver:app_launcher_scene_state_observer_];
}

AppLauncherBrowserAgent::~AppLauncherBrowserAgent() = default;

#pragma mark - BrowserObserver

void AppLauncherBrowserAgent::BrowserDestroyed(Browser* browser) {
  [browser->GetSceneState() removeObserver:app_launcher_scene_state_observer_];
  browser->RemoveObserver(this);
}

#pragma mark - AppLauncherBrowserAgent::TabHelperDelegate

AppLauncherBrowserAgent::TabHelperDelegate::TabHelperDelegate(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_);
  DCHECK(browser_->GetWebStateList());
}

AppLauncherBrowserAgent::TabHelperDelegate::~TabHelperDelegate() = default;

base::WeakPtr<AppLauncherBrowserAgent::TabHelperDelegate>
AppLauncherBrowserAgent::TabHelperDelegate::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

#pragma mark AppLauncherTabHelperDelegate

void AppLauncherBrowserAgent::TabHelperDelegate::LaunchAppForTabHelper(
    AppLauncherTabHelper* tab_helper,
    const GURL& url,
    base::OnceCallback<void(bool)> completion,
    base::OnceCallback<void()> back_completion) {
  // Don't open application if the scene is not active or if an app opening is
  // already in progress (should only happen if there are very quick calls to
  // this callback).
  if ([browser_->GetSceneState() activationLevel] !=
          SceneActivationLevelForegroundActive ||
      app_launch_completion_ || back_to_app_completion_) {
    std::move(completion).Run(false);
    return;
  }

  app_launch_completion_ = std::move(completion);
  back_to_app_completion_ = std::move(back_completion);
  base::OnceCallback<void(bool)> launch_completion = base::BindOnce(
      &AppLauncherBrowserAgent::TabHelperDelegate::OnAppLaunchCompleted,
      AsWeakPtr());

  // Uses a Mailto Handler to open the appropriate app.
  if (url.SchemeIs(url::kMailToScheme)) {
    MailtoHandlerServiceFactory::GetForProfile(browser_->GetProfile())
        ->HandleMailtoURL(net::NSURLWithGURL(url),
                          base::BindOnce(std::move(launch_completion), true));
    return;
  }

  LaunchExternalApp(url, std::move(launch_completion));
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
  return OverlayRequestQueue::FromWebState(web_state,
                                           OverlayModality::kWebContentArea);
}

// Called in the completion handler of `UIApplication openURL:...`.
void AppLauncherBrowserAgent::TabHelperDelegate::OnAppLaunchCompleted(
    bool success) {
  if (!success) {
    back_to_app_completion_.Reset();
  }
  if (app_launch_completion_) {
    std::move(app_launch_completion_).Run(success);
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&AppLauncherBrowserAgent::TabHelperDelegate::
                   SceneActivationLevelChanged,
               AsWeakPtr()),
      kCheckSceneStatusDelay);
}

// Called on SceneState activation transitions.
void AppLauncherBrowserAgent::TabHelperDelegate::SceneActivationLevelChanged() {
  if (browser_->GetSceneState().activationLevel !=
          SceneActivationLevelForegroundInactive &&
      back_to_app_completion_) {
    std::move(back_to_app_completion_).Run();
  }
}
