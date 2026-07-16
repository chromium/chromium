// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APP_SWITCHER_APP_SWITCHER_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APP_SWITCHER_APP_SWITCHER_API_H_

#import <Foundation/Foundation.h>

#import <string>
#import <string_view>

#import "base/functional/callback.h"
#import "url/gurl.h"

namespace ios::provider {

// Result of the app switcher parameters fetching.
struct AppSwitcherUrlOpeningResult {
  bool is_incognito = false;
  bool is_ai_summarization = false;
  std::string hashed_user_id;
  NSError* error = nil;
};

// Callback to run once the fetching is done.
using AppSwitcherResponseCallback =
    base::OnceCallback<void(AppSwitcherUrlOpeningResult result)>;

// Fetches the app switcher params for a given `url` and `app_id`. The callback
// response will be invoked asynchronously on the calling sequence.
void FetchAppSwitcherParams(const GURL& url,
                            std::string_view app_id,
                            AppSwitcherResponseCallback callback);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APP_SWITCHER_APP_SWITCHER_API_H_
