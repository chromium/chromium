// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google_one/coordinator/google_one_coordinator.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/google_one/shared/google_one_deep_link_util.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/google_one_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/public/provider/chrome/browser/google_one/google_one_api.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// Constants to build the name of the outcome histogram.
const char kOutcomeHistogramPrefix[] = "IOS.GoogleOne.Outcome";
const char kSettingsHistogramSuffix[] = ".Settings";
const char kDriveHistogramSuffix[] = ".Drive";
const char kPhotosHistogramSuffix[] = ".Photos";
const char kDeepLinkHistogramSuffix[] = ".DeepLink";

// Returns the correct suffix based on `entry_point`.
std::string HistogramSuffixForEntryPoint(GoogleOneEntryPoint entry_point) {
  switch (entry_point) {
    case GoogleOneEntryPoint::kSettings:
      return kSettingsHistogramSuffix;
    case GoogleOneEntryPoint::kSaveToDriveAlert:
      return kDriveHistogramSuffix;
    case GoogleOneEntryPoint::kSaveToPhotosAlert:
      return kPhotosHistogramSuffix;
    case GoogleOneEntryPoint::kDeepLink:
      return kDeepLinkHistogramSuffix;
  }
}

// Returns the correct histogram based on `entry_point`.
std::string HistogramForEntryPoint(GoogleOneEntryPoint entry_point) {
  return kOutcomeHistogramPrefix + HistogramSuffixForEntryPoint(entry_point);
}

// An histogram value for the different outcomes of the GoogleOne flow, based
// on the outcome returned by the library and the internal state of the feature.
// LINT.IfChange(GoogleOneOutcome)
enum class GoogleOneOutcomeMetrics {
  kSuccess = 0,
  kUnknownFailure = 1,
  kUnspecifiedFailure = 2,
  kInterrupted = 3,
  kInterruptedByOpeningURL = 4,
  kInterruptedByUser = 5,
  kAlreadyPresented = 6,
  kPurchaseFailed = 7,
  kUserWillLeaveApp = 8,
  kLaunchFailed = 9,
  kInvalidParameters = 10,
  kInvalidParametersFallbackToOpeningURLInNewTab = 11,
  kMaxValue = kInvalidParametersFallbackToOpeningURLInNewTab
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:GoogleOneOutcome)

// Returns the correct histogram bucket based on the outcome returned by the
// library and the internal state of the feature.
GoogleOneOutcomeMetrics HistogramOutcomeBucket(GoogleOneOutcome outcome,
                                               BOOL opened_url,
                                               BOOL stopped) {
  switch (outcome) {
    case GoogleOneOutcome::kGoogleOneEntryOutcomeNoError:
      return GoogleOneOutcomeMetrics::kSuccess;
    case GoogleOneOutcome::kGoogleOneEntryOutcomeUnknownError:
      return GoogleOneOutcomeMetrics::kUnknownFailure;
    case GoogleOneOutcome::kGoogleOneEntryOutcomeErrorUnspecified:
      return GoogleOneOutcomeMetrics::kUnspecifiedFailure;
    case GoogleOneOutcome::kGoogleOneEntryOutcomePurchaseCancelled:
      if (opened_url) {
        return GoogleOneOutcomeMetrics::kInterruptedByOpeningURL;
      }
      if (stopped) {
        return GoogleOneOutcomeMetrics::kInterrupted;
      }
      return GoogleOneOutcomeMetrics::kInterruptedByUser;
    case GoogleOneOutcome::kGoogleOneEntryOutcomeAlreadyPresented:
      return GoogleOneOutcomeMetrics::kAlreadyPresented;
    case GoogleOneOutcome::kGoogleOneEntryOutcomePurchaseFailed:
      return GoogleOneOutcomeMetrics::kPurchaseFailed;
    case GoogleOneOutcome::kGoogleOneEntryOutcomeWillLeaveApp:
      return GoogleOneOutcomeMetrics::kUserWillLeaveApp;
    case GoogleOneOutcome::kGoogleOneEntryOutcomeLaunchFailed:
      return GoogleOneOutcomeMetrics::kLaunchFailed;
    case GoogleOneOutcome::kGoogleOneEntryOutcomeInvalidParameters:
      return GoogleOneOutcomeMetrics::kInvalidParameters;
  }
}

}  // namespace

