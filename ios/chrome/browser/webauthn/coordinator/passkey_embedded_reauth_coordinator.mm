// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/passkey_embedded_reauth_coordinator.h"

#import <memory>
#import <string>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/location.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/types/expected.h"
#import "components/webauthn/ios/ios_passkey_client_commands.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/public/provider/chrome/browser/signin/embedded_reauth_api.h"
#import "ios/public/provider/chrome/browser/signin/embedded_reauth_interaction_manager.h"

@implementation PasskeyEmbeddedReAuthCoordinator {
  // The Packaged Lookup Token for the ReAuth challenge.
  NSString* _packagedLookupToken;

  // The callback to run when the ReAuth challenge completes or is cancelled.
  webauthn::IOSPasskeyClient::EmbeddedReAuthCallback _callback;

  // The interaction manager used to present the embedded ReAuth UI.
  std::unique_ptr<EmbeddedReAuthInteractionManager> _interactionManager;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
           packagedLookupToken:(NSString*)packagedLookupToken
                      callback:
                          (webauthn::IOSPasskeyClient::EmbeddedReAuthCallback)
                              callback {
  CHECK(callback);
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _packagedLookupToken = [packagedLookupToken copy];
    _callback = std::move(callback);
  }
  return self;
}

- (void)dealloc {
  CHECK(!_interactionManager);
}

#pragma mark - ChromeCoordinator

- (void)start {
  // Embedded ReAuth can be presented over an incognito browser, but the
  // `AuthenticationService` only exists for the original profile.
  ProfileIOS* profile = self.profile->GetOriginalProfile();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  id<SystemIdentity> identity =
      authService ? authService->GetPrimaryIdentity() : nil;

  __weak __typeof(self) weakSelf = self;
  if (!identity || _packagedLookupToken.length == 0 ||
      !self.baseViewController ||
      self.baseViewController.presentedViewController) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](__typeof(self) strongSelf) {
              [strongSelf
                  onEmbeddedReAuthCompletedWithResult:base::unexpected(nil)];
            },
            weakSelf));
    return;
  }

  std::unique_ptr<EmbeddedReAuthInteractionManager> interactionManager =
      ios::provider::CreateEmbeddedReAuthInteractionManager();
  if (!interactionManager || !interactionManager->IsReAuthSupported()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](__typeof(self) strongSelf) {
              [strongSelf
                  onEmbeddedReAuthCompletedWithResult:base::unexpected(nil)];
            },
            weakSelf));
    return;
  }

  _interactionManager = std::move(interactionManager);
  _interactionManager->InitiateEmbeddedReAuth(
      identity, _packagedLookupToken,
      EmbeddedReAuthTokenType::kPackagedLookupToken, self.baseViewController,
      base::BindOnce(
          [](__typeof(self) strongSelf,
             base::expected<NSString*, NSError*> result) {
            [strongSelf onEmbeddedReAuthCompletedWithResult:std::move(result)];
          },
          weakSelf));
}

- (void)stop {
  // TODO(crbug.com/434939275): Cancel the in-flight ReAuth flow and dismiss the
  // presented UI once supported by `EmbeddedReAuthInteractionManager`.
  _interactionManager.reset();
  if (_callback) {
    std::move(_callback).Run("");
  }
}

#pragma mark - Private

// Handles completion of the embedded ReAuth challenge.
- (void)onEmbeddedReAuthCompletedWithResult:
    (base::expected<NSString*, NSError*>)result {
  if (!_callback) {
    return;
  }
  _interactionManager.reset();

  webauthn::IOSPasskeyClient::EmbeddedReAuthCallback callback =
      std::move(_callback);
  std::string reauthProofToken =
      (result.has_value() && result.value().length > 0)
          ? base::SysNSStringToUTF8(result.value())
          : "";

  id<IOSPasskeyClientCommands> passkeyClientHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), IOSPasskeyClientCommands);
  // Do not access `self` after dismissing; the owner stops and releases this
  // coordinator.
  [passkeyClientHandler dismissPasskeyEmbeddedReAuth];

  std::move(callback).Run(std::move(reauthProofToken));
}

@end
