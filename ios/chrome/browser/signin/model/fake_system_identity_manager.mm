// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"

#import "base/functional/bind.h"
#import "base/i18n/time_formatting.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "ios/chrome/browser/signin/model/fake_account_details_view_controller.h"
#import "ios/chrome/browser/signin/model/fake_refresh_access_token_error.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_details.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_interaction_manager.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager_storage.h"
#import "ios/chrome/browser/signin/model/refresh_access_token_error.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"

namespace {

// Expiration duration of the fake access token returned by `GetAccessToken()`.
// Needs to be cover the execution of a typical test, as tests usually set up
// their mock to expect just one token request and rely on the tokens being
// served from cache.
constexpr base::TimeDelta kAccessTokenExpiration = base::Minutes(5);

// Returns a hosted domain for identity.
NSString* FakeGetHostedDomainForIdentity(id<SystemIdentity> identity) {
  return base::SysUTF8ToNSString(
      gaia::ExtractDomainName(base::SysNSStringToUTF8(identity.userEmail)));
}

// Stores a pointer to the last created FakeSystemIdentityManager*. Used to
// check whether the conversion is possible in `FromSystemIdentityManager()`.
FakeSystemIdentityManager* gFakeSystemIdentityManager = nullptr;

}  // anonymous namespace

FakeSystemIdentityManager::FakeSystemIdentityManager()
    : FakeSystemIdentityManager(nil) {}

FakeSystemIdentityManager::FakeSystemIdentityManager(
    NSArray<id<SystemIdentity>>* identities)
    : storage_([[FakeSystemIdentityManagerStorage alloc] init]) {
  DCHECK(!gFakeSystemIdentityManager);
  gFakeSystemIdentityManager = this;

  for (id<SystemIdentity> identity in identities) {
    [storage_ addIdentity:identity];
  }
}

FakeSystemIdentityManager::~FakeSystemIdentityManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(gFakeSystemIdentityManager, this);
  gFakeSystemIdentityManager = nullptr;
}

// static
FakeSystemIdentityManager* FakeSystemIdentityManager::FromSystemIdentityManager(
    SystemIdentityManager* manager) {
  DCHECK_EQ(gFakeSystemIdentityManager, manager);
  return gFakeSystemIdentityManager;
}

void FakeSystemIdentityManager::AddIdentity(id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(![storage_ containsIdentity:identity]);
  [storage_ addIdentity:identity];
  FireIdentityListChanged(/*notify_user*/ false);

  // Set up capabilities to remove the delay while displaying the history sync
  // opt-in screen for testing.
  // TODO(b/327221052): verify if this should be replaced by a handler for
  // default capabilities.
  AccountCapabilitiesTestMutator* mutator =
      GetPendingCapabilitiesMutator(identity);
  mutator->set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);
}

void FakeSystemIdentityManager::AddIdentityWithUnknownCapabilities(
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(![storage_ containsIdentity:identity]);
  [storage_ addIdentity:identity];
  FireIdentityListChanged(/*notify_user*/ false);
}

void FakeSystemIdentityManager::AddIdentityWithCapabilities(
    id<SystemIdentity> identity,
    NSDictionary<NSString*, NSNumber*>* capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(![storage_ containsIdentity:identity]);
  [storage_ addIdentity:identity];
  AccountCapabilitiesTestMutator* mutator =
      GetPendingCapabilitiesMutator(identity);
  for (NSString* name in capabilities) {
    std::string stdString = base::SysNSStringToUTF8(name);
    bool value = capabilities[name].boolValue;
    mutator->SetCapability(stdString, value);
  }
  FireIdentityListChanged(/*notify_user*/ false);
}

void FakeSystemIdentityManager::ForgetIdentityFromOtherApplication(
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (![storage_ containsIdentity:identity])
    return;

  ForgetIdentityAsync(identity, base::DoNothing(), /*notify_user*/ true);
}

AccountCapabilitiesTestMutator*
FakeSystemIdentityManager::GetPendingCapabilitiesMutator(
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK([storage_ containsIdentity:identity]);
  FakeSystemIdentityDetails* details = [storage_ detailsForIdentity:identity];
  return details.pendingCapabilitiesMutator;
}

AccountCapabilities FakeSystemIdentityManager::GetVisibleCapabilities(
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK([storage_ containsIdentity:identity]);
  FakeSystemIdentityDetails* details = [storage_ detailsForIdentity:identity];
  return details.visibleCapabilities;
}

void FakeSystemIdentityManager::FireSystemIdentityReloaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FireIdentityListChanged(/*notify_user*/ true);
}

void FakeSystemIdentityManager::FireIdentityUpdatedNotification(
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FireIdentityUpdated(identity);
}

