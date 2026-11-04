// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_for_xcallback_url_context.h"

#import <UIKit/UIKit.h>

#import <map>
#import <string>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/app/startup/app_group_command_payload.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/startup/app_startup_utils.h"
#import "ios/chrome/app/task_request_url_context_private.h"
#import "ios/chrome/common/app_group/app_group_utils.h"
#import "ios/chrome/common/x_callback_url.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// Maps the `poa` query parameter of the `/open` command to the post-opening
// action to trigger once the tab is opened.
TabOpeningPostOpeningAction XCallbackPoaToPostOpeningAction(
    const std::string& poa_param) {
  if (poa_param == kPostOpeningActionDefaultBrowserSettings) {
    return TabOpeningPostOpeningAction::SHOW_DEFAULT_BROWSER_SETTINGS;
  }
  return TabOpeningPostOpeningAction::NO_ACTION;
}

}  // namespace

@implementation TaskRequestForXCallbackURLContext {
  // App Group command written by an extension, consumed from the shared
  // defaults at initialization so that every decision about this request
  // (account switch, metrics, command handling) uses the same snapshot. Empty
  // if the request isn't an App Group command.
  AppGroupCommandPayload _appGroupCommand;
  // Whether the request is an App Group command from the share extension.
  BOOL _isFromShareExtension;
}

- (instancetype)initWithURLContext:(UIOpenURLContext*)URLContext
                        sceneState:(SceneState*)sceneState
                       isColdStart:(BOOL)isColdStart {
  if ((self = [super initWithURLContext:URLContext
                             sceneState:sceneState
                            isColdStart:isColdStart])) {
    // Only requires a recent `command_time` (not `command` or `caller_app`),
    // which is more permissive than the validation needed to execute the
    // command. Must run before `ConsumeAppGroupCommandPayload()` removes it.
    _isFromShareExtension =
        app_group::IsShareExtensionCommandURL(URLContext.URL);
    // Must run after `[super initWithURLContext:...]`, which reads the App
    // Group command to decide whether to extract the `gaia_id`.
    if (IsXCallbackAppGroupCommandPath(URLContext.URL.path)) {
      _appGroupCommand = ConsumeAppGroupCommandPayload();
    }
  }
  return self;
}

- (BOOL)isFromShareExtension {
  return _isFromShareExtension;
}

- (NSString*)secureSourceApplication {
  return _appGroupCommand.is_valid ? _appGroupCommand.caller_app : nil;
}

- (void)recordMetrics {
  [super recordMetrics];

  base::UmaHistogramEnumeration(kAppLaunchSource, AppLaunchSource::X_CALLBACK);

  NSString* action = [self.URLContext.URL path];
  if (IsXCallbackAppGroupCommandPath(action)) {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_XCALLBACK_APPGROUP_COMMAND,
                              MOBILE_SESSION_START_ACTION_COUNT);
  } else if (IsXCallbackOpenCommandPath(action)) {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_XCALLBACK_OPEN,
                              MOBILE_SESSION_START_ACTION_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_XCALLBACK_OTHER,
                              MOBILE_SESSION_START_ACTION_COUNT);
  }
}

- (void)handleCommandWithSceneState:(SceneState*)sceneState {
  if (IsXCallbackOpenCommandPath([self.URLContext.URL path])) {
    [self handleOpenCommandWithSceneState:sceneState];
  }
  // TODO(crbug.com/493816082): Handle the `/app-group-command` command.
}

#pragma mark - Private

// Opens the http(s) URL passed in the `url` query parameter of the `/open`
// command. Requests with a missing or non-http(s) URL are discarded.
- (void)handleOpenCommandWithSceneState:(SceneState*)sceneState {
  std::map<std::string, std::string> parameters =
      ExtractQueryParametersFromXCallbackURL(
          net::GURLWithNSURL(self.URLContext.URL));
  GURL externalURL(parameters[kXCallbackOpenCommandURLParameter]);
  if (!externalURL.is_valid() || !externalURL.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  [self openTabWithSceneState:sceneState
                  externalURL:externalURL
                   virtualURL:GURL()
                   targetMode:ApplicationModeForTabOpening::UNDETERMINED
            postOpeningAction:XCallbackPoaToPostOpeningAction(
                                  parameters[kPostOpeningActionParameter])
        postOpeningCompletion:nil
             fromWidgetOrSiri:NO];
}

@end
