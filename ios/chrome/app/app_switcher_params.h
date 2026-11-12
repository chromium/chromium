// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APP_SWITCHER_PARAMS_H_
#define IOS_CHROME_APP_APP_SWITCHER_PARAMS_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "base/time/time.h"
#import "ios/chrome/app/app_startup_parameters.h"

class GURL;

// Parameters fetched from the App Switcher for a URL opened from a caller app.
struct AppSwitcherParams {
  // Whether the caller app is allowed to change the application mode.
  bool can_change_application_mode = false;
  // Whether the App Switcher requested to open the URL in incognito.
  bool is_incognito = false;
  // The error returned by the fetch, if any.
  NSError* error = nil;
  // Whether the App Switcher requested to start an AI summarization.
  bool is_ai_summarization = false;
  // The hashed Gaia ID of the App Switcher user, set only when
  // `is_ai_summarization` is true.
  NSString* hashed_user_id = nil;
  // Whether the `IOS.AppModeFetching.*` metrics should be recorded once the
  // application mode is resolved. Only the application mode fetcher, used when
  // AI summarization is disabled, records them.
  bool should_record_app_mode_fetching = false;
  // The time at which the fetch started.
  base::TimeTicks fetch_start_time;
};

// Callback invoked with the fetched App Switcher parameters.
using AppSwitcherParamsCallback =
    base::OnceCallback<void(const AppSwitcherParams&)>;

// Returns whether App Switcher parameters must be fetched before opening a URL
// from `source_app_id`, given whether incognito mode is forced by policy.
bool ShouldFetchAppSwitcherParams(NSString* source_app_id,
                                  bool is_incognito_forced);

// Fetches App Switcher parameters for `url` opened from `source_app_id`,
// records the fetch metrics and invokes `callback` with the result once the
// fetch completes (possibly synchronously).
void RequestAppSwitcherParams(const GURL& url,
                              NSString* source_app_id,
                              AppSwitcherParamsCallback callback);

// Returns the application mode in which to open a tab given the fetched
// `params`, or `current_mode` if `params` doesn't change the mode.
ApplicationModeForTabOpening ApplicationModeForAppSwitcherParams(
    const AppSwitcherParams& params,
    ApplicationModeForTabOpening current_mode);

// Records the `IOS.AppModeFetching.*` metrics for `params`, if needed. Must be
// called only when the fetched application mode is applied (i.e. not forced by
// policy).
void RecordAppModeFetchingMetrics(const AppSwitcherParams& params);

#endif  // IOS_CHROME_APP_APP_SWITCHER_PARAMS_H_