@implementation GoogleOneCoordinator {
  GoogleOneEntryPoint _entryPoint;
  id<GoogleOneController> _controller;
  id<SystemIdentity> _identity;
  GURL _inputURL;
  // UI blocker used while the there is only one buying flow at the time on
  // any window.
  std::unique_ptr<ScopedUIBlocker> _UIBlocker;
  // Whether the internal controller has been stopped.
  BOOL _controllerStopped;
  // Whether this coordinator has been stopped, either from the controller or
  // by an external source.
  BOOL _stopped;
  // Whether a URL has been opened from the library.
  BOOL _openedURL;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entryPoint:(GoogleOneEntryPoint)entryPoint
                                  identity:(id<SystemIdentity>)identity {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entryPoint = entryPoint;
    CHECK(identity);
    _identity = identity;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entryPoint:(GoogleOneEntryPoint)entryPoint
                                  inputURL:(const GURL&)inputURL {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK(inputURL.is_valid());
    _entryPoint = entryPoint;
    _inputURL = inputURL;
  }
  return self;
}

- (void)start {
  [super start];
  id<SystemIdentity> identityToUse = _identity;
  if (_inputURL.is_valid()) {
    NSString* accountParam = GoogleOneAccountFromURL(_inputURL);
    if (accountParam.length > 0) {
      identityToUse = FindIdentityForGoogleOneAccount(accountParam);
    }
    if (!identityToUse) {
      AuthenticationService* authService =
          AuthenticationServiceFactory::GetForProfile(
              self.browser->GetProfile());
      identityToUse = authService->GetPrimaryIdentity();
    }
  }

  // If the user is not signed in and there is no valid account specified in the
  // URL, there is no account to show settings for. Cancel the action.
  if (!identityToUse) {
    [self flowDidCompleteWithOutcome:GoogleOneOutcome::
                                         kGoogleOneEntryOutcomeInvalidParameters
                               error:nil];
    return;
  }

  GoogleOneConfiguration* configuration = [[GoogleOneConfiguration alloc] init];
  configuration.entryPoint = _entryPoint;
  configuration.identity = identityToUse;

  __weak __typeof(self) weakSelf = self;
  configuration.flowDidEndWithErrorCallback =
      ^(GoogleOneOutcome outcome, NSError* error) {
        [weakSelf flowDidCompleteWithOutcome:outcome error:error];
      };
  configuration.openURLCallback = ^(NSURL* url) {
    [weakSelf openURL:url];
  };
  // There can be only one purchase flow in the application.
  SceneState* sceneState = self.browser->GetSceneState();
  _UIBlocker =
      ScopedUIBlocker::AppScoped(sceneState, sceneState.profileState.appState);
  _controller = ios::provider::CreateGoogleOneController(configuration);
  if (_inputURL.is_valid()) {
    TriggerAccountSwitchSnackbarWithIdentity(identityToUse, self.browser);
    [_controller launchWithViewController:self.baseViewController
                                      URL:net::NSURLWithGURL(_inputURL)
                               completion:nil];
  } else {
    [_controller launchWithViewController:self.baseViewController
                               completion:nil];
  }
}

- (void)stop {
  if (_stopped) {
    return;
  }
  _stopped = YES;
  if (!_controllerStopped) {
    [_controller stop];
  }
  _UIBlocker.reset();
  [super stop];
}

#pragma mark - Private

- (void)openURL:(NSURL*)url {
  Browser* browser = self.browser;
  if (!browser) {
    return;
  }
  _openedURL = YES;
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:net::GURLWithNSURL(url)
                   inIncognito:browser->GetProfile()->IsOffTheRecord()];

  [HandlerForProtocol(browser->GetCommandDispatcher(), SceneCommands)
      openURLInNewTab:command];
}

- (void)flowDidCompleteWithOutcome:(GoogleOneOutcome)outcome
                             error:(NSError*)error {
  Browser* browser = self.browser;
  if (!browser) {
    return;
  }
  base::UmaHistogramEnumeration(
      HistogramForEntryPoint(_entryPoint),
      HistogramOutcomeBucket(outcome, _openedURL, _stopped));
  _controllerStopped = YES;
  if (!_stopped) {
    [HandlerForProtocol(browser->GetCommandDispatcher(), GoogleOneCommands)
        hideGoogleOne];
  }
}

@end
