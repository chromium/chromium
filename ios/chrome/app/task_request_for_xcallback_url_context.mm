// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_for_xcallback_url_context.h"

#import <UIKit/UIKit.h>

#import <map>
#import <optional>
#import <string>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/startup/app_group_command_payload.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/startup/app_startup_utils.h"
#import "ios/chrome/app/task_request_url_context_private.h"
#import "ios/chrome/app/task_request_utils.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_utils.h"
#import "ios/chrome/common/x_callback_url.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// Commands that a Chrome extension can request through the App Group.
enum class AppGroupCommandType {
  kOpenURL,
  kOpenURLInIncognito,
  kSearchText,
  kIncognitoSearchText,
  kSearchImage,
  kIncognitoSearchImage,
  kVoiceSearch,
  kNewTab,
  kFocusOmnibox,
  kIncognitoSearch,
  kQRScanner,
  kLens,
  kSearchPasswords,
};

// Maps the `poa` query parameter of the `/open` command to the post-opening
// action to trigger once the tab is opened.
TabOpeningPostOpeningAction XCallbackPoaToPostOpeningAction(
    const std::string& poa_param) {
  if (poa_param == kPostOpeningActionDefaultBrowserSettings) {
    return TabOpeningPostOpeningAction::SHOW_DEFAULT_BROWSER_SETTINGS;
  }
  return TabOpeningPostOpeningAction::NO_ACTION;
}

// Returns the type of the App Group `command`, or `std::nullopt` if unknown.
std::optional<AppGroupCommandType> AppGroupCommandTypeFromString(
    NSString* command) {
  const struct {
    NSString* name;
    AppGroupCommandType type;
  } kCommandTypes[] = {
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupOpenURLCommand),
       AppGroupCommandType::kOpenURL},
      {app_group::kChromeAppGroupOpenURLInIcognitoCommand,
       AppGroupCommandType::kOpenURLInIncognito},
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupSearchTextCommand),
       AppGroupCommandType::kSearchText},
      {app_group::kChromeAppGroupIncognitoSearchTextCommand,
       AppGroupCommandType::kIncognitoSearchText},
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupSearchImageCommand),
       AppGroupCommandType::kSearchImage},
      {app_group::kChromeAppGroupIncognitoSearchImageCommand,
       AppGroupCommandType::kIncognitoSearchImage},
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupVoiceSearchCommand),
       AppGroupCommandType::kVoiceSearch},
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupNewTabCommand),
       AppGroupCommandType::kNewTab},
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupFocusOmniboxCommand),
       AppGroupCommandType::kFocusOmnibox},
      {base::SysUTF8ToNSString(
           app_group::kChromeAppGroupIncognitoSearchCommand),
       AppGroupCommandType::kIncognitoSearch},
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupQRScannerCommand),
       AppGroupCommandType::kQRScanner},
      {base::SysUTF8ToNSString(app_group::kChromeAppGroupLensCommand),
       AppGroupCommandType::kLens},
      {base::SysUTF8ToNSString(
           app_group::kChromeAppGroupSearchPasswordsCommand),
       AppGroupCommandType::kSearchPasswords},
  };
  for (const auto& command_type : kCommandTypes) {
    if ([command isEqualToString:command_type.name]) {
      return command_type.type;
    }
  }
  return std::nullopt;
}

// Returns the http(s) URL sent in the `external_text` of an App Group command,
// or an empty URL if it is missing or has another scheme.
GURL HTTPURLFromAppGroupText(NSString* external_text) {
  GURL url(base::SysNSStringToUTF8(external_text));
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return GURL();
  }
  return url;
}

// Tab to open to execute an App Group command.
struct AppGroupCommandTab {
  GURL url;
  ApplicationModeForTabOpening target_mode;
  TabOpeningPostOpeningAction post_opening_action =
      TabOpeningPostOpeningAction::NO_ACTION;
};

// Returns the tab opening the http(s) URL sent in `external_text` in
// `target_mode`, or `std::nullopt` if there is no such URL.
std::optional<AppGroupCommandTab> OpenURLTabForAppGroupCommand(
    NSString* external_text,
    ApplicationModeForTabOpening target_mode) {
  GURL url = HTTPURLFromAppGroupText(external_text);
  if (url.is_empty()) {
    return std::nullopt;
  }
  return AppGroupCommandTab{url, target_mode};
}

// Returns the tab searching `external_text` in `target_mode` with the default
// search engine of `profile`, or `std::nullopt` if there is no text. An empty
// text is still searched. Opens the NTP if `profile` has no valid default
// search engine (e.g. disabled by policy).
std::optional<AppGroupCommandTab> SearchTextTabForAppGroupCommand(
    NSString* external_text,
    ApplicationModeForTabOpening target_mode,
    ProfileIOS* profile) {
  if (!external_text) {
    return std::nullopt;
  }
  // Incognito uses the default search engine of its original profile, so
  // `profile` is used in both modes.
  const GURL url = GenerateResultGURLFromSearchQuery(external_text, profile);
  return AppGroupCommandTab{
      .url = url.is_valid() ? url : GURL(kChromeUINewTabURL),
      .target_mode = target_mode};
}

