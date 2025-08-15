// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/frame_metadata_observer_registry.h"

#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/content_extraction/paid_content.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

Vector<mojom::blink::MetaTagPtr> CollectMetaTags(
    LocalFrame* frame,
    const HeapVector<String>& names_to_find) {
  Vector<mojom::blink::MetaTagPtr> found_tags;
  if (!frame) {
    return found_tags;
  }

  Document* document = frame->GetDocument();
  if (document && document->head()) {
    for (HTMLMetaElement& meta :
         Traversal<HTMLMetaElement>::ChildrenOf(*document->head())) {
      const String& name = meta.GetName();
      if (names_to_find.Contains(name)) {
        auto meta_tag = mojom::blink::MetaTag::New();
        meta_tag->name = name;
        meta_tag->content = meta.Content();
        found_tags.push_back(std::move(meta_tag));
      }
    }
  }
  return found_tags;
}

}  // namespace

class FrameMetadataObserverRegistry::MetaTagsMutationObserver final
    : public MutationObserver::Delegate {
 public:
  explicit MetaTagsMutationObserver(FrameMetadataObserverRegistry* registry)
      : registry_(registry), observer_(MutationObserver::Create(this)) {}

  void Observe(Node* target) {
    MutationObserverInit* init = MutationObserverInit::Create();
    init->setChildList(true);
    init->setAttributes(true);
    init->setAttributeFilter(Vector<String>{"name", "content"});
    init->setSubtree(true);
    DummyExceptionStateForTesting exception_state;
    observer_->observe(target, init, exception_state);
    DCHECK(!exception_state.HadException());
  }

  ExecutionContext* GetExecutionContext() const override {
    return registry_->GetSupplementable()->GetExecutionContext();
  }

  void Deliver(const HeapVector<Member<MutationRecord>>&,
               MutationObserver&) override {
    registry_->OnMetaTagsChanged();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(registry_);
    visitor->Trace(observer_);
    MutationObserver::Delegate::Trace(visitor);
  }

 private:
  Member<FrameMetadataObserverRegistry> registry_;
  Member<MutationObserver> observer_;
};

// static
const char FrameMetadataObserverRegistry::kSupplementName[] =
    "FrameMetadataObserverRegistry";

// static
FrameMetadataObserverRegistry* FrameMetadataObserverRegistry::From(
    Document& document) {
  return Supplement<Document>::From<FrameMetadataObserverRegistry>(document);
}

// static
void FrameMetadataObserverRegistry::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::FrameMetadataObserverRegistry>
        receiver) {
  CHECK(frame && frame->GetDocument());

  auto& document = *frame->GetDocument();
  auto* registry = FrameMetadataObserverRegistry::From(document);
  if (!registry) {
    registry = MakeGarbageCollected<FrameMetadataObserverRegistry>(
        base::PassKey<FrameMetadataObserverRegistry>(), *frame);
    Supplement<Document>::ProvideTo(document, registry);
  }
  registry->Bind(std::move(receiver));
}

FrameMetadataObserverRegistry::FrameMetadataObserverRegistry(
    base::PassKey<FrameMetadataObserverRegistry>,
    LocalFrame& frame)
    : Supplement<Document>(*frame.GetDocument()),
      receiver_set_(this, frame.DomWindow()),
      paid_content_metadata_observers_(frame.DomWindow()),
      metatags_observers_(frame.DomWindow()) {
  // Observer endpoints are explicitly closed when the other side is no
  // longer interested, so clean up the meta tags requested by that
  // observer at disconnect time.
  metatags_observers_.set_disconnect_handler(
      blink::BindRepeating(&FrameMetadataObserverRegistry::DisconnectHandler,
                           WrapWeakPersistent(this)));
}

FrameMetadataObserverRegistry::~FrameMetadataObserverRegistry() = default;

void FrameMetadataObserverRegistry::Bind(
    mojo::PendingReceiver<mojom::blink::FrameMetadataObserverRegistry>
        receiver) {
  receiver_set_.Add(
      std::move(receiver),
      GetSupplementable()->GetTaskRunner(TaskType::kInternalUserInteraction));
}

void FrameMetadataObserverRegistry::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
  visitor->Trace(receiver_set_);
  visitor->Trace(dom_content_loaded_observer_);
  visitor->Trace(paid_content_metadata_observers_);
  visitor->Trace(metatags_observers_);
  visitor->Trace(metatags_observer_names_);
  visitor->Trace(meta_tags_mutation_observer_);
}

