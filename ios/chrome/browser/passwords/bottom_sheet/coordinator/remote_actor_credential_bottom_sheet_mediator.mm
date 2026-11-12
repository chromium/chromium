// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/remote_actor_credential_bottom_sheet_mediator.h"

#import <memory>
#import <optional>
#import <utility>
#import <vector>

#import "base/check.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/types/expected.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_controller.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_utils.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_consumer.h"
#import "ios/chrome/browser/passwords/model/remote_actor/proto/remote_actor_password_upload_request.pb.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_credential_sharing_delegate_bridge.h"
#import "ios/chrome/browser/passwords/password_suggestion/ui/password_suggestion_utils.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

using password_manager::RemoteActorCredentialSharingResult;

}  // namespace

@interface RemoteActorCredentialBottomSheetMediator () <
    RemoteActorCredentialSharingDelegate>
@end

@implementation RemoteActorCredentialBottomSheetMediator {
  // Dependencies of the sharing controller.
  raw_ptr<signin::IdentityManager> _identityManager;
  raw_ptr<syncer::SyncService> _syncService;
  raw_ptr<password_manager::PasswordStoreInterface> _profilePasswordStore;
  raw_ptr<password_manager::PasswordStoreInterface> _accountPasswordStore;
  raw_ptr<password_manager::RemoteActorCredentialSharingService>
      _sharingService;

  // Client used by the sharing controller for device reauthentication.
  raw_ptr<password_manager::PasswordManagerClient> _passwordManagerClient;

  // C++ bridge implementing the controller's `Delegate` interface.
  std::unique_ptr<RemoteActorCredentialSharingDelegateBridge> _delegateBridge;

  // Controller coordinating the sharing flow with password stores and backend.
  std::unique_ptr<password_manager::RemoteActorCredentialSharingController>
      _controller;

  // Pending callback to return the chosen credential or error to the
  // controller.
  RemoteActorCredentialSelectionCallback _credentialSelectedCallback;

  // Stored password forms matching `_origin`.
  std::vector<std::unique_ptr<password_manager::PasswordForm>> _credentials;

  // Origin for which credentials are being selected.
  url::Origin _origin;

  // `FaviconLoader` used to load the favicon.
  raw_ptr<FaviconLoader> _faviconLoader;

  // Remote actor request forwarded to the sharing controller on start.
  remote_actor::proto::RemoteActorPasswordUploadRequest _request;
}

- (instancetype)
    initWithIdentityManager:(signin::IdentityManager*)identityManager
                syncService:(syncer::SyncService*)syncService
       profilePasswordStore:
           (password_manager::PasswordStoreInterface*)profilePasswordStore
       accountPasswordStore:
           (password_manager::PasswordStoreInterface*)accountPasswordStore
             sharingService:
                 (password_manager::RemoteActorCredentialSharingService*)
                     sharingService
      passwordManagerClient:
          (password_manager::PasswordManagerClient*)passwordManagerClient
              faviconLoader:(FaviconLoader*)faviconLoader
                    request:
                        (remote_actor::proto::RemoteActorPasswordUploadRequest)
                            request {
  self = [super init];
  if (self) {
    _identityManager = identityManager;
    _syncService = syncService;
    _profilePasswordStore = profilePasswordStore;
    _accountPasswordStore = accountPasswordStore;
    _sharingService = sharingService;
    _passwordManagerClient = passwordManagerClient;
    _faviconLoader = faviconLoader;
    _request = std::move(request);
  }
  return self;
}

- (void)start {
  CHECK(!_controller);
  _delegateBridge =
      std::make_unique<RemoteActorCredentialSharingDelegateBridge>(self);
  _controller = std::make_unique<
      password_manager::RemoteActorCredentialSharingController>(
      _identityManager, _syncService, _profilePasswordStore,
      _accountPasswordStore, _sharingService, _passwordManagerClient,
      _delegateBridge.get());

  __weak __typeof(self) weakSelf = self;
  _controller->Start(
      _request.gaia_id(), _request.domain(), _request.task_id(),
      base::BindOnce(^(RemoteActorCredentialSharingResult result) {
        [weakSelf handleSharingResult:result];
      }));
}

