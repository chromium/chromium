// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBID_NATIVE_IDP_FETCHER_H_
#define CONTENT_PUBLIC_BROWSER_WEBID_NATIVE_IDP_FETCHER_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
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
  struct CONTENT_EXPORT RequestParams {
    RequestParams();
    ~RequestParams();
    RequestParams(const RequestParams&);
    RequestParams& operator=(const RequestParams&);
    RequestParams(RequestParams&&);
    RequestParams& operator=(RequestParams&&);

    GURL url;
    std::optional<std::string> body;
    base::flat_map<std::string, std::string> headers;
  };

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

  // The string returned encodes a JSON payload. This is expected to be the same
  // JSON format as the corresponding HTTP endpoint response.
  using FetchResult = base::expected<std::string, FetchError>;
  using FetchCallback = base::OnceCallback<void(FetchResult)>;

  // Fetches data from the native IdP asynchronously.
  // Invokes `callback` with either a JSON string response or an error. The JSON
  // payload is expected to match the corresponding HTTP endpoint response
  // format. `params` contains target endpoint URL and optional body and headers
  // passed to the IDP.
  virtual void Fetch(const RequestParams& params, FetchCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBID_NATIVE_IDP_FETCHER_H_