void FakeSystemIdentityManager::WaitForServiceCallbacksToComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (pending_callbacks_) {
    DCHECK(resume_closure_.is_null());

    base::RunLoop run_loop;
    resume_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

bool FakeSystemIdentityManager::ContainsIdentity(id<SystemIdentity> identity) {
  return [storage_ containsIdentity:identity];
}

id<RefreshAccessTokenError>
FakeSystemIdentityManager::CreateRefreshAccessTokenFailure(
    id<SystemIdentity> identity,
    HandleMDMNotificationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK([storage_ containsIdentity:identity]);
  FakeSystemIdentityDetails* details = [storage_ detailsForIdentity:identity];
  details.error = [[FakeRefreshAccessTokenError alloc]
      initWithCallback:std::move(callback)];
  return details.error;
}

bool FakeSystemIdentityManager::IsSigninSupported() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

bool FakeSystemIdentityManager::HandleSessionOpenURLContexts(
    UIScene* scene,
    NSSet<UIOpenURLContext*>* url_contexts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignored.
  return false;
}

void FakeSystemIdentityManager::ApplicationDidDiscardSceneSessions(
    NSSet<UISceneSession*>* scene_sessions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignored.
}

void FakeSystemIdentityManager::DismissDialogs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignored.
}

FakeSystemIdentityManager::DismissViewCallback
FakeSystemIdentityManager::PresentAccountDetailsController(
    PresentDialogConfiguration configuration) {
  ProceduralBlock dismissalCompletion = nil;
  if (configuration.dismissal_completion) {
    dismissalCompletion =
        base::CallbackToBlock(std::move(configuration.dismissal_completion));
  }
  FakeAccountDetailsViewController* account_details_view_controller =
      [[FakeAccountDetailsViewController alloc]
             initWithIdentity:configuration.identity
          dismissalCompletion:dismissalCompletion];
  [configuration.view_controller
      presentViewController:account_details_view_controller
                   animated:configuration.animated
                 completion:nil];
  return base::BindOnce(^(BOOL dismiss_animated) {
    [account_details_view_controller dismissAnimated:dismiss_animated];
  });
}

FakeSystemIdentityManager::DismissViewCallback
FakeSystemIdentityManager::PresentWebAndAppSettingDetailsController(
    PresentDialogConfiguration configuration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::DoNothing();
}

FakeSystemIdentityManager::DismissViewCallback
FakeSystemIdentityManager::PresentLinkedServicesSettingsDetailsController(
    PresentDialogConfiguration configuration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::DoNothing();
}

id<SystemIdentityInteractionManager>
FakeSystemIdentityManager::CreateInteractionManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return [[FakeSystemIdentityInteractionManager alloc]
      initWithManager:GetWeakPtr()];
}

void FakeSystemIdentityManager::IterateOverIdentities(
    IdentityIteratorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (FakeSystemIdentityDetails* details in storage_) {
    if (callback.Run(details.identity) == IteratorResult::kInterruptIteration)
      break;
  }
}

void FakeSystemIdentityManager::ForgetIdentity(
    id<SystemIdentity> identity,
    ForgetIdentityCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK([storage_ containsIdentity:identity]);
  // Forgetting an identity is an asynchronous operation (as it requires some
  // network calls).
  PostClosure(FROM_HERE,
              base::BindOnce(&FakeSystemIdentityManager::ForgetIdentityAsync,
                             GetWeakPtr(), identity, std::move(callback),
                             /*notify_user*/ false));
}

void FakeSystemIdentityManager::GetAccessToken(
    id<SystemIdentity> identity,
    const std::set<std::string>& scopes,
    AccessTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetAccessToken(identity, /*client_id*/ {}, scopes, std::move(callback));
}

void FakeSystemIdentityManager::GetAccessToken(
    id<SystemIdentity> identity,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    AccessTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK([storage_ containsIdentity:identity]);
  // Fetching the access token is an asynchronous operation (as it requires
  // some network calls).
  PostClosure(FROM_HERE,
              base::BindOnce(&FakeSystemIdentityManager::GetAccessTokenAsync,
                             GetWeakPtr(), identity, std::move(callback)));
}

