// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESPONSE_BODY_LOADER_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESPONSE_BODY_LOADER_CLIENT_H_

#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom-blink-forward.h"

namespace blink {

// A ResponseBodyLoaderClient receives signals for loading a response body.
class ResponseBodyLoaderClient : public GarbageCollectedMixin {
 public:
  virtual ~ResponseBodyLoaderClient() = default;

  // Called when reading a chunk, with the chunk.
  virtual void DidReceiveData(base::span<const char> data) = 0;

  // Called when finishing reading the entire body. This must be the last
  // signal.
  virtual void DidFinishLoadingBody() = 0;

  // Called when seeing an error while reading the body. This must be the last
  // signal.
  virtual void DidFailLoadingBody() = 0;

  // Called when the loader cancelled loading the body.
  virtual void DidCancelLoadingBody() = 0;

  // Called when the body loader is suspended and the data pipe is drained.
  virtual void EvictFromBackForwardCache(
      mojom::blink::RendererEvictionReason) = 0;

  virtual void DidBufferLoadWhileInBackForwardCache(size_t num_bytes) = 0;

  virtual bool CanContinueBufferingWhileInBackForwardCache() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESPONSE_BODY_LOADER_CLIENT_H_
