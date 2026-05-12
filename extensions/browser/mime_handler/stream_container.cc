// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/stream_container.h"

#include <utility>

#include "extensions/browser/mime_handler/mime_handler_body_cache.h"
#include "net/http/http_response_headers.h"

namespace extensions {

StreamContainer::StreamContainer(
    int tab_id,
    bool embedded,
    const GURL& handler_url,
    const ExtensionId& extension_id,
    blink::mojom::TransferrableURLLoaderPtr transferrable_loader,
    const GURL& original_url)
    : embedded_(embedded),
      tab_id_(tab_id),
      handler_url_(handler_url),
      extension_id_(extension_id),
      transferrable_loader_(std::move(transferrable_loader)),
      mime_type_(transferrable_loader_->head->mime_type),
      original_url_(original_url),
      stream_url_(transferrable_loader_->url),
      response_headers_(transferrable_loader_->head->headers) {}

StreamContainer::~StreamContainer() = default;

base::WeakPtr<StreamContainer> StreamContainer::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

blink::mojom::TransferrableURLLoaderPtr
StreamContainer::TakeTransferrableURLLoader() {
  return std::move(transferrable_loader_);
}

void StreamContainer::SetBodyCache(scoped_refptr<MimeHandlerBodyCache> cache) {
  body_cache_ = std::move(cache);
}

mojo::ScopedDataPipeConsumerHandle StreamContainer::GetFallbackDataPipe() {
  if (body_cache_ && body_cache_->is_complete()) {
    return body_cache_->CreatePipe();
  }
  return mojo::ScopedDataPipeConsumerHandle();
}

}  // namespace extensions
