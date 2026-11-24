// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_for_xcallback_url_context.h"

#import <UIKit/UIKit.h>

#import <map>
#import <memory>
#import <optional>
#import <string>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/ios/block_types.h"
#import "base/location.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/startup/app_group_command_payload.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/startup/app_startup_utils.h"
#import "ios/chrome/app/task_request_url_context_private.h"
#import "ios/chrome/app/task_request_utils.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_utils.h"
#import "ios/chrome/common/x_callback_url.h"
#import "ios/web/public/js_image_transcoder/java_script_image_transcoder.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// Format to which images from outside the app are transcoded before being
// searched with Lens, as in the legacy startup flow.
NSString* const kTranscodedImageType = @"image/jpeg";

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

}  // namespace

@implementation TaskRequestForXCallbackURLContext {
  // App Group command written by an extension, consumed from the shared
  // defaults at initialization so that every decision about this request
  // (account switch, metrics, command handling) uses the same snapshot. Empty
  // if the request isn't an App Group command.
  AppGroupCommandPayload _appGroupCommand;
  // Whether the request is an App Group command from the share extension.
  BOOL _isFromShareExtension;
  // Re-encodes the image to search with Lens, as it comes from outside the app.
  std::unique_ptr<web::JavaScriptImageTranscoder> _imageTranscoder;
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
  NSString* externalText = _appGroupCommand.external_text;
  ProfileIOS* profile = sceneState.profileState.profile;
  // Image searches don't map to a single tab, as how they open a tab depends on
  // the default search engine and may be asynchronous. Every other command
  // opens `tab`, unless it is `std::nullopt` and the command is discarded.
  std::optional<AppGroupCommandTab> tab;
  switch (*type) {
    case AppGroupCommandType::kSearchImage:
      [self searchAppGroupImageInMode:ApplicationModeForTabOpening::UNDETERMINED
                           sceneState:sceneState];
      return;
    case AppGroupCommandType::kIncognitoSearchImage:
      [self searchAppGroupImageInMode:ApplicationModeForTabOpening::INCOGNITO
                           sceneState:sceneState];
      return;
    case AppGroupCommandType::kOpenURL:
      tab = OpenURLTabForAppGroupCommand(
          externalText, ApplicationModeForTabOpening::UNDETERMINED);
      break;
    case AppGroupCommandType::kOpenURLInIncognito:
      tab = OpenURLTabForAppGroupCommand(
          externalText, ApplicationModeForTabOpening::INCOGNITO);
      break;
    case AppGroupCommandType::kSearchText:
      tab = SearchTextTabForAppGroupCommand(
          externalText, ApplicationModeForTabOpening::UNDETERMINED, profile);
      break;
    case AppGroupCommandType::kIncognitoSearchText:
      tab = SearchTextTabForAppGroupCommand(
          externalText, ApplicationModeForTabOpening::INCOGNITO, profile);
      break;
    case AppGroupCommandType::kVoiceSearch:
      tab = AppGroupCommandTab{GURL(kChromeUINewTabURL),
                               ApplicationModeForTabOpening::NORMAL,
                               TabOpeningPostOpeningAction::START_VOICE_SEARCH};
      break;
    case AppGroupCommandType::kNewTab:
      tab = AppGroupCommandTab{GURL(kChromeUINewTabURL),
                               ApplicationModeForTabOpening::NORMAL};
      break;
    case AppGroupCommandType::kFocusOmnibox:
      tab = AppGroupCommandTab{GURL(kChromeUINewTabURL),
                               ApplicationModeForTabOpening::NORMAL,
                               TabOpeningPostOpeningAction::FOCUS_OMNIBOX};
      break;
    case AppGroupCommandType::kIncognitoSearch:
      tab = AppGroupCommandTab{GURL(kChromeUINewTabURL),
                               ApplicationModeForTabOpening::INCOGNITO,
                               TabOpeningPostOpeningAction::FOCUS_OMNIBOX};
      break;
    case AppGroupCommandType::kQRScanner:
      tab = AppGroupCommandTab{
          GURL(kChromeUINewTabURL), ApplicationModeForTabOpening::NORMAL,
          TabOpeningPostOpeningAction::START_QR_CODE_SCANNER};
      break;
    case AppGroupCommandType::kLens:
      tab = AppGroupCommandTab{
          GURL(), ApplicationModeForTabOpening::NORMAL,
          TabOpeningPostOpeningAction::START_LENS_FROM_HOME_SCREEN_WIDGET};
      break;
    case AppGroupCommandType::kSearchPasswords:
      tab = AppGroupCommandTab{GURL(), ApplicationModeForTabOpening::NORMAL,
                               TabOpeningPostOpeningAction::SEARCH_PASSWORDS};
      break;
  }
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

// Searches the image sent by the extension in `targetMode` as the legacy
// startup flow does: with Lens when available, otherwise with the default
// search engine if it supports image search, otherwise by only opening the NTP.
// Commands without an image are discarded.
- (void)searchAppGroupImageInMode:(ApplicationModeForTabOpening)targetMode
                       sceneState:(SceneState*)sceneState {
  NSData* imageData = _appGroupCommand.external_data;
  if (!imageData) {
    return;
  }
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(
          sceneState.profileState.profile);
  if ([self shouldSearchImageWithLens:templateURLService]) {
    [self openNTPAndSearchImageWithLens:imageData
                             targetMode:targetMode
                             sceneState:sceneState];
    return;
  }
  if (!search_engines::SupportsSearchByImage(templateURLService)) {
    [self openImageSearchTabWithURLLoadParams:UrlLoadParams::InNewTab(
                                                  GURL(kChromeUINewTabURL))
                                   targetMode:targetMode
                        postOpeningCompletion:nil
                                   sceneState:sceneState];
    return;
  }
  // The block retains `self` until the image is resized for the search
  // request, while `sceneState` is captured weakly to avoid retaining a scene
  // that disconnects in the meantime.
  __weak SceneState* weakSceneState = sceneState;
  ImageSearchParamGenerator::PrepareImageDataFromDataAsync(
      imageData, base::BindOnce(^(NSData* resizedImageData) {
        [self openImageSearchResultsForResizedImageData:resizedImageData
                                             targetMode:targetMode
                                             sceneState:weakSceneState];
      }));
}

// Returns whether images must be searched with Lens. Logs the Lens
// availability, with the same entry point as the legacy startup flow.
- (BOOL)shouldSearchImageWithLens:(TemplateURLService*)templateURLService {
  return lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
      LensEntrypoint::ContextMenu,
      search_engines::SupportsSearchImageWithLens(templateURLService));
}

