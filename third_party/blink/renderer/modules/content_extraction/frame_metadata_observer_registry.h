// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_FRAME_METADATA_OBSERVER_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_FRAME_METADATA_OBSERVER_REGISTRY_H_

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/content_extraction/frame_metadata_observer_registry.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote_set.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class LocalFrame;

// Registry used to Add Observers for when frame metadata changes.
class MODULES_EXPORT FrameMetadataObserverRegistry final
    : public GarbageCollected<FrameMetadataObserverRegistry>,
      public mojom::blink::FrameMetadataObserverRegistry,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];
  static FrameMetadataObserverRegistry* From(Document&);
  static void BindReceiver(
      LocalFrame* frame,
      mojo::PendingReceiver<mojom::blink::FrameMetadataObserverRegistry>
          receiver);

  FrameMetadataObserverRegistry(base::PassKey<FrameMetadataObserverRegistry>,
                                LocalFrame&);
  FrameMetadataObserverRegistry(const FrameMetadataObserverRegistry&) = delete;
  FrameMetadataObserverRegistry& operator=(
      const FrameMetadataObserverRegistry&) = delete;
  ~FrameMetadataObserverRegistry() override;

  void Trace(Visitor* visitor) const override;

  // mojom::blink::FrameMetadataObserverRegistry:
  void AddPaidContentMetadataObserver(
      mojo::PendingRemote<mojom::blink::PaidContentMetadataObserver> observer)
      override;

  void AddMetaTagsObserver(
      const Vector<String>& names,
      mojo::PendingRemote<mojom::blink::MetaTagsObserver> observer) override;

 private:
  class DomContentLoadedListener;
  friend class DomContentLoadedListener;

  void Bind(mojo::PendingReceiver<mojom::blink::FrameMetadataObserverRegistry>
                receiver);

  void OnDomContentLoaded();
  void OnPaidContentMetadataChanged();
  void OnMetaTagsChanged();

  void ListenForDomContentLoaded();

  void DisconnectHandler(mojo::RemoteSetElementId id);

  HeapMojoReceiverSet<mojom::blink::FrameMetadataObserverRegistry,
                      FrameMetadataObserverRegistry>
      receiver_set_;

  HeapMojoRemoteSet<mojom::blink::PaidContentMetadataObserver>
      paid_content_metadata_observers_;

  HeapMojoRemoteSet<mojom::blink::MetaTagsObserver> metatags_observers_;

  HeapHashMap<uint32_t, HeapVector<String>> metatags_observer_names_;

  Member<DomContentLoadedListener> dom_content_loaded_observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_FRAME_METADATA_OBSERVER_REGISTRY_H_
