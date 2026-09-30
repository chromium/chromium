// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_FALLBACK_URL_LOADER_REQUEST_INTERCEPTOR_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_FALLBACK_URL_LOADER_REQUEST_INTERCEPTOR_H_

#include <cstdint>

#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/url_loader_request_interceptor.h"

namespace extensions::mime_handler {

// Answers a native-handler fallback re-navigation from the body the stream
// manager kept, so the document is not fetched a second time. Declines every
// other navigation.
class MimeHandlerFallbackURLLoaderRequestInterceptor final
    : public content::URLLoaderRequestInterceptor {
 public:
  MimeHandlerFallbackURLLoaderRequestInterceptor(
      content::FrameTreeNodeId frame_tree_node_id,
      int64_t navigation_id);
  MimeHandlerFallbackURLLoaderRequestInterceptor(
      const MimeHandlerFallbackURLLoaderRequestInterceptor&) = delete;
  MimeHandlerFallbackURLLoaderRequestInterceptor& operator=(
      const MimeHandlerFallbackURLLoaderRequestInterceptor&) = delete;
  ~MimeHandlerFallbackURLLoaderRequestInterceptor() override;

  // content::URLLoaderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      LoaderCallback callback) override;

 private:
  RequestHandler CreateRequestHandler(
      const network::ResourceRequest& tentative_resource_request);

  const content::FrameTreeNodeId frame_tree_node_id_;
  const int64_t navigation_id_;
};

}  // namespace extensions::mime_handler

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_FALLBACK_URL_LOADER_REQUEST_INTERCEPTOR_H_
