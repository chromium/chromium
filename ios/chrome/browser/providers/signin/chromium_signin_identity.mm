// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/signin_identity_api.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
      id<SystemIdentity> identity,
      UIViewController* view_controller,
      bool animated) final;
  DismissViewCallback PresentWebAndAppSettingDetailsController(
      id<SystemIdentity> identity,
      UIViewController* view_controller,
      bool animated) final;
  id<SystemIdentityInteractionManager> CreateInteractionManager() final;
  void IterateOverIdentities(IdentityIteratorCallback callback) final;
  void ForgetIdentity(id<SystemIdentity> identity,
                      ForgetIdentityCallback callback) final;
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
    id<SystemIdentity> identity,
    UIViewController* view_controller,
    bool animated) {
  NOTREACHED();
  return {};
}

SystemIdentityManager::DismissViewCallback
ChromiumSystemIdentityManager::PresentWebAndAppSettingDetailsController(
    id<SystemIdentity> identity,
    UIViewController* view_controller,
    bool animated) {
  NOTREACHED();
  return {};
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
  return nil;
}

void ChromiumSystemIdentityManager::GetHostedDomain(
    id<SystemIdentity> identity,
    HostedDomainCallback callback) {
  NOTREACHED();
}

NSString* ChromiumSystemIdentityManager::GetCachedHostedDomainForIdentity(
    id<SystemIdentity> identity) {
  NOTREACHED();
  return @"";
}

void ChromiumSystemIdentityManager::FetchCapabilities(
    id<SystemIdentity> identity,
    const std::set<std::string>& names,
    FetchCapabilitiesCallback callback) {
  NOTREACHED();
}

bool ChromiumSystemIdentityManager::HandleMDMNotification(
    id<SystemIdentity> identity,
    id<RefreshAccessTokenError> error,
    HandleMDMCallback callback) {
  NOTREACHED();
  return false;
}

bool ChromiumSystemIdentityManager::IsMDMError(id<SystemIdentity> identity,
                                               NSError* error) {
  NOTREACHED();
  return false;
}

}  // anonymous namespace

std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager(
    id<SingleSignOnService> sso_service) {
  // Signin is not supported in Chromium, return a null object.
  return std::make_unique<ChromiumSystemIdentityManager>();
}

}  // namespace provider
}  // namespace ios