- (void)disconnect {
  if (_credentialSelectedCallback) {
    std::move(_credentialSelectedCallback)
        .Run(base::unexpected(
            RemoteActorCredentialSharingResult::kUserCancelledDialog));
  }

  _controller.reset();
  _delegateBridge.reset();
  _identityManager = nullptr;
  _syncService = nullptr;
  _profilePasswordStore = nullptr;
  _accountPasswordStore = nullptr;
  _sharingService = nullptr;
  _passwordManagerClient = nullptr;
  _consumer = nil;
  _delegate = nil;
  _faviconLoader = nullptr;
  _credentials.clear();
  _origin = url::Origin();
  _request.Clear();
}

#pragma mark - Properties

- (void)setConsumer:(id<RemoteActorCredentialBottomSheetConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [_consumer setDomain:[self shownDomain]];
  [self pushStateToConsumer];
}

#pragma mark - RemoteActorCredentialSharingDelegate

- (void)
    showCredentialSelectionWithCredentials:
        (std::vector<std::unique_ptr<password_manager::PasswordForm>>)
            credentials
                                    origin:(const url::Origin&)origin
                                  callback:
                                      (RemoteActorCredentialSelectionCallback)
                                          callback {
  // The controller resolves the flow itself when no credentials are found.
  CHECK(!credentials.empty());
  _origin = origin;
  _credentialSelectedCallback = std::move(callback);
  _credentials = std::move(credentials);
  [self pushStateToConsumer];
}

#pragma mark - RemoteActorCredentialBottomSheetMutator

- (void)didSelectCredentialAtIndex:(NSInteger)index {
  if (!_credentialSelectedCallback) {
    return;
  }
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), _credentials.size());
  [_consumer setLoading:YES];
  std::move(_credentialSelectedCallback).Run(*_credentials[index]);
}

- (void)didCancel {
  if (_credentialSelectedCallback) {
    // The controller reports the final result through `handleSharingResult:`.
    std::move(_credentialSelectedCallback)
        .Run(base::unexpected(
            RemoteActorCredentialSharingResult::kUserCancelledDialog));
    return;
  }
  [self.delegate mediator:self
      didFinishWithResult:RemoteActorCredentialSharingResult::
                              kUserCancelledDialog];
}

#pragma mark - Private

// Returns the requested domain formatted for display, or an empty string if the
// request does not hold a valid origin (the controller then rejects the
// request). `GetShownOrigin()` is needed to handle look-alike domains.
- (NSString*)shownDomain {
  std::optional<url::Origin> origin =
      password_manager::GetOriginFromDomainOrOrigin(_request.domain());
  if (!origin) {
    return @"";
  }
  return base::SysUTF8ToNSString(password_manager::GetShownOrigin(*origin));
}

// Pushes the suggestions and favicon to `_consumer`. The consumer may be set
// before the credentials arrive asynchronously, so this is a no-op until both
// are available.
- (void)pushStateToConsumer {
  if (!_consumer || _credentials.empty()) {
    return;
  }
  NSMutableArray<FormSuggestion*>* suggestions =
      [NSMutableArray arrayWithCapacity:_credentials.size()];
  for (const auto& credential : _credentials) {
    [suggestions
        addObject:
            [FormSuggestion
                suggestionWithValue:base::SysUTF16ToNSString(
                                        credential->username_value)
                 displayDescription:nil
                               icon:nil
                               type:autofill::SuggestionType::kPasswordEntry
                            payload:autofill::Suggestion::Payload()
                     requiresReauth:YES]];
  }
  [_consumer setSuggestions:suggestions];
  [self fetchFavicon];
}

// Handles the final sharing `result` from the controller.
- (void)handleSharingResult:(RemoteActorCredentialSharingResult)result {
  [_consumer setLoading:NO];
  [self.delegate mediator:self didFinishWithResult:result];
}

// Fetches the favicon for `_origin` and forwards the attributes to the
// consumer.
- (void)fetchFavicon {
  GURL faviconURL = _origin.GetURL();
  if (!_faviconLoader || !faviconURL.is_valid()) {
    [_consumer setFaviconAttributes:GetDefaultGlobeFaviconAttributes()];
    return;
  }

  __weak id<RemoteActorCredentialBottomSheetConsumer> weakConsumer = _consumer;
  _faviconLoader->FaviconForPageUrl(
      faviconURL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false,
      ^(FaviconAttributes* attributes, bool cached) {
        [weakConsumer setFaviconAttributes:attributes];
      });
}

@end
