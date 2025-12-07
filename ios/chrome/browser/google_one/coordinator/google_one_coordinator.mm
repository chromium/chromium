// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google_one/coordinator/google_one_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/google_one_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/public/provider/chrome/browser/google_one/google_one_api.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// Constants to build the name of the outcome histogram.
const char kOutcomeHistogramPrefix[] = "IOS.GoogleOne.Outcome";
const char kSettingsHistogramSuffix[] = ".Settings";
const char kDriveHistogramSuffix[] = ".Drive";
const char kPhotosHistogramSuffix[] = ".Photos";

// Returns the correct suffix based on `entry_point`.
std::string HistogramSuffixForEntryPoint(GoogleOneEntryPoint entry_point) {
  switch (entry_point) {
    case GoogleOneEntryPoint::kSettings:
      return kSettingsHistogramSuffix;
    case GoogleOneEntryPoint::kSaveToDriveAlert:
      return kDriveHistogramSuffix;
    case GoogleOneEntryPoint::kSaveToPhotosAlert:
      return kPhotosHistogramSuffix;
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
  kMaxValue = kInvalidParameters
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

- (void)start {
  [super start];
  GoogleOneConfiguration* configuration = [[GoogleOneConfiguration alloc] init];
  configuration.entryPoint = _entryPoint;
  configuration.identity = _identity;
  __weak __typeof(self) weakSelf = self;
  configuration.flowDidEndWithErrorCallback =
      ^(GoogleOneOutcome outcome, NSError* error) {
        [weakSelf flowDidCompleteWithOutcome:outcome error:error];
      };
  configuration.openURLCallback = ^(NSURL* url) {
    [weakSelf openURL:url];
  };
  // There can be only one purchase flow in the application.
  _UIBlocker = std::make_unique<ScopedUIBlocker>(self.browser->GetSceneState(),
                                                 UIBlockerExtent::kApplication);
  _controller = ios::provider::CreateGoogleOneController(configuration);
  [_controller launchWithViewController:self.baseViewController completion:nil];
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

  [HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands)
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
