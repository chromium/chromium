// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_startup_parameters.h"

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/startup/app_startup_utils.h"
#import "ios/chrome/browser/app_switcher/metrics/app_switcher_metrics.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/app_switcher/app_switcher_api.h"
#import "ios/public/provider/chrome/browser/application_mode_fetcher/application_mode_fetcher_api.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

namespace {

// This enum is used for Histogram. Items should not be removed or reordered and
// this enum should be kept synced with `IOSAppModeFetchingOutcomeType` in
// histograms.xml. The four states correspond to:
// - APP_MODE_FETCHED_INCOGNITO: The application mode fetching response was
// successful and the fetched app mode is incognito.
// - APP_MODE_FETCHED_NON_INCOGNITO: The application mode fetching response was
// successful and the fetched app mode is non-incognito.
// - APP_MODE_FETCHED_FAILURE: The application mode fetching results in an error
// (different from a time out error)
// - APP_MODE_FETCHED_TIME_OUT: The application mode fetching results in a time
// out error.
enum class AppModeFetchingOutcomeType {
  kIncognito,
  kNonIncognito,
  kFailure,
  kTimeOut,
  kMaxValue = kTimeOut
};

// Returns a `AppSwitcherParamsRequestStatus` based on the source `app_ID`, the
// `AppSwitcherParamsRequestStatus` and if the mode is forced or not.
AppSwitcherParamsRequestStatus AppSwitcherParamsAvailability(
    NSString* app_id,
    ApplicationModeForTabOpening mode,
    bool application_mode_forced) {
  if (IsAppSwitcherAISummarizationEnabled()) {
    if (IsCallerAppAllowListedForAISummarization(app_id)) {
      return AppSwitcherParamsRequestStatus::kUnavailable;
    }
  }

  if (IsCallerAppAllowListedForApplicationMode(app_id)) {
    if (!(application_mode_forced &&
          mode == ApplicationModeForTabOpening::INCOGNITO)) {
      return AppSwitcherParamsRequestStatus::kUnavailable;
    }
  }

  return AppSwitcherParamsRequestStatus::kAvailable;
}

}  // namespace

@implementation AppStartupParameters {
  GURL _externalURL;
  GURL _completeURL;
  std::vector<GURL> _URLs;
  AppSwitcherParamsRequestStatus _appSwitcherParamsRequestStatus;
  NSString* _sourceAppID;

  // The mode in which the tab must be opened. Defaults to UNDETERMINED.
  ApplicationModeForTabOpening _applicationMode;

  // Whether the application mode is forced or not (for example incognito mode
  // or regular mode were forced based on the profile prefs).
  BOOL _forceApplicationMode;

  // An array of blocks to execute once the `applicationMode` is available.
  NSMutableArray<AppSwitcherParamsRequestBlock>* _pendingBlocks;
}

@synthesize inputURLs = _inputURLs;
@synthesize postOpeningAction = _postOpeningAction;
@synthesize textQuery = _textQuery;

- (const GURL&)externalURL {
  return _externalURL;
}

- (const GURL&)completeURL {
  return _completeURL;
}

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                        completeURL:(const GURL&)completeURL
                    applicationMode:(ApplicationModeForTabOpening)mode
               forceApplicationMode:(BOOL)forceApplicationMode {
  self = [super init];
  if (self) {
    _externalURL = externalURL;
    _completeURL = completeURL;
    _applicationMode = mode;
    _appSwitcherParamsRequestStatus =
        AppSwitcherParamsRequestStatus::kAvailable;
    _forceApplicationMode = forceApplicationMode;
  }
  return self;
}

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                        completeURL:(const GURL&)completeURL
                        sourceAppID:(NSString*)sourceAppID
                    applicationMode:(ApplicationModeForTabOpening)mode
               forceApplicationMode:(BOOL)forceApplicationMode {
  self = [super init];
  if (self) {
    _externalURL = externalURL;
    _completeURL = completeURL;
    _sourceAppID = [sourceAppID copy];
    _applicationMode = mode;
    _appSwitcherParamsRequestStatus =
        AppSwitcherParamsAvailability(sourceAppID, mode, forceApplicationMode);
    _forceApplicationMode = forceApplicationMode;
  }
  return self;
}

- (instancetype)initWithURLs:(const std::vector<GURL>&)URLs
             applicationMode:(ApplicationModeForTabOpening)mode
        forceApplicationMode:(BOOL)forceApplicationMode {
  if (URLs.empty()) {
    self = [self initWithExternalURL:GURL(kChromeUINewTabURL)
                         completeURL:GURL(kChromeUINewTabURL)
                     applicationMode:mode
                forceApplicationMode:forceApplicationMode];
  } else {
    self = [self initWithExternalURL:URLs.front()
                         completeURL:URLs.front()
                     applicationMode:mode
                forceApplicationMode:forceApplicationMode];
  }

  if (self) {
    _URLs = URLs;
  }
  return self;
}

