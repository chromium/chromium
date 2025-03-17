// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APPLICATION_MODE_FETCHER_APPLICATION_MODE_FETCHER_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APPLICATION_MODE_FETCHER_APPLICATION_MODE_FETCHER_API_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"
#import "url/gurl.h"

namespace ios::provider {

// Callback to run once the fetching is done. `is_incognito` is false doesn't
// mean that the URL will open in regular mode, it means that chrome should
// resume its original flow. When an error occurs, depending on the current
// flag, the original flow will resume or the incognito interstitial will be
// presented.
using AppModeFetchingResponse =
    base::OnceCallback<void(bool is_incognito, NSError* error)>;

// Fetches the application mode for a given `url` and `app_id`. The callback
// response will be invoked asynchronously on the calling sequence
void FetchApplicationMode(const GURL& url,
                          NSString* app_id,
                          AppModeFetchingResponse fetching_response);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APPLICATION_MODE_FETCHER_APPLICATION_MODE_FETCHER_API_H_
