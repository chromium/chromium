// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_FRAME_METADATA_OBSERVER_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_FRAME_METADATA_OBSERVER_REGISTRY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom-blink.h"
#include "third_party/blink/public/mojom/content_extraction/frame_metadata_observer_registry.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
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
  struct MetaTagsObserverTraits;
  struct PaidContentObserverTraits;
  template <typename Traits>
  class FrameMetadataMutationObserver;

  class DomContentLoadedListener;
  class MetaTagAttributeObserver;
  class PaidContentAttributeObserver;
  friend class DomContentLoadedListener;

  struct MetaTagsObserverTraits {
    using ElementType = HTMLMetaElement;
    static void OnChanged(FrameMetadataObserverRegistry* registry) {
      registry->OnMetaTagsChanged();
    }
    static void ObserveAttributes(FrameMetadataObserverRegistry* registry,
                                  ElementType* element) {
      registry->ObserveMetaTagAttributes(element);
    }
    static void StopObservingAttributes(FrameMetadataObserverRegistry* registry,
                                        ElementType* element) {
      registry->StopObservingMetaTagAttributes(element);
    }
    static void DisconnectAllAttributeObservers(
        FrameMetadataObserverRegistry* registry) {
      registry->DisconnectAllAttributeObservers();
    }
  };

  struct PaidContentObserverTraits {
    using ElementType = HTMLScriptElement;
    static void OnChanged(FrameMetadataObserverRegistry* registry) {
      registry->OnPaidContentMetadataChanged();
    }
    static void ObserveAttributes(FrameMetadataObserverRegistry* registry,
                                  ElementType* element) {
      registry->ObservePaidContentScriptAttributes(element);
    }
    static void StopObservingAttributes(FrameMetadataObserverRegistry* registry,
                                        ElementType* element) {
      registry->StopObservingPaidContentScriptAttributes(element);
    }
    static void DisconnectAllAttributeObservers(
        FrameMetadataObserverRegistry* registry) {
      registry->DisconnectAllPaidContentAttributeObservers();
    }
  };

  template <typename Traits>
  class FrameMetadataMutationObserver final
      : public MutationObserver::Delegate {
   public:
    explicit FrameMetadataMutationObserver(
        FrameMetadataObserverRegistry* registry)
        : registry_(registry), observer_(MutationObserver::Create(this)) {}

    void ObserveHead(HTMLHeadElement* head) {
      if (observing_.Get() == head) {
        return;
      }
      Disconnect();

      observing_ = head;
      if (!head) {
        return;
      }

      // Start observing childList changes in the head.
      MutationObserverInit* init = MutationObserverInit::Create();
      init->setChildList(true);
      init->setSubtree(true);
      DummyExceptionStateForTesting exception_state;
      observer_->observe(head, init, exception_state);
      DCHECK(!exception_state.HadException());

      // For all existing elements, set up attribute observers.
      for (typename Traits::ElementType& element :
           Traversal<typename Traits::ElementType>::ChildrenOf(*head)) {
        Traits::ObserveAttributes(registry_, &element);
      }
    }

    void ObserveDocument(Element* document_element) {
      if (observing_.Get() == document_element) {
        return;
      }
      observer_->disconnect();
      MutationObserverInit* init = MutationObserverInit::Create();
      init->setChildList(true);
      DummyExceptionStateForTesting exception_state;
      observer_->observe(document_element, init, exception_state);
      DCHECK(!exception_state.HadException());
      observing_ = document_element;
    }

    void Disconnect() {
      observer_->disconnect();
      observing_ = nullptr;
      Traits::DisconnectAllAttributeObservers(registry_);
    }

    ExecutionContext* GetExecutionContext() const override {
      return registry_->GetSupplementable()->GetExecutionContext();
    }

    void Deliver(const HeapVector<Member<MutationRecord>>& records,
                 MutationObserver&) override {
      bool needs_update = false;
      for (const auto& record : records) {
        if (record->type() == "childList") {
          // This handles the case where the <head> element itself is added to
          // the doc.
          for (unsigned i = 0; i < record->addedNodes()->length(); ++i) {
            if (IsA<HTMLHeadElement>(record->addedNodes()->item(i))) {
              Traits::OnChanged(registry_);
              return;
            }
          }

          // This handles meta tags added/removed inside the head.
          for (unsigned i = 0; i < record->addedNodes()->length(); ++i) {
            if (auto* element = DynamicTo<typename Traits::ElementType>(
                    record->addedNodes()->item(i))) {
              Traits::ObserveAttributes(registry_, element);
              needs_update = true;
            }
          }
          for (unsigned i = 0; i < record->removedNodes()->length(); ++i) {
            if (auto* element = DynamicTo<typename Traits::ElementType>(
                    record->removedNodes()->item(i))) {
              Traits::StopObservingAttributes(registry_, element);
              needs_update = true;
            }
          }
        }
      }

      if (needs_update) {
        Traits::OnChanged(registry_);
      }
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(registry_);
      visitor->Trace(observer_);
      visitor->Trace(observing_);
      MutationObserver::Delegate::Trace(visitor);
    }

   private:
    Member<FrameMetadataObserverRegistry> registry_;
    Member<MutationObserver> observer_;
    WeakMember<Node> observing_;
  };

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
    bool sent_initial_update = false;
  };

  // Data for each metatags observer, keyed by RemoteSetElementId.
  HeapHashMap<uint32_t, Member<MetaTagsObserverData>>
      remote_id_to_observer_data_;
  // A map from metatag name to the number of observers that are interested in
  // it.
  HashMap<String, int> all_metatag_name_counts_;

  Member<DomContentLoadedListener> dom_content_loaded_observer_;

  Member<FrameMetadataMutationObserver<MetaTagsObserverTraits>>
      meta_tags_mutation_observer_;
  Member<FrameMetadataMutationObserver<PaidContentObserverTraits>>
      paid_content_mutation_observer_;

  HeapHashMap<WeakMember<HTMLMetaElement>, Member<MutationObserver>>
      meta_tag_attribute_observers_;
  HeapHashMap<WeakMember<HTMLScriptElement>, Member<MutationObserver>>
      paid_content_attribute_observers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_FRAME_METADATA_OBSERVER_REGISTRY_H_
