// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/actor_login/ios_chrome_actor_login_delegate_client.h"

#import "base/notimplemented.h"
#import "components/password_manager/core/browser/actor_login/internal/actor_login_web_content_interface.h"
#import "components/password_manager/core/browser/password_form_cache.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/prefs/pref_service.h"
#import "components/translate/core/browser/translate_manager.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/passwords/model/actor_login/ios_chrome_actor_login_permission_cleaning_service_factory.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"
#import "url/origin.h"

using actor_login::ActionSequenceDelegate;
using actor_login::ActorLoginCredentialsFetcher;
using actor_login::ActorLoginDelegateClient;
using actor_login::ActorLoginMetricsHelper;
using actor_login::ActorLoginPermissionCleaningService;
using actor_login::ActorLoginQualityLoggerInterface;
using actor_login::ActorLoginSiwgControllerInterface;
using actor_login::ActorLoginWebContentInterface;
using actor_login::Credential;
using actor_login::LoginStatusResultOrErrorReply;

IOSChromeActorLoginDelegateClient::IOSChromeActorLoginDelegateClient(
    web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

IOSChromeActorLoginDelegateClient::~IOSChromeActorLoginDelegateClient() =
    default;

id<ActorLoginToolDelegate>
IOSChromeActorLoginDelegateClient::GetActorLoginToolDelegate() {
  if (auto* helper = PasswordTabHelper::FromWebState(web_state_)) {
    return helper->GetSharedPasswordController();
  }
  return nil;
}

password_manager::PasswordFormCache*
IOSChromeActorLoginDelegateClient::GetPasswordFormCache() {
  if (auto* helper = PasswordTabHelper::FromWebState(web_state_)) {
    if (auto* password_manager = helper->GetPasswordManager()) {
      return password_manager->GetPasswordFormCache();
    }
  }
  return nullptr;
}

void IOSChromeActorLoginDelegateClient::SetActorLoginWebContentInterface(
    ActorLoginWebContentInterface* web_interface) {
  web_interface_ = web_interface;
}

PrefService* IOSChromeActorLoginDelegateClient::GetPrefs() {
  return ProfileIOS::FromBrowserState(web_state_->GetBrowserState())
      ->GetPrefs();
}

password_manager::PasswordManagerClient*
IOSChromeActorLoginDelegateClient::GetPasswordManagerClient() {
  if (auto* helper = PasswordTabHelper::FromWebState(web_state_)) {
    return helper->GetPasswordManagerClient();
  }
  return nullptr;
}

password_manager::PasswordManagerDriver*
IOSChromeActorLoginDelegateClient::GetPasswordManagerDriverForMainFrame() {
  web::WebFramesManager* frames_manager =
      password_manager::PasswordManagerJavaScriptFeature::GetInstance()
          ->GetWebFramesManager(web_state_);
  web::WebFrame* main_frame =
      frames_manager ? frames_manager->GetMainWebFrame() : nullptr;
  return main_frame ? IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(
                          web_state_, main_frame)
                    : nullptr;
}

ukm::SourceId
IOSChromeActorLoginDelegateClient::GetPageUkmSourceIdForMainFrame() {
  return ukm::GetSourceIdForWebStateDocument(web_state_);
}

url::Origin
IOSChromeActorLoginDelegateClient::GetLastCommittedOriginForMainFrame() {
  web::WebFramesManager* frames_manager =
      password_manager::PasswordManagerJavaScriptFeature::GetInstance()
          ->GetWebFramesManager(web_state_);
  web::WebFrame* main_frame =
      frames_manager ? frames_manager->GetMainWebFrame() : nullptr;
  if (!main_frame) {
    return url::Origin();
  }
  return main_frame->GetSecurityOrigin();
}

translate::TranslateManager*
IOSChromeActorLoginDelegateClient::GetTranslateManager() {
  if (auto* client = ChromeIOSTranslateClient::FromWebState(web_state_)) {
    return client->GetTranslateManager();
  }
  return nullptr;
}

ActorLoginPermissionCleaningService*
IOSChromeActorLoginDelegateClient::GetPermissionCleaningService() {
  return IOSChromeActorLoginPermissionCleaningServiceFactory::GetForProfile(
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
}

std::unique_ptr<ActorLoginCredentialsFetcher>
IOSChromeActorLoginDelegateClient::CreateFederatedCredentialsFetcher(
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    ActorLoginMetricsHelper* metrics_helper) {
  // Not supported on iOS.
  return nullptr;
}

std::unique_ptr<ActorLoginSiwgControllerInterface>
IOSChromeActorLoginDelegateClient::CreateSiwgController(
    const Credential& credential,
    bool should_store_permission,
    LoginStatusResultOrErrorReply on_finished_callback,
    base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    base::TimeTicks attempt_login_tool_start_time,
    base::OnceCallback<void(bool)> post_button_click_login_result_callback) {
  // Not supported on iOS.
  NOTREACHED(base::NotFatalUntil::M155)
      << "Should not have received federated login credential on iOS.";
  return nullptr;
}

bool IOSChromeActorLoginDelegateClient::IsTaskInFocus() {
  // TODO(crbug.com/496662229): Take into account the case when the web state is
  // not active, but a task is actuating on this web state and the task card is
  // visible.
  return web_state_->IsVisible();
}

bool IOSChromeActorLoginDelegateClient::SupportsFedCmEmbedderInitiatedLogin() {
  return false;
}

void IOSChromeActorLoginDelegateClient::RemoveFederatedEmbedderLoginRequest() {
  NOTREACHED(base::NotFatalUntil::M155)
      << "Should not have received federated login credential on iOS.";
}

void IOSChromeActorLoginDelegateClient::ObserveControlStateForCurrentTask(
    base::OnceClosure on_released_callback) {
  // Not supported on iOS.
  NOTREACHED(base::NotFatalUntil::M155)
      << "Should not have received federated login credential on iOS.";
}

base::WeakPtr<ActorLoginDelegateClient>
IOSChromeActorLoginDelegateClient::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void IOSChromeActorLoginDelegateClient::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->HasCommitted() &&
      !navigation_context->IsSameDocument() && web_interface_) {
    web_interface_->OnPrimaryPageChanged();
  }
}

void IOSChromeActorLoginDelegateClient::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
  if (web_interface_) {
    web_interface_->OnContextDestroyed();
  }
  web_interface_ = nullptr;
}
