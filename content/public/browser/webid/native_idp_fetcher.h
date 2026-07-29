// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBID_NATIVE_IDP_FETCHER_H_
#define CONTENT_PUBLIC_BROWSER_WEBID_NATIVE_IDP_FETCHER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// An interface representing a native application Identity Provider (IDP)
// accessible via an alternative transport mechanism (such as Android Bound
// Services) rather than over HTTP.
class CONTENT_EXPORT NativeIdpFetcher {
 public:
  virtual ~NativeIdpFetcher() = default;

  // The error types that the fetcher may return when fetching.
  enum class FetchError {
    // The native app is not installed.
    kNoServiceFound,
    // The native app is installed but the fetcher cannot connect to it.
    kConnectionFailed,
    // The native app was successfully connected but encountered an error while
    // fetching.
    kFetchFailed,
  };

  // The string returned encodes a JSON payload. For FetchAccounts, this is
  // expected to be the same JSON format as the HTTP accounts endpoint response.
  using FetchResult = base::expected<std::string, FetchError>;
  using FetchCallback = base::OnceCallback<void(FetchResult)>;

  // Fetches accounts from the native IdP asynchronously.
  // Invokes `callback` with either the string response or an error. The
  // implementation should be equivalent to fetching `accounts_url` but doesn't
  // necessarily actually fetch the URL.
  virtual void FetchAccounts(const GURL& accounts_url,
                             FetchCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBID_NATIVE_IDP_FETCHER_H_