// Opens the NTP in `targetMode`, then searches `imageData` with Lens.
- (void)openNTPAndSearchImageWithLens:(NSData*)imageData
                           targetMode:(ApplicationModeForTabOpening)targetMode
                           sceneState:(SceneState*)sceneState {
  __weak SceneState* weakSceneState = sceneState;
  ProceduralBlock completion = ^{
    [self transcodeAndSearchImageWithLens:imageData sceneState:weakSceneState];
  };
  [self openImageSearchTabWithURLLoadParams:UrlLoadParams::InNewTab(
                                                GURL(kChromeUINewTabURL))
                                 targetMode:targetMode
                      postOpeningCompletion:completion
                                 sceneState:sceneState];
}

// LINT.IfChange(ShareExtensionLensImageSearch)
// Searches `imageData` with Lens in the current browser of `sceneState`, once
// it is transcoded, as it comes from outside the app and can't be trusted.
// `self` owns the transcoder and is retained by its callback until it replies,
// so that the search outlives the request execution. A request searches at
// most one image.
- (void)transcodeAndSearchImageWithLens:(NSData*)imageData
                             sceneState:(SceneState*)sceneState {
  CHECK(!_imageTranscoder);
  if (!sceneState) {
    return;
  }
  _imageTranscoder = std::make_unique<web::JavaScriptImageTranscoder>();
  __weak SceneState* weakSceneState = sceneState;
  // TODO(crbug.com/541315801): C2PA: Shared image could have C2PA metadata;
  // candidate to pass raw bytes.
  _imageTranscoder->TranscodeImage(
      imageData, kTranscodedImageType, nil, nil, nil,
      base::BindOnce(^(NSData* transcodedImageData, NSError* error) {
        [self searchTranscodedImageWithLens:error ? nil : transcodedImageData
                                 sceneState:weakSceneState];
      }));
}

