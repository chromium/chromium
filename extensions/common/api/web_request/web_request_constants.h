// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_CONSTANTS_H_
#define EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_CONSTANTS_H_

#include <string_view>

#include "base/containers/fixed_flat_set.h"

namespace extensions {

// Prefix for chrome.webRequest event names (e.g. "webRequest.onBeforeRequest").
inline constexpr char kWebRequestEventPrefix[] = "webRequest.";

// Prefix for <webview> event names (e.g. "webViewInternal.onBeforeRequest").
inline constexpr char kWebViewEventPrefix[] = "webViewInternal.";

// Keys of the payload dictionary that is appended as a second argument in
// per-context dispatch.
inline constexpr char kContextDispatchAwaitResponseKey[] = "awaitResponse";
inline constexpr char kContextDispatchInstanceIdKey[] = "instanceId";
inline constexpr char kContextDispatchWindowIdKey[] = "windowId";

// Request/response header names only delivered to webRequest listeners that
// registered the "extraHeaders" option. Lowercase for case-insensitive
// comparison.
// NOTE: Keep in sync with the corresponding constants in
// //extensions/renderer/resources/web_request_event.js.
inline constexpr auto kExtraRequestHeaderNames =
    base::MakeFixedFlatSet<std::string_view>(
        {"accept-encoding", "accept-language", "cookie", "origin", "referer"});
inline constexpr auto kExtraResponseHeaderNames =
    base::MakeFixedFlatSet<std::string_view>({"set-cookie"});

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_WEB_REQUEST_WEB_REQUEST_CONSTANTS_H_