// Returns the tab to open in `profile` for a command of `type` carrying
// `external_text`, or `std::nullopt` if the command must be discarded.
std::optional<AppGroupCommandTab> TabForAppGroupCommand(
    AppGroupCommandType type,
    NSString* external_text,
    ProfileIOS* profile) {
  switch (type) {
    case AppGroupCommandType::kOpenURL:
      return OpenURLTabForAppGroupCommand(
          external_text, ApplicationModeForTabOpening::UNDETERMINED);
    case AppGroupCommandType::kOpenURLInIncognito:
      return OpenURLTabForAppGroupCommand(
          external_text, ApplicationModeForTabOpening::INCOGNITO);
    case AppGroupCommandType::kSearchText:
      return SearchTextTabForAppGroupCommand(
          external_text, ApplicationModeForTabOpening::UNDETERMINED, profile);
    case AppGroupCommandType::kIncognitoSearchText:
      return SearchTextTabForAppGroupCommand(
          external_text, ApplicationModeForTabOpening::INCOGNITO, profile);
    case AppGroupCommandType::kSearchImage:
    case AppGroupCommandType::kIncognitoSearchImage:
      // TODO(crbug.com/493816082): Search the image sent by the extension,
      // discarding commands without one, as the legacy startup flow does.
      return std::nullopt;
    case AppGroupCommandType::kVoiceSearch:
      return AppGroupCommandTab{
          GURL(kChromeUINewTabURL), ApplicationModeForTabOpening::NORMAL,
          TabOpeningPostOpeningAction::START_VOICE_SEARCH};
    case AppGroupCommandType::kNewTab:
      return AppGroupCommandTab{GURL(kChromeUINewTabURL),
                                ApplicationModeForTabOpening::NORMAL};
    case AppGroupCommandType::kFocusOmnibox:
      return AppGroupCommandTab{GURL(kChromeUINewTabURL),
                                ApplicationModeForTabOpening::NORMAL,
                                TabOpeningPostOpeningAction::FOCUS_OMNIBOX};
    case AppGroupCommandType::kIncognitoSearch:
      return AppGroupCommandTab{GURL(kChromeUINewTabURL),
                                ApplicationModeForTabOpening::INCOGNITO,
                                TabOpeningPostOpeningAction::FOCUS_OMNIBOX};
    case AppGroupCommandType::kQRScanner:
      return AppGroupCommandTab{
          GURL(kChromeUINewTabURL), ApplicationModeForTabOpening::NORMAL,
          TabOpeningPostOpeningAction::START_QR_CODE_SCANNER};
    case AppGroupCommandType::kLens:
      return AppGroupCommandTab{
          GURL(), ApplicationModeForTabOpening::NORMAL,
          TabOpeningPostOpeningAction::START_LENS_FROM_HOME_SCREEN_WIDGET};
    case AppGroupCommandType::kSearchPasswords:
      return AppGroupCommandTab{GURL(), ApplicationModeForTabOpening::NORMAL,
                                TabOpeningPostOpeningAction::SEARCH_PASSWORDS};
  }
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
  NSString* action = [self.URLContext.URL path];
  if (IsXCallbackOpenCommandPath(action)) {
    [self handleOpenCommandWithSceneState:sceneState];
  } else if (IsXCallbackAppGroupCommandPath(action)) {
    [self handleAppGroupCommandWithSceneState:sceneState];
  }
}

#pragma mark - Private

// Executes the App Group command consumed at initialization. Invalid, unknown
// or incomplete commands are discarded.
- (void)handleAppGroupCommandWithSceneState:(SceneState*)sceneState {
  if (!_appGroupCommand.is_valid) {
    return;
  }
  std::optional<AppGroupCommandType> type =
      AppGroupCommandTypeFromString(_appGroupCommand.command);
  if (!type) {
    return;
  }
  std::optional<AppGroupCommandTab> tab = TabForAppGroupCommand(
      *type, _appGroupCommand.external_text, sceneState.profileState.profile);
  if (!tab) {
    return;
  }

  UrlLoadParams urlLoadParams = UrlLoadParams::InNewTab(tab->url);
  [self openTabWithSceneState:sceneState
                urlLoadParams:urlLoadParams
                   targetMode:tab->target_mode
            postOpeningAction:tab->post_opening_action
        postOpeningCompletion:nil];
}

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
                urlLoadParams:UrlLoadParams::InNewTab(externalURL)
                   targetMode:ApplicationModeForTabOpening::UNDETERMINED
            postOpeningAction:XCallbackPoaToPostOpeningAction(
                                  parameters[kPostOpeningActionParameter])
        postOpeningCompletion:nil];
}

@end