// Searches the transcoded `imageData` with Lens in the current browser of
// `sceneState`. Does nothing if `imageData` is nil or can't be decoded, as
// Lens requires an image, or if the scene has no browser anymore.
- (void)searchTranscodedImageWithLens:(NSData*)imageData
                           sceneState:(SceneState*)sceneState {
  // This is called from the transcoder's callback, so the transcoder must not
  // be destroyed synchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(_imageTranscoder));
  if (!imageData) {
    return;
  }
  // TODO(crbug.com/541315801): C2PA: Shared image could have C2PA metadata;
  // candidate to pass raw bytes.
  UIImage* image = [UIImage imageWithData:imageData];
  Browser* browser = sceneState.browserProviderInterface.currentBrowser;
  if (!image || !browser) {
    return;
  }
  id<LensCommands> lensHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), LensCommands);
  SearchImageWithLensCommand* command = [[SearchImageWithLensCommand alloc]
      initWithImage:image
         entryPoint:LensEntrypoint::ContextMenu];
  [lensHandler searchImageWithLens:command];
}
// LINT.ThenChange(//ios/chrome/browser/shared/coordinator/scene/scene_controller.mm:ShareExtensionLensImageSearch)

// Opens the default search engine results for `resizedImageData`. Does nothing
// if the scene or its profile went away, or if the default search engine
// stopped supporting image search while the image was resized.
- (void)openImageSearchResultsForResizedImageData:(NSData*)resizedImageData
                                       targetMode:(ApplicationModeForTabOpening)
                                                      targetMode
                                       sceneState:(SceneState*)sceneState {
  ProfileIOS* profile = sceneState.profileState.profile;
  if (!profile) {
    // The scene was disconnected or its profile unloaded during the resize.
    return;
  }
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  if (!search_engines::SupportsSearchByImage(templateURLService)) {
    // The default search engine changed during the resize. Loading the
    // results would fail the checks of `LoadParamsForResizedImageData()`.
    return;
  }
  UrlLoadParams urlLoadParams =
      UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
  urlLoadParams.web_params =
      ImageSearchParamGenerator::LoadParamsForResizedImageData(
          resizedImageData, GURL(), templateURLService);
  [self openImageSearchTabWithURLLoadParams:urlLoadParams
                                 targetMode:targetMode
                      postOpeningCompletion:nil
                                 sceneState:sceneState];
}

// Opens a tab loading `urlLoadParams` in `targetMode` for an image search.
// Image searches are external requests, so they skip the new tab animation.
- (void)
    openImageSearchTabWithURLLoadParams:(UrlLoadParams)urlLoadParams
                             targetMode:(ApplicationModeForTabOpening)targetMode
                  postOpeningCompletion:(ProceduralBlock)postOpeningCompletion
                             sceneState:(SceneState*)sceneState {
  urlLoadParams.from_external = true;
  [self openTabWithSceneState:sceneState
                urlLoadParams:urlLoadParams
                   targetMode:targetMode
            postOpeningAction:TabOpeningPostOpeningAction::NO_ACTION
        postOpeningCompletion:postOpeningCompletion];
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