void FakeSystemIdentityManager::FetchAvatarForIdentity(
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Fetching the avatar is an asynchronous operation (as it requires some
  // network calls).
  DCHECK([storage_ containsIdentity:identity]);
  PostClosure(
      FROM_HERE,
      base::BindOnce(&FakeSystemIdentityManager::FetchAvatarForIdentityAsync,
                     GetWeakPtr(), identity));
}

UIImage* FakeSystemIdentityManager::GetCachedAvatarForIdentity(
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK([storage_ containsIdentity:identity]);
  FakeSystemIdentityDetails* details = [storage_ detailsForIdentity:identity];
  return details.cachedAvatar;
}

void FakeSystemIdentityManager::GetHostedDomain(id<SystemIdentity> identity,
                                                HostedDomainCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK([storage_ containsIdentity:identity]);
  // Fetching the hosted domain is an asynchronous operation (as it requires
  // some network calls).
  PostClosure(FROM_HERE,
              base::BindOnce(&FakeSystemIdentityManager::GetHostedDomainAsync,
                             GetWeakPtr(), identity, std::move(callback)));
}

NSString* FakeSystemIdentityManager::GetCachedHostedDomainForIdentity(
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSString* domain = FakeGetHostedDomainForIdentity(identity);
  return [domain isEqualToString:@"gmail.com"] ? @"" : domain;
}

void FakeSystemIdentityManager::FetchCapabilities(
    id<SystemIdentity> identity,
    const std::set<std::string>& names,
    FetchCapabilitiesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK([storage_ containsIdentity:identity]);
  // Fetching the hosted domain is an asynchronous operation (as it requires
  // some network calls).
  PostClosure(
      FROM_HERE,
      base::BindOnce(&FakeSystemIdentityManager::FetchCapabilitiesAsync,
                     GetWeakPtr(), identity, names, std::move(callback)));
}

bool FakeSystemIdentityManager::HandleMDMNotification(
    id<SystemIdentity> identity,
    NSArray<id<SystemIdentity>>* active_identities,
    id<RefreshAccessTokenError> error,
    HandleMDMCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK([storage_ containsIdentity:identity]);
  FakeSystemIdentityDetails* details = [storage_ detailsForIdentity:identity];
  if (![details.error isEqualToError:error]) {
    return false;
  }

  // Handling MDM error is asynchronous operation (as it requires some
  // network calls).
  PostClosure(FROM_HERE,
              base::BindOnce(details.error.callback, std::move(callback)));
  return true;
}

bool FakeSystemIdentityManager::IsMDMError(id<SystemIdentity> identity,
                                           NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

base::WeakPtr<FakeSystemIdentityManager>
FakeSystemIdentityManager::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeSystemIdentityManager::ForgetIdentityAsync(
    id<SystemIdentity> identity,
    ForgetIdentityCallback callback,
    bool notify_user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (![storage_ containsIdentity:identity]) {
    // The identity was removed before async method was called. There is
    // nothing to do.
    return;
  }
  [storage_ removeIdentity:identity];

  FireIdentityListChanged(notify_user);

  std::move(callback).Run(/*error*/ nil);
}

void FakeSystemIdentityManager::GetAccessTokenAsync(
    id<SystemIdentity> identity,
    AccessTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (![storage_ containsIdentity:identity]) {
    // The identity was removed before async method was called. There is
    // nothing to do.
    return;
  }
  FakeSystemIdentityDetails* details = [storage_ detailsForIdentity:identity];
  if (details.error) {
    NSError* error = [NSError errorWithDomain:@"com.google.HTTPStatus"
                                         code:-1
                                     userInfo:nil];

    FireIdentityAccessTokenRefreshFailed(identity, details.error);
    std::move(callback).Run(std::nullopt, error);
  } else {
    const base::Time valid_until = base::Time::Now() + kAccessTokenExpiration;
    AccessTokenInfo info{TimeFormatHTTP(valid_until), valid_until};
    std::move(callback).Run(std::move(info), nil);
  }
}

void FakeSystemIdentityManager::FetchAvatarForIdentityAsync(
    id<SystemIdentity> identity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (![storage_ containsIdentity:identity]) {
    // The identity was removed before async method was called. There is
    // nothing to do.
    return;
  }
  FakeSystemIdentityDetails* details = [storage_ detailsForIdentity:identity];
  if (!details.cachedAvatar) {
    details.cachedAvatar = ios::provider::GetSigninDefaultAvatar();
  }

  FireIdentityUpdated(identity);
}

void FakeSystemIdentityManager::GetHostedDomainAsync(
    id<SystemIdentity> identity,
    HostedDomainCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (![storage_ containsIdentity:identity]) {
    // The identity was removed before async method was called. There is
    // nothing to do.
    return;
  }
  std::move(callback).Run(FakeGetHostedDomainForIdentity(identity), nil);
}

void FakeSystemIdentityManager::FetchCapabilitiesAsync(
    id<SystemIdentity> identity,
    const std::set<std::string>& names,
    FetchCapabilitiesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (![storage_ containsIdentity:identity]) {
    // The identity was removed before async method was called. There is
    // nothing to do.
    return;
  }
  FakeSystemIdentityDetails* details = [storage_ detailsForIdentity:identity];

  // Simulates the action to refresh the internal capability state with
  // the pending changes fetched from the server.
  [details updateVisibleCapabilities];
  const FakeSystemIdentityCapabilitiesMap& capabilities =
      details.visibleCapabilities;

  std::map<std::string, CapabilityResult> result;
  for (const std::string& name : names) {
    const auto& iter = capabilities.find(name);
    if (iter == capabilities.end()) {
      result.insert({name, CapabilityResult::kUnknown});
    } else {
      result.insert({name, iter->second ? CapabilityResult::kTrue
                                        : CapabilityResult::kFalse});
    }
  }

  std::move(callback).Run(result);
}

void FakeSystemIdentityManager::PostClosure(base::Location from_here,
                                            base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++pending_callbacks_;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      from_here, base::BindOnce(&FakeSystemIdentityManager::ExecuteClosure,
                                GetWeakPtr(), std::move(closure)));
}

void FakeSystemIdentityManager::ExecuteClosure(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(pending_callbacks_, 0u);
  if (--pending_callbacks_ == 0u) {
    if (!resume_closure_.is_null()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(resume_closure_));
    }
  }

  if (!closure.is_null()) {
    std::move(closure).Run();
  }
}
