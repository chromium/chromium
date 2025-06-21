// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/frame_metadata_observer_registry.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/modules/content_extraction/paid_content.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

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
      observers_(frame.DomWindow()) {}

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
  visitor->Trace(observers_);
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
    if (!registry) {
      // There is no registry. Just abort.
      return;
    }
    registry->OnDomContentLoaded();
  }
};

void FrameMetadataObserverRegistry::AddObserver(
    mojo::PendingRemote<mojom::blink::FrameMetadataObserver> observer) {
  observers_.Add(std::move(observer), GetSupplementable()->GetTaskRunner(
                                          TaskType::kInternalUserInteraction));
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

void FrameMetadataObserverRegistry::OnDomContentLoaded() {
  OnPaidContentMetadataChanged();

  if(dom_content_loaded_observer_) {
    GetSupplementable()->removeEventListener(event_type_names::kDOMContentLoaded,
      dom_content_loaded_observer_.Get(),
      false);
    dom_content_loaded_observer_ = nullptr;
  }
}

void FrameMetadataObserverRegistry::OnPaidContentMetadataChanged() {
  PaidContent paid_content;
  // TODO(gklassen): Add a variant of QueryPaidElements that just checks for
  // presence and doesn't look for elements.
  bool has_paid_content = paid_content.QueryPaidElements(*GetSupplementable());

  // TODO(gklassen): Add a MuationObserver to monitor for changes during the
  // lifetime of the page.

  for (auto& observer : observers_) {
    observer->OnPaidContentMetadataChanged(has_paid_content);
  }
}


}  // namespace blink
