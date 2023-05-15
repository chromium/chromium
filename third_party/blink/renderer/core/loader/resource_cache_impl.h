// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_CACHE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_CACHE_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/loader/resource_cache.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class LocalFrame;

// Implements ResourceCache. Owned by LocalFrame.
class CORE_EXPORT ResourceCacheImpl final
    : public GarbageCollected<ResourceCacheImpl>,
      public mojom::blink::ResourceCache {
 public:
  static void Bind(LocalFrame*,
                   mojo::PendingReceiver<mojom::blink::ResourceCache>);

  ResourceCacheImpl(LocalFrame*,
                    mojo::PendingReceiver<mojom::blink::ResourceCache>);

  void AddReceiver(mojo::PendingReceiver<mojom::blink::ResourceCache>);

  void ClearReceivers();

  void Trace(Visitor*) const;

 private:
  // mojom::blink::ResourceCache implementations:
  void Contains(const KURL& url, ContainsCallback callback) override;

  Member<LocalFrame> frame_;
  HeapMojoReceiverSet<mojom::blink::ResourceCache, ResourceCacheImpl>
      receivers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_CACHE_IMPL_H_
