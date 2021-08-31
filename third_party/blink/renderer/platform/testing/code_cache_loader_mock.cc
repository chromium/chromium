// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/code_cache_loader_mock.h"

namespace blink {

void CodeCacheLoaderMock::FetchFromCodeCache(
    blink::mojom::CodeCacheType cache_type,
    const WebURL& url,
    WebCodeCacheLoader::FetchCodeCacheCallback callback) {
  if (controller_ && controller_->delayed_) {
    // This simple mock doesn't support multiple in-flight loads.
    CHECK(!controller_->callback_);

    controller_->callback_ = std::move(callback);
  } else {
    std::move(callback).Run(base::Time(), mojo_base::BigBuffer());
  }
}

void CodeCacheLoaderMock::ClearCodeCacheEntry(
    blink::mojom::CodeCacheType cache_type,
    const WebURL& url) {}

void CodeCacheLoaderMock::Controller::DelayResponse() {
  delayed_ = true;
}
void CodeCacheLoaderMock::Controller::Respond(base::Time time,
                                              mojo_base::BigBuffer data) {
  CHECK(callback_);
  std::move(callback_).Run(time, std::move(data));
}

}  // namespace blink
