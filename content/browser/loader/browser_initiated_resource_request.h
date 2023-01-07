// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_BROWSER_INITIATED_RESOURCE_REQUEST_H_
#define CONTENT_BROWSER_LOADER_BROWSER_INITIATED_RESOURCE_REQUEST_H_

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace blink {
struct RendererPreferences;
}  // namespace blink

namespace content {

class BrowserContext;

// Sets request headers appropriate for browser-initiated resource requests,
// i.e., requests for navigations and dedicated/shared/service worker
// scripts.
// If `should_update_existing_headers` is true, this function may update values
// that are already set in `headers`.
// This needs to be called on the UI thread.
void UpdateAdditionalHeadersForBrowserInitiatedRequest(
    net::HttpRequestHeaders* headers,
    BrowserContext* browser_context,
    bool should_update_existing_headers,
    const blink::RendererPreferences& renderer_preferences,
    bool is_for_worker_script);

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_BROWSER_INITIATED_RESOURCE_REQUEST_H_
