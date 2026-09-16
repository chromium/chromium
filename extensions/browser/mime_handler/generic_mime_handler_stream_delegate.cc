// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/generic_mime_handler_stream_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "extensions/browser/mime_handler/stream_info.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"

namespace extensions::mime_handler {

namespace {

// Restricts `headers` to the CORS-safelisted response header names, so a
// generic (third-party) MIME handler extension only sees the response headers
// that fetch() would expose to script cross-origin. Without this, a
// zero-permission handler could read arbitrary cross-origin response headers
// (auth tokens, and similar) off the stream it handles.
// https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name
void FilterToCorsSafelistedResponseHeaders(net::HttpResponseHeaders* headers) {
  std::vector<std::string> names_to_remove;
  size_t iter = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    if (!network::cors::IsCorsSafelistedResponseHeaderName(name)) {
      names_to_remove.emplace_back(name);
    }
  }
  headers->RemoveHeaders(names_to_remove);
}

}  // namespace

GenericMimeHandlerStreamDelegate::GenericMimeHandlerStreamDelegate() = default;

void GenericMimeHandlerStreamDelegate::OnExtensionFrameReadyToCommit(
    content::NavigationHandle* navigation_handle,
    extensions::StreamInfo* stream_info) {
  CHECK(stream_info);
  blink::mojom::TransferrableURLLoaderPtr transferrable_loader =
      stream_info->stream()->TakeTransferrableURLLoader();
  // Nothing left to hand over, and the filtering below reads through this
  // pointer.
  if (!transferrable_loader) {
    return;
  }
  CHECK(transferrable_loader->head);
  if (transferrable_loader->head->headers) {
    FilterToCorsSafelistedResponseHeaders(
        transferrable_loader->head->headers.get());
  }
  // Register the transferable URL loader as a subresource override so the
  // handler page can fetch the original response data via the stream URL.
  navigation_handle->RegisterSubresourceOverride(
      std::move(transferrable_loader));
}

void GenericMimeHandlerStreamDelegate::ValidateContentFrameHost(
    content::RenderFrameHost* /*content_host*/,
    extensions::StreamInfo* /*stream_info*/) {}

bool GenericMimeHandlerStreamDelegate::RequiresPerInstanceProcessIsolation()
    const {
  return true;
}

bool GenericMimeHandlerStreamDelegate::ShouldFilterResponseHeadersForHandler()
    const {
  return true;
}

GenericMimeHandlerStreamDelegate::~GenericMimeHandlerStreamDelegate() = default;

}  // namespace extensions::mime_handler
