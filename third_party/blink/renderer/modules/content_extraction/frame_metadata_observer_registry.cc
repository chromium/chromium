// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/frame_metadata_observer_registry.h"

#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/content_extraction/paid_content.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

void CollectMetaTagsFromFrame(LocalFrame* frame,
                              const HeapVector<String>& names_to_find,
                              mojom::blink::PageMetadata& page_metadata) {
  if (!frame) {
    return;
  }

    auto* local_frame = To<LocalFrame>(frame);
    Document* document = local_frame->GetDocument();
    if (document && document->head()) {
      Vector<mojom::blink::MetaTagPtr> found_tags;
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
      if (!found_tags.empty()) {
        auto frame_metadata = mojom::blink::FrameMetadata::New();
        frame_metadata->url = document->Url();
        frame_metadata->meta_tags = std::move(found_tags);
        page_metadata.frame_metadata.push_back(std::move(frame_metadata));
      }
    }
}

}  // namespace

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
  metatags_observers_.set_disconnect_handler(blink::BindRepeating(
      &FrameMetadataObserverRegistry::DisconnectHandler, WrapPersistent(this)));
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
}

void FrameMetadataObserverRegistry::OnPaidContentMetadataChanged() {
  if (paid_content_metadata_observers_.empty()) {
    return;
  }
  PaidContent paid_content;
  bool paid_content_exists =
      paid_content.QueryPaidElements(*GetSupplementable());

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
    auto page_metadata = mojom::blink::PageMetadata::New();
    CollectMetaTagsFromFrame(current_frame, it.value, *page_metadata);
    metatags_observers_.Get(mojo::RemoteSetElementId(it.key))
        ->OnMetaTagsChanged(std::move(page_metadata));
  }
}

void FrameMetadataObserverRegistry::DisconnectHandler(
    mojo::RemoteSetElementId id) {
  metatags_observer_names_.erase(id.value());
}

}  // namespace blink
