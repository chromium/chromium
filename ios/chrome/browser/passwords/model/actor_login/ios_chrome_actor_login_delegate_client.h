// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_ACTOR_LOGIN_IOS_CHROME_ACTOR_LOGIN_DELEGATE_CLIENT_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_ACTOR_LOGIN_IOS_CHROME_ACTOR_LOGIN_DELEGATE_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate_client.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"

namespace actor_login {
class ActorLoginWebContentInterface;
class ActorLoginPermissionCleaningService;
class ActorLoginCredentialsFetcher;
class ActorLoginQualityLoggerInterface;
class ActorLoginMetricsHelper;
class ActorLoginSiwgControllerInterface;
class ActionSequenceDelegate;
struct Credential;
}  // namespace actor_login

namespace password_manager {
class PasswordFormCache;
}  // namespace password_manager

@protocol ActorLoginToolDelegate;
class PrefService;

// iOS-specific implementation of `ActorLoginDelegateClient`.
class IOSChromeActorLoginDelegateClient
    : public actor_login::ActorLoginDelegateClient,
      public web::WebStateObserver,
      public web::WebStateUserData<IOSChromeActorLoginDelegateClient> {
 public:
  ~IOSChromeActorLoginDelegateClient() override;

  // Not copyable or movable.
  IOSChromeActorLoginDelegateClient(const IOSChromeActorLoginDelegateClient&) =
      delete;
  IOSChromeActorLoginDelegateClient& operator=(
      const IOSChromeActorLoginDelegateClient&) = delete;

  // Returns the delegate for Actor Login form extraction.
  id<ActorLoginToolDelegate> GetActorLoginToolDelegate();

  // Returns the PasswordFormCache for the associated WebState.
  password_manager::PasswordFormCache* GetPasswordFormCache();

  // ActorLoginDelegateClient:
  void SetActorLoginWebContentInterface(
      actor_login::ActorLoginWebContentInterface* web_interface) override;
  PrefService* GetPrefs() override;
  password_manager::PasswordManagerClient* GetPasswordManagerClient() override;
  password_manager::PasswordManagerDriver*
  GetPasswordManagerDriverForMainFrame() override;
  ukm::SourceId GetPageUkmSourceIdForMainFrame() override;
  url::Origin GetLastCommittedOriginForMainFrame() override;
  translate::TranslateManager* GetTranslateManager() override;
  actor_login::ActorLoginPermissionCleaningService*
  GetPermissionCleaningService() override;
  std::unique_ptr<actor_login::ActorLoginCredentialsFetcher>
  CreateFederatedCredentialsFetcher(
      base::WeakPtr<actor_login::ActorLoginQualityLoggerInterface> mqls_logger,
      actor_login::ActorLoginMetricsHelper* metrics_helper) override;
  std::unique_ptr<actor_login::ActorLoginSiwgControllerInterface>
  CreateSiwgController(
      const actor_login::Credential& credential,
      bool should_store_permission,
      actor_login::LoginStatusResultOrErrorReply on_finished_callback,
      base::WeakPtr<actor_login::ActionSequenceDelegate>
          action_sequence_delegate,
      base::WeakPtr<actor_login::ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_tool_start_time,
      base::OnceCallback<void(bool)> post_button_click_login_result_callback)
      override;
  bool IsTaskInFocus() override;
  bool SupportsFedCmEmbedderInitiatedLogin() override;
  void RemoveFederatedEmbedderLoginRequest() override;
  void ObserveControlStateForCurrentTask(
      base::OnceClosure on_released_callback) override;
  base::WeakPtr<actor_login::ActorLoginDelegateClient> AsWeakPtr() override;

  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<IOSChromeActorLoginDelegateClient>;

  explicit IOSChromeActorLoginDelegateClient(web::WebState* web_state);

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Weak reference to the registered delegate interface to dispatch
  // WebState-scoped events.
  raw_ptr<actor_login::ActorLoginWebContentInterface> web_interface_ = nullptr;

  base::WeakPtrFactory<IOSChromeActorLoginDelegateClient> weak_ptr_factory_{
      this};
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_ACTOR_LOGIN_IOS_CHROME_ACTOR_LOGIN_DELEGATE_CLIENT_H_