- (NSString*)description {
  NSMutableString* description =
      [NSMutableString stringWithFormat:@"AppStartupParameters: %s",
                                        _externalURL.spec().c_str()];
  if (_applicationMode == ApplicationModeForTabOpening::INCOGNITO) {
    [description appendString:@", should launch in incognito"];
  }

  switch (self.postOpeningAction) {
    case START_QR_CODE_SCANNER:
      [description appendString:@", should launch QR scanner"];
      break;
    case START_LENS_FROM_APP_ICON_LONG_PRESS:
    case START_LENS_FROM_HOME_SCREEN_WIDGET:
    case START_LENS_FROM_SPOTLIGHT:
    case START_LENS_FROM_INTENTS:
      [description appendString:@", should launch Lens"];
      break;
    case START_VOICE_SEARCH:
      [description appendString:@", should launch voice search"];
      break;
    case FOCUS_OMNIBOX:
      [description appendString:@", should focus omnibox"];
      break;
    case OPEN_READING_LIST:
      [description appendString:@", should open reading list"];
      break;
    case OPEN_BOOKMARKS:
      [description appendString:@", should open bookmarks"];
      break;
    case OPEN_RECENT_TABS:
      [description appendString:@", should open recent tabs"];
      break;
    case OPEN_TAB_GRID:
      [description appendString:@", should open tab grid"];
      break;
    case SET_CHROME_DEFAULT_BROWSER:
      [description appendString:@", should open set chrome default browser"];
      break;
    case VIEW_HISTORY:
      [description appendString:@", should open history"];
      break;
    case OPEN_PAYMENT_METHODS:
      [description appendString:@", should open payment methods"];
      break;
    case RUN_SAFETY_CHECK:
      [description appendString:@", should run safety check"];
      break;
    case MANAGE_PASSWORDS:
      [description appendString:@", should open manage passwords setting page"];
      break;
    case MANAGE_SETTINGS:
      [description appendString:@", should open settings page"];
      break;
    case OPEN_LATEST_TAB:
      [description appendString:@", should resume latest tab"];
      break;
    case OPEN_CLEAR_BROWSING_DATA_DIALOG:
      [description appendString:@", should open Clear Browsing Data dialog"];
      break;
    case ADD_BOOKMARKS:
      [description appendString:@", should add bookmarks"];
      break;
    case ADD_READING_LIST_ITEMS:
      [description appendString:@", should add reading list items"];
      break;
    case SHOW_GOOGLE_ONE_SCREEN:
      [description appendString:@", should show Google One screen"];
      break;
    case START_GEMINI_AI_SUMMARIZATION:
      [description appendString:@", should launch Gemini AI summarization"];
      break;
    default:
      break;
  }

  return description;
}

- (void)setPostOpeningAction:(TabOpeningPostOpeningAction)action {
  DCHECK([self isValidPostOpeningAction:action]);
  _postOpeningAction = action;
}

#pragma mark - Private methods

- (BOOL)isValidPostOpeningAction:(TabOpeningPostOpeningAction)action {
  switch (action) {
      // Post opening actions that are allowed on any URL.
    case NO_ACTION:
    case SHOW_DEFAULT_BROWSER_SETTINGS:
    case EXTERNAL_ACTION_SHOW_BROWSER_SETTINGS:
    case SEARCH_PASSWORDS:
    case CREDENTIAL_EXCHANGE_IMPORT:
    case TRIGGER_GEMINI_PROMO:
    case START_GEMINI_AI_SUMMARIZATION:
      return YES;

      // Lens action are valid on empty URLs, in addition to
      // the URLs where all actions are valid.
    case START_LENS_FROM_APP_ICON_LONG_PRESS:
    case START_LENS_FROM_HOME_SCREEN_WIDGET:
    case START_LENS_FROM_SPOTLIGHT:
    case OPEN_LATEST_TAB:
    case START_LENS_FROM_INTENTS:
    case SHOW_GOOGLE_ONE_SCREEN:
      if (_externalURL.is_empty()) {
        return YES;
      }
      [[fallthrough]];

      // Other actions are only valid on NTP;
    default:
      return _externalURL == GURL(kChromeUINewTabURL);
  }
}

- (void)fetchAppSwitcherParamsWithBlock:(AppSwitcherParamsRequestBlock)block {
  switch (_appSwitcherParamsRequestStatus) {
    case AppSwitcherParamsRequestStatus::kAvailable:
      CHECK(!_pendingBlocks);
      block(_applicationMode);
      break;
    case AppSwitcherParamsRequestStatus::kRequested:
      CHECK(_pendingBlocks);
      [_pendingBlocks addObject:block];
      break;
    case AppSwitcherParamsRequestStatus::kUnavailable: {
      CHECK(!_pendingBlocks);
      _pendingBlocks = [[NSMutableArray alloc] init];
      [_pendingBlocks addObject:block];
      _appSwitcherParamsRequestStatus =
          AppSwitcherParamsRequestStatus::kRequested;
      __weak __typeof(self) weakSelf = self;
      base::TimeTicks startFetchTime = base::TimeTicks::Now();

      if (IsAppSwitcherAISummarizationEnabled()) {
        auto fetching_response = base::BindOnce(
            ^(ios::provider::AppSwitcherUrlOpeningResult result) {
              [weakSelf handleAppSwitcherParamsRequest:result
                                        startFetchTime:startFetchTime];
            });
        ios::provider::FetchAppSwitcherParams(
            _externalURL, base::SysNSStringToUTF8(_sourceAppID),
            std::move(fetching_response));
      } else {
        auto fetching_response = base::BindOnce(
            ^(bool isAppSwitcherIncognito, NSError* error) {
              [weakSelf handleApplicationModeRequest:isAppSwitcherIncognito
                                                    error:error
                                           startFetchTime:startFetchTime];
            });
        ios::provider::FetchApplicationMode(_externalURL, _sourceAppID,
                                            std::move(fetching_response));
      }
      break;
    }
  }
}

