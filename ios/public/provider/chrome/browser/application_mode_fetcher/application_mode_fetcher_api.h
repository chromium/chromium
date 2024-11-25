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
// resume its original flow.
using AppModeFetchingCallback = base::OnceCallback<void(bool is_incognito)>;

// Fetches the application mode for a given `url` and `app_id`. The callback
// will be invoked asynchronously on the calling sequence
void FetchApplicationMode(const GURL& url,
                          NSString* app_id,
                          AppModeFetchingCallback callback);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APPLICATION_MODE_FETCHER_APPLICATION_MODE_FETCHER_API_H_
