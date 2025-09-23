// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_FRAME_METADATA_OBSERVER_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_FRAME_METADATA_OBSERVER_REGISTRY_H_

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom-blink.h"
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

class HTMLMetaElement;
class HTMLScriptElement;
class LocalFrame;
class MutationObserver;

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
  class MetaTagAttributeObserver;
  class MetaTagsMutationObserver;
  class PaidContentAttributeObserver;
  class PaidContentMutationObserver;
  friend class DomContentLoadedListener;

  void Bind(mojo::PendingReceiver<mojom::blink::FrameMetadataObserverRegistry>
                receiver);

  void DisconnectAllAttributeObservers();
  void ObserveMetaTagAttributes(HTMLMetaElement* meta);
  void StopObservingMetaTagAttributes(HTMLMetaElement* meta);
  void DisconnectAllPaidContentAttributeObservers();
  void ObservePaidContentScriptAttributes(HTMLScriptElement* script);
  void StopObservingPaidContentScriptAttributes(HTMLScriptElement* script);

  void OnDomContentLoaded();
  void OnPaidContentMetadataChanged();
  void OnMetaTagsChanged();

  // Returns true if there are observers.
  bool UpdateMetaTagsObserver();
  // Returns true if there are observers.
  bool UpdatePaidContentObserver();

  void ListenForDomContentLoaded();

  void DisconnectHandler(mojo::RemoteSetElementId);
  void PaidContentDisconnectHandler(mojo::RemoteSetElementId);

  HeapMojoReceiverSet<mojom::blink::FrameMetadataObserverRegistry,
                      FrameMetadataObserverRegistry>
      receiver_set_;

  HeapMojoRemoteSet<mojom::blink::PaidContentMetadataObserver>
      paid_content_metadata_observers_;

  HeapMojoRemoteSet<mojom::blink::MetaTagsObserver> metatags_observers_;

  struct MetaTagsObserverData : public GarbageCollected<MetaTagsObserverData> {
    void Trace(Visitor* visitor) const { visitor->Trace(names_to_observe); }

    HeapVector<String> names_to_observe;
    Vector<mojom::blink::MetaTagPtr> last_sent_meta_tags;
  };

  // Data for each metatags observer, keyed by RemoteSetElementId.
  HeapHashMap<uint32_t, Member<MetaTagsObserverData>>
      remote_id_to_observer_data_;
  // A map from metatag name to the number of observers that are interested in
  // it.
  HashMap<String, int> all_metatag_name_counts_;

  Member<DomContentLoadedListener> dom_content_loaded_observer_;

  Member<MetaTagsMutationObserver> meta_tags_mutation_observer_;
  Member<PaidContentMutationObserver> paid_content_mutation_observer_;

  HeapHashMap<WeakMember<HTMLMetaElement>, Member<MutationObserver>>
      meta_tag_attribute_observers_;
  HeapHashMap<WeakMember<HTMLScriptElement>, Member<MutationObserver>>
      paid_content_attribute_observers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_FRAME_METADATA_OBSERVER_REGISTRY_H_
