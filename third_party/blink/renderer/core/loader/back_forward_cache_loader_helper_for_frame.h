// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_BACK_FORWARD_CACHE_LOADER_HELPER_FOR_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_BACK_FORWARD_CACHE_LOADER_HELPER_FOR_FRAME_H_

#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/back_forward_cache_loader_helper.h"

namespace blink {

class LocalFrame;

class BackForwardCacheLoaderHelperForFrame
    : public BackForwardCacheLoaderHelper {
 public:
  explicit BackForwardCacheLoaderHelperForFrame(LocalFrame& frame)
      : frame_(&frame) {}

  void EvictFromBackForwardCache(
      mojom::blink::RendererEvictionReason reason) override;
  void DidBufferLoadWhileInBackForwardCache(size_t num_bytes) override;
  bool CanContinueBufferingWhileInBackForwardCache() const override;
  void Detach() override;
  void Trace(Visitor*) const override;

 private:
  WeakMember<LocalFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_BACK_FORWARD_CACHE_LOADER_HELPER_FOR_FRAME_H_
