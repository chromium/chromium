// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_URL_LOADER_THROTTLES_H_
#define CONTENT_PUBLIC_BROWSER_URL_LOADER_THROTTLES_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"

namespace blink {
class URLLoaderThrottle;
}  // namespace blink

namespace network {
struct ResourceRequest;
}  // namespace network

namespace content {

class BrowserContext;
class NavigationUIData;
class WebContents;

// Wrapper around ContentBrowserClient::CreateURLLoaderThrottles which inserts
// additional content specific throttles.
CONTENT_EXPORT
std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
CreateContentBrowserURLLoaderThrottles(
    const network::ResourceRequest& request,
    BrowserContext* browser_context,
    const base::RepeatingCallback<WebContents*()>& wc_getter,
    NavigationUIData* navigation_ui_data,
    FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id);

// Wrapper around `ContentBrowserClient::CreateURLLoaderThrottlesForKeepAlive()`
// which inserts additional content specific throttles for handling fetch
// keepalive requests when their initiator is destroyed.
CONTENT_EXPORT
std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
CreateContentBrowserURLLoaderThrottlesForKeepAlive(
    const network::ResourceRequest& request,
    BrowserContext* browser_context,
    const base::RepeatingCallback<WebContents*()>& wc_getter,
    FrameTreeNodeId frame_tree_node_id);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_URL_LOADER_THROTTLES_H_