// Handles the callback from the App Switcher parameters fetcher.
- (void)handleAppSwitcherParamsRequest:
            (ios::provider::AppSwitcherUrlOpeningResult)result
                        startFetchTime:(base::TimeTicks)startFetchTime {
  _appSwitcherParamsRequestStatus = AppSwitcherParamsRequestStatus::kAvailable;

  BOOL success = !result.error;
  RecordAppSwitcherFetchOutcome(success);
  RecordAppSwitcherFetchDuration(base::TimeTicks::Now() - startFetchTime);

  if (IsAppSwitcherAISummarizationEnabled()) {
    if (IsCallerAppAllowListedForAISummarization(_sourceAppID)) {
      if (success && result.is_ai_summarization) {
        self.postOpeningAction = START_GEMINI_AI_SUMMARIZATION;
        self.appSwitcherHashedUserID =
            base::SysUTF8ToNSString(result.hashed_user_id);
        RecordAppSwitcherAISummarizationEntrypoint();
      }
    }
  }

  if (IsCallerAppAllowListedForApplicationMode(_sourceAppID)) {
    [self handleApplicationModeRequest:result.is_incognito
                                 error:result.error
                        startFetchTime:startFetchTime];
  }

  for (AppSwitcherParamsRequestBlock block in _pendingBlocks) {
    block(_applicationMode);
  }
  _pendingBlocks = nil;
}

- (void)setApplicationMode:(ApplicationModeForTabOpening)applicationMode
      forceApplicationMode:(BOOL)forceApplicationMode {
  if (forceApplicationMode) {
    if (applicationMode == ApplicationModeForTabOpening::INCOGNITO) {
      self.unexpectedMode =
          _applicationMode == ApplicationModeForTabOpening::NORMAL;

    } else if (applicationMode == ApplicationModeForTabOpening::NORMAL) {
      self.unexpectedMode =
          _applicationMode == ApplicationModeForTabOpening::INCOGNITO;
    }
  }
  _applicationMode = applicationMode;
  _forceApplicationMode = forceApplicationMode;
}

- (ApplicationModeForTabOpening)applicationMode {
  // TODO(crbug.com/465336545): Remove this getter once the
  // `kChromeStartupParametersAsync` is fully removed.
  return _applicationMode;
}

- (void)handleApplicationModeRequest:(BOOL)isAppSwitcherIncognito
                               error:(NSError*)error
                      startFetchTime:(base::TimeTicks)startFetchTime {
  _appSwitcherParamsRequestStatus = AppSwitcherParamsRequestStatus::kAvailable;
  // If the application mode was forced, ignore the fetched application mode
  // result. When `IsAppSwitcherAISummarizationEnabled()` is enabled,
  // `handleAppSwitcherParamsRequest:` handles executing pending blocks.
  if (_forceApplicationMode) {
    if (!IsAppSwitcherAISummarizationEnabled()) {
      for (AppSwitcherParamsRequestBlock block in _pendingBlocks) {
        block(_applicationMode);
      }
      _pendingBlocks = nil;
    }
    return;
  }

  AppModeFetchingOutcomeType outcome =
      AppModeFetchingOutcomeType::kNonIncognito;
  if (isAppSwitcherIncognito) {
    // When the `applicationMode` needs changing the error associated to the
    // response must be nil.
    CHECK(!error);
    _applicationMode = ApplicationModeForTabOpening::APP_SWITCHER_INCOGNITO;
    outcome = AppModeFetchingOutcomeType::kIncognito;
  } else {
    if (error) {
      _applicationMode =
          ApplicationModeForTabOpening::APP_SWITCHER_UNDETERMINED;
      outcome = [error.domain isEqualToString:@"AppSwitcherTimeoutError"]
                    ? AppModeFetchingOutcomeType::kTimeOut
                    : AppModeFetchingOutcomeType::kFailure;
    }
  }

  if (!IsAppSwitcherAISummarizationEnabled()) {
    for (AppSwitcherParamsRequestBlock block in _pendingBlocks) {
      block(_applicationMode);
    }
    _pendingBlocks = nil;
    base::UmaHistogramEnumeration("IOS.AppModeFetching.Outcome", outcome);
    base::UmaHistogramTimes("IOS.AppModeFetching.Duration",
                            base::TimeTicks::Now() - startFetchTime);
  }
}

@end