class FrameMetadataObserverRegistry::DomContentLoadedListener final
    : public NativeEventListener {
 public:
  void Invoke(ExecutionContext* execution_context,
              blink::Event* event) override {
    DCHECK_EQ(event->type(), "DOMContentLoaded");

    // We can only get DOMContentLoaded event from a Window, not a Worker.
    DCHECK(execution_context->IsWindow());
    LocalDOMWindow& window = *To<LocalDOMWindow>(execution_context);

    Document& document = *window.document();

    auto* registry =
        Supplement<Document>::From<FrameMetadataObserverRegistry>(document);
    if (registry) {
      registry->OnDomContentLoaded();
    }
  }
};

void FrameMetadataObserverRegistry::ListenForDomContentLoaded() {
  if (GetSupplementable()->HasFinishedParsing()) {
    OnDomContentLoaded();
  } else {
    if (!dom_content_loaded_observer_) {
      dom_content_loaded_observer_ =
          MakeGarbageCollected<DomContentLoadedListener>();
      GetSupplementable()->addEventListener(event_type_names::kDOMContentLoaded,
                                            dom_content_loaded_observer_.Get(),
                                            false);
    }
  }
}

void FrameMetadataObserverRegistry::AddPaidContentMetadataObserver(
    mojo::PendingRemote<mojom::blink::PaidContentMetadataObserver> observer) {
  paid_content_metadata_observers_.Add(
      std::move(observer),
      GetSupplementable()->GetTaskRunner(TaskType::kInternalUserInteraction));
  ListenForDomContentLoaded();
}

void FrameMetadataObserverRegistry::AddMetaTagsObserver(
    const Vector<String>& names,
    mojo::PendingRemote<mojom::blink::MetaTagsObserver> observer) {
  const mojo::RemoteSetElementId& remote_id = metatags_observers_.Add(
      std::move(observer),
      GetSupplementable()->GetTaskRunner(TaskType::kInternalUserInteraction));

  metatags_observer_names_.Set(remote_id.value(), HeapVector<String>(names));
  has_sent_metatags_.Set(remote_id.value(), false);
  ListenForDomContentLoaded();
}

void FrameMetadataObserverRegistry::OnDomContentLoaded() {
  OnPaidContentMetadataChanged();
  OnMetaTagsChanged();

  if (dom_content_loaded_observer_) {
    GetSupplementable()->removeEventListener(
        event_type_names::kDOMContentLoaded, dom_content_loaded_observer_.Get(),
        false);
    dom_content_loaded_observer_ = nullptr;
  }

  if (!metatags_observers_.empty() && !meta_tags_mutation_observer_) {
    meta_tags_mutation_observer_ =
        MakeGarbageCollected<MetaTagsMutationObserver>(this);
    if (auto* head = GetSupplementable()->head()) {
      meta_tags_mutation_observer_->Observe(head);
    }
  }
}

void FrameMetadataObserverRegistry::OnPaidContentMetadataChanged() {
  if (paid_content_metadata_observers_.empty()) {
    return;
  }
  PaidContent paid_content;
  bool paid_content_exists =
      paid_content.QueryPaidElements(*GetSupplementable());

  if (!paid_content_exists) {
    return;
  }

  // TODO(gklassen): Add a MutationObserver to monitor for changes during
  // the lifetime of the page.

  for (auto& observer : paid_content_metadata_observers_) {
    observer->OnPaidContentMetadataChanged(paid_content_exists);
  }
}

void FrameMetadataObserverRegistry::OnMetaTagsChanged() {
  if (metatags_observers_.empty()) {
    return;
  }

  LocalFrame* current_frame = GetSupplementable()->GetFrame();
  if (!current_frame) {
    return;
  }

  for (auto& it : metatags_observer_names_) {
    const mojo::RemoteSetElementId remote_id(it.key);
    auto meta_tags = CollectMetaTags(current_frame, it.value);

    const bool has_metatags = !meta_tags.empty();
    DCHECK(has_sent_metatags_.Contains(remote_id.value()));
    const bool has_sent_metatags_before =
        has_sent_metatags_.at(remote_id.value());

    // Only send the meta tags if matching names were found, or if we have
    // already sent meta tags for this observer and they have since been
    // removed.
    if (has_metatags || has_sent_metatags_before) {
      metatags_observers_.Get(remote_id)->OnMetaTagsChanged(
          std::move(meta_tags));
      has_sent_metatags_.Set(remote_id.value(), has_metatags);
    }
  }
}

void FrameMetadataObserverRegistry::DisconnectHandler(
    mojo::RemoteSetElementId id) {
  metatags_observer_names_.erase(id.value());
  has_sent_metatags_.erase(id.value());
}

}  // namespace blink
