// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/signin_identity_api.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"

namespace ios {
namespace provider {
namespace {

// Null implementation of SystemIdentityManager.
//
// Since this object never returns any identities, none of the method that
// take an identity as a parameter should be called (since all identities
// must be obtained from the SystemIdentityManager) and thus they assert.
//
// The existence of this object simplify SystemIdentityManager usage as the
// client code does not have to check that the object exists.
class ChromiumSystemIdentityManager final : public SystemIdentityManager {
 public:
  ChromiumSystemIdentityManager();
  ~ChromiumSystemIdentityManager() final;

  // SystemIdentityManager implementation.
  bool IsSigninSupported() final;
  bool HandleSessionOpenURLContexts(
      UIScene* scene,
      NSSet<UIOpenURLContext*>* url_contexts) final;
  void ApplicationDidDiscardSceneSessions(
      NSSet<UISceneSession*>* scene_sessions) final;
  void DismissDialogs() final;
  DismissViewCallback PresentAccountDetailsController(
      PresentDialogConfiguration configuration) final;
  DismissViewCallback PresentWebAndAppSettingDetailsController(
      PresentDialogConfiguration configuration) final;
  DismissViewCallback PresentLinkedServicesSettingsDetailsController(
      PresentDialogConfiguration configuration) final;
  id<SystemIdentityInteractionManager> CreateInteractionManager() final;
  void IterateOverIdentities(IdentityIteratorCallback callback) final;
  void ForgetIdentity(id<SystemIdentity> identity,
                      ForgetIdentityCallback callback) final;
  bool IdentityRemovedByUser(NSString* gaia_id) final;
  void GetAccessToken(id<SystemIdentity> identity,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) final;
  void GetAccessToken(id<SystemIdentity> identity,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) final;
  void FetchAvatarForIdentity(id<SystemIdentity> identity) final;
  UIImage* GetCachedAvatarForIdentity(id<SystemIdentity> identity) final;
  void GetHostedDomain(id<SystemIdentity> identity,
                       HostedDomainCallback callback) final;
  NSString* GetCachedHostedDomainForIdentity(id<SystemIdentity> identity) final;
  void FetchCapabilities(id<SystemIdentity> identity,
                         const std::set<std::string>& names,
                         FetchCapabilitiesCallback callback) final;
  bool HandleMDMNotification(id<SystemIdentity> identity,
                             NSArray<id<SystemIdentity>>* active_identities,
                             id<RefreshAccessTokenError> error,
                             HandleMDMCallback callback) final;
  bool IsMDMError(id<SystemIdentity> identity, NSError* error) final;
};

ChromiumSystemIdentityManager::ChromiumSystemIdentityManager() = default;

ChromiumSystemIdentityManager::~ChromiumSystemIdentityManager() = default;

bool ChromiumSystemIdentityManager::IsSigninSupported() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

bool ChromiumSystemIdentityManager::HandleSessionOpenURLContexts(
    UIScene* scene,
    NSSet<UIOpenURLContext*>* url_contexts) {
  // Nothing to do.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

void ChromiumSystemIdentityManager::ApplicationDidDiscardSceneSessions(
    NSSet<UISceneSession*>* scene_sessions) {
  // Nothing to do.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ChromiumSystemIdentityManager::DismissDialogs() {
  // Nothing to do.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SystemIdentityManager::DismissViewCallback
ChromiumSystemIdentityManager::PresentAccountDetailsController(
    PresentDialogConfiguration configuration) {
  NOTREACHED();
}

SystemIdentityManager::DismissViewCallback
ChromiumSystemIdentityManager::PresentWebAndAppSettingDetailsController(
    PresentDialogConfiguration configuration) {
  NOTREACHED();
}

SystemIdentityManager::DismissViewCallback
ChromiumSystemIdentityManager::PresentLinkedServicesSettingsDetailsController(
    PresentDialogConfiguration configuration) {
  NOTREACHED();
}

id<SystemIdentityInteractionManager>
ChromiumSystemIdentityManager::CreateInteractionManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return nil;
}

void ChromiumSystemIdentityManager::IterateOverIdentities(
    IdentityIteratorCallback callback) {
  // Nothing to do, there is no identities.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ChromiumSystemIdentityManager::ForgetIdentity(
    id<SystemIdentity> identity,
    ForgetIdentityCallback callback) {
  NOTREACHED();
}

bool ChromiumSystemIdentityManager::IdentityRemovedByUser(NSString* gaia_id) {
  NOTREACHED();
}

void ChromiumSystemIdentityManager::GetAccessToken(
    id<SystemIdentity> identity,
    const std::set<std::string>& scopes,
    AccessTokenCallback callback) {
  NOTREACHED();
}

void ChromiumSystemIdentityManager::GetAccessToken(
    id<SystemIdentity> identity,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    AccessTokenCallback callback) {
  NOTREACHED();
}

void ChromiumSystemIdentityManager::FetchAvatarForIdentity(
    id<SystemIdentity> identity) {
  NOTREACHED();
}

UIImage* ChromiumSystemIdentityManager::GetCachedAvatarForIdentity(
    id<SystemIdentity> identity) {
  NOTREACHED();
}

void ChromiumSystemIdentityManager::GetHostedDomain(
    id<SystemIdentity> identity,
    HostedDomainCallback callback) {
  NOTREACHED();
}

NSString* ChromiumSystemIdentityManager::GetCachedHostedDomainForIdentity(
    id<SystemIdentity> identity) {
  NOTREACHED();
}

void ChromiumSystemIdentityManager::FetchCapabilities(
    id<SystemIdentity> identity,
    const std::set<std::string>& names,
    FetchCapabilitiesCallback callback) {
  NOTREACHED();
}

bool ChromiumSystemIdentityManager::HandleMDMNotification(
    id<SystemIdentity> identity,
    NSArray<id<SystemIdentity>>* active_identities,
    id<RefreshAccessTokenError> error,
    HandleMDMCallback callback) {
  NOTREACHED();
}

bool ChromiumSystemIdentityManager::IsMDMError(id<SystemIdentity> identity,
                                               NSError* error) {
  NOTREACHED();
}

}  // anonymous namespace

std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager(
    id<SingleSignOnService> sso_service) {
  // Signin is not supported in Chromium, return a null object.
  return std::make_unique<ChromiumSystemIdentityManager>();
}

}  // namespace provider
}  // namespace ios
