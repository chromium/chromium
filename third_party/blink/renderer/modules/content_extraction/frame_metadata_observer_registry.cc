// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/frame_metadata_observer_registry.h"

#include <optional>

#include "mojo/public/cpp/bindings/lib/wtf_clone_equals_util.h"
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
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/content_extraction/paid_content.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/key_value_pair.h"

namespace blink {

namespace {

template <typename T>
void DeliverMutation(const HeapVector<Member<MutationRecord>>& records,
                     base::RepeatingClosure on_changed) {
  // We are looking for changes to elements of type T.
  for (const auto& record : records) {
    if (record->type() == "attributes") {
      if (IsA<T>(record->target())) {
        on_changed.Run();
        return;
      }
    } else {  // "childList"
      for (unsigned i = 0; i < record->addedNodes()->length(); ++i) {
        if (IsA<T>(record->addedNodes()->item(i))) {
          on_changed.Run();
          return;
        }
      }
      for (unsigned i = 0; i < record->removedNodes()->length(); ++i) {
        if (IsA<T>(record->removedNodes()->item(i))) {
          on_changed.Run();
          return;
        }
      }
    }
  }
}

void ObserveDocument(WeakMember<Node>& observing,
                     MutationObserver* observer,
                     Element* document_element) {
  if (observing.Get() == document_element) {
    return;
  }
  observer->disconnect();
  MutationObserverInit* init = MutationObserverInit::Create();
  init->setChildList(true);
  DummyExceptionStateForTesting exception_state;
  observer->observe(document_element, init, exception_state);
  DCHECK(!exception_state.HadException());
  observing = document_element;
}

template <typename ObserverSet, typename MutationObserver>
bool UpdateObserver(Document* document,
                    ObserverSet& observer_set,
                    MutationObserver& mutation_observer);

}  // namespace

class FrameMetadataObserverRegistry::PaidContentAttributeObserver final
    : public MutationObserver::Delegate {
 public:
  explicit PaidContentAttributeObserver(
      FrameMetadataObserverRegistry* registry);

  ExecutionContext* GetExecutionContext() const override {
    return registry_->GetSupplementable()->GetExecutionContext();
  }

  void Deliver(const HeapVector<Member<MutationRecord>>& /*records*/,
               MutationObserver&) override {
    registry_->OnPaidContentMetadataChanged();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(registry_);
    MutationObserver::Delegate::Trace(visitor);
  }

 private:
  Member<FrameMetadataObserverRegistry> registry_;
};

class FrameMetadataObserverRegistry::PaidContentMutationObserver final
    : public MutationObserver::Delegate {
 public:
  explicit PaidContentMutationObserver(FrameMetadataObserverRegistry* registry);

  void ObserveHead(HTMLHeadElement* head);
  void ObserveDocument(Element* document_element);

  void Disconnect() {
    observer_->disconnect();
    observing_ = nullptr;
    registry_->DisconnectAllPaidContentAttributeObservers();
  }

  ExecutionContext* GetExecutionContext() const override {
    return registry_->GetSupplementable()->GetExecutionContext();
  }

  void Deliver(const HeapVector<Member<MutationRecord>>& records,
               MutationObserver&) override {
    bool needs_update = false;
    for (const auto& record : records) {
      if (record->type() == "childList") {
        for (unsigned i = 0; i < record->addedNodes()->length(); ++i) {
          if (IsA<HTMLHeadElement>(record->addedNodes()->item(i))) {
            registry_->OnPaidContentMetadataChanged();
            return;
          }
        }
        for (unsigned i = 0; i < record->addedNodes()->length(); ++i) {
          if (auto* script =
                  DynamicTo<HTMLScriptElement>(record->addedNodes()->item(i))) {
            registry_->ObservePaidContentScriptAttributes(script);
            needs_update = true;
          }
        }
        for (unsigned i = 0; i < record->removedNodes()->length(); ++i) {
          if (auto* script = DynamicTo<HTMLScriptElement>(
                  record->removedNodes()->item(i))) {
            registry_->StopObservingPaidContentScriptAttributes(script);
            needs_update = true;
          }
        }
      }
    }

    if (needs_update) {
      registry_->OnPaidContentMetadataChanged();
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

class FrameMetadataObserverRegistry::MetaTagAttributeObserver final
    : public MutationObserver::Delegate {
 public:
  explicit MetaTagAttributeObserver(FrameMetadataObserverRegistry* registry);

  ExecutionContext* GetExecutionContext() const override {
    return registry_->GetSupplementable()->GetExecutionContext();
  }

  void Deliver(const HeapVector<Member<MutationRecord>>& /*records*/,
               MutationObserver&) override {
    registry_->OnMetaTagsChanged();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(registry_);
    MutationObserver::Delegate::Trace(visitor);
  }

 private:
  Member<FrameMetadataObserverRegistry> registry_;
};

// TODO(gklassen): Use templated class to avoid duplication between this and
// PaidContentMutationObserver.
class FrameMetadataObserverRegistry::MetaTagsMutationObserver final
    : public MutationObserver::Delegate {
 public:
  explicit MetaTagsMutationObserver(FrameMetadataObserverRegistry* registry);

  void ObserveHead(HTMLHeadElement* head);
  void ObserveDocument(Element* document_element);

  void Disconnect() {
    observer_->disconnect();
    observing_ = nullptr;
    registry_->DisconnectAllAttributeObservers();
  }

  ExecutionContext* GetExecutionContext() const override {
    return registry_->GetSupplementable()->GetExecutionContext();
  }

  void Deliver(const HeapVector<Member<MutationRecord>>& records,
               MutationObserver&) override {
    bool needs_update = false;
    for (const auto& record : records) {
      if (record->type() == "childList") {
        // This handles the case where the <head> element itself is added to the
        // doc.
        for (unsigned i = 0; i < record->addedNodes()->length(); ++i) {
          if (IsA<HTMLHeadElement>(record->addedNodes()->item(i))) {
            registry_->OnMetaTagsChanged();
            return;
          }
        }

        // This handles meta tags added/removed inside the head.
        for (unsigned i = 0; i < record->addedNodes()->length(); ++i) {
          if (auto* meta =
                  DynamicTo<HTMLMetaElement>(record->addedNodes()->item(i))) {
            registry_->ObserveMetaTagAttributes(meta);
            needs_update = true;
          }
        }
        for (unsigned i = 0; i < record->removedNodes()->length(); ++i) {
          if (auto* meta =
                  DynamicTo<HTMLMetaElement>(record->removedNodes()->item(i))) {
            registry_->StopObservingMetaTagAttributes(meta);
            needs_update = true;
          }
        }
      }
    }

    if (needs_update) {
      registry_->OnMetaTagsChanged();
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

FrameMetadataObserverRegistry::PaidContentAttributeObserver::
    PaidContentAttributeObserver(FrameMetadataObserverRegistry* registry)
    : registry_(registry) {}

FrameMetadataObserverRegistry::PaidContentMutationObserver::
    PaidContentMutationObserver(FrameMetadataObserverRegistry* registry)
    : registry_(registry), observer_(MutationObserver::Create(this)) {}

void FrameMetadataObserverRegistry::PaidContentMutationObserver::ObserveHead(
    HTMLHeadElement* head) {
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

  // For all existing script tags, set up attribute observers.
  for (HTMLScriptElement& script :
       Traversal<HTMLScriptElement>::ChildrenOf(*head)) {
    registry_->ObservePaidContentScriptAttributes(&script);
  }
}

void FrameMetadataObserverRegistry::PaidContentMutationObserver::
    ObserveDocument(Element* document_element) {
  ::blink::ObserveDocument(observing_, observer_.Get(), document_element);
}

FrameMetadataObserverRegistry::MetaTagsMutationObserver::
    MetaTagsMutationObserver(FrameMetadataObserverRegistry* registry)
    : registry_(registry),
      observer_(MutationObserver::Create(this)) {}  // NO LINT

FrameMetadataObserverRegistry::MetaTagAttributeObserver::
    MetaTagAttributeObserver(FrameMetadataObserverRegistry* registry)
    : registry_(registry) {}

void FrameMetadataObserverRegistry::MetaTagsMutationObserver::ObserveHead(
    HTMLHeadElement* head) {
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

  // For all existing meta tags, set up attribute observers.
  for (HTMLMetaElement& meta : Traversal<HTMLMetaElement>::ChildrenOf(*head)) {
    registry_->ObserveMetaTagAttributes(&meta);
  }
}

void FrameMetadataObserverRegistry::MetaTagsMutationObserver::ObserveDocument(
    Element* document_element) {
  ::blink::ObserveDocument(observing_, observer_.Get(), document_element);
}

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
      metatags_observers_(frame.DomWindow()),
      meta_tags_mutation_observer_(
          MakeGarbageCollected<MetaTagsMutationObserver>(this)),
      paid_content_mutation_observer_(
          MakeGarbageCollected<PaidContentMutationObserver>(this)) {
  // Observer endpoints are explicitly closed when the other side is no
  // longer interested, so clean up the meta tags requested by that
  // observer at disconnect time.
  metatags_observers_.set_disconnect_handler(
      blink::BindRepeating(&FrameMetadataObserverRegistry::DisconnectHandler,
                           WrapWeakPersistent(this)));
  paid_content_metadata_observers_.set_disconnect_handler(blink::BindRepeating(
      &FrameMetadataObserverRegistry::PaidContentDisconnectHandler,
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
  visitor->Trace(remote_id_to_observer_data_);
  visitor->Trace(meta_tags_mutation_observer_);
  visitor->Trace(paid_content_mutation_observer_);
  visitor->Trace(meta_tag_attribute_observers_);
  visitor->Trace(paid_content_attribute_observers_);
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
  DCHECK(!names.empty());
  const mojo::RemoteSetElementId& remote_id = metatags_observers_.Add(
      std::move(observer),
      GetSupplementable()->GetTaskRunner(TaskType::kInternalUserInteraction));

  auto* observer_data = MakeGarbageCollected<MetaTagsObserverData>();
  observer_data->names_to_observe = HeapVector<String>(names);
  remote_id_to_observer_data_.Set(remote_id.value(), observer_data);

  for (const String& name : names) {
    auto result = all_metatag_name_counts_.insert(name, 1);
    if (!result.is_new_entry) {
      result.stored_value->value++;
    }
  }
  ListenForDomContentLoaded();
}

void FrameMetadataObserverRegistry::DisconnectAllAttributeObservers() {
  for (auto& it : meta_tag_attribute_observers_) {
    it.value->disconnect();
  }
  meta_tag_attribute_observers_.clear();
}

void FrameMetadataObserverRegistry::
    DisconnectAllPaidContentAttributeObservers() {
  for (auto& it : paid_content_attribute_observers_) {
    it.value->disconnect();
  }
  paid_content_attribute_observers_.clear();
}

void FrameMetadataObserverRegistry::ObserveMetaTagAttributes(
    HTMLMetaElement* meta) {
  if (meta_tag_attribute_observers_.Contains(meta)) {
    return;
  }

  auto* attribute_observer_delegate =
      MakeGarbageCollected<MetaTagAttributeObserver>(this);
  auto* attribute_observer =
      MutationObserver::Create(attribute_observer_delegate);

  MutationObserverInit* init = MutationObserverInit::Create();
  init->setAttributes(true);
  init->setAttributeFilter(Vector<String>{"name", "content"});
  DummyExceptionStateForTesting exception_state;
  attribute_observer->observe(meta, init, exception_state);
  DCHECK(!exception_state.HadException());

  meta_tag_attribute_observers_.Set(meta, attribute_observer);
}

void FrameMetadataObserverRegistry::ObservePaidContentScriptAttributes(
    HTMLScriptElement* script) {
  if (paid_content_attribute_observers_.Contains(script)) {
    return;
  }

  auto* attribute_observer_delegate =
      MakeGarbageCollected<PaidContentAttributeObserver>(this);
  auto* attribute_observer =
      MutationObserver::Create(attribute_observer_delegate);

  MutationObserverInit* init = MutationObserverInit::Create();
  init->setAttributes(true);
  init->setAttributeFilter(Vector<String>{"type"});
  init->setChildList(true);  // For text content changes.
  DummyExceptionStateForTesting exception_state;
  attribute_observer->observe(script, init, exception_state);
  DCHECK(!exception_state.HadException());

  paid_content_attribute_observers_.Set(script, attribute_observer);
}

void FrameMetadataObserverRegistry::StopObservingMetaTagAttributes(
    HTMLMetaElement* meta) {
  Member<MutationObserver> observer = meta_tag_attribute_observers_.Take(meta);
  DCHECK(observer);
  if (observer) {
    observer->disconnect();
  }
}

void FrameMetadataObserverRegistry::StopObservingPaidContentScriptAttributes(
    HTMLScriptElement* script) {
  Member<MutationObserver> observer =
      paid_content_attribute_observers_.Take(script);
  DCHECK(observer);
  if (observer) {
    observer->disconnect();
  }
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
  if (!UpdatePaidContentObserver()) {
    return;
  }
  PaidContent paid_content;
  bool paid_content_exists =
      paid_content.QueryPaidElements(*GetSupplementable());

  if (!paid_content_exists) {
    return;
  }

  for (auto& observer : paid_content_metadata_observers_) {
    observer->OnPaidContentMetadataChanged(paid_content_exists);
  }
}

void FrameMetadataObserverRegistry::OnMetaTagsChanged() {
  if (!UpdateMetaTagsObserver()) {
    return;
  }
  Document* document = GetSupplementable();
  HTMLHeadElement* head = document->head();
  HashMap<String, String> name_to_content_map;
  if (head) {
    for (HTMLMetaElement& meta :
         Traversal<HTMLMetaElement>::ChildrenOf(*head)) {
      const String& name = meta.GetName();
      if (!name.IsNull() && all_metatag_name_counts_.Contains(name)) {
        name_to_content_map.Set(name, meta.Content());
      }
    }
  }

  for (auto& it : remote_id_to_observer_data_) {
    mojo::RemoteSetElementId remote_id(it.key);
    const auto& names_to_find = it.value->names_to_observe;

    Vector<mojom::blink::MetaTagPtr> current_meta_tags;
    for (const String& name : names_to_find) {
      auto meta_it = name_to_content_map.find(name);
      if (meta_it != name_to_content_map.end()) {
        current_meta_tags.push_back(
            mojom::blink::MetaTag::New(name, meta_it->value));
      }
    }

    auto& last_sent_meta_tags = it.value->last_sent_meta_tags;
    if (mojo::Equals(last_sent_meta_tags, current_meta_tags)) {
      continue;
    }

    auto* observer = metatags_observers_.Get(remote_id);
    observer->OnMetaTagsChanged(mojo::Clone(current_meta_tags));
    last_sent_meta_tags = std::move(current_meta_tags);
  }
}

bool FrameMetadataObserverRegistry::UpdateMetaTagsObserver() {
  return UpdateObserver(GetSupplementable(), metatags_observers_,
                        meta_tags_mutation_observer_);
}

bool FrameMetadataObserverRegistry::UpdatePaidContentObserver() {
  return UpdateObserver(GetSupplementable(), paid_content_metadata_observers_,
                        paid_content_mutation_observer_);
}

void FrameMetadataObserverRegistry::DisconnectHandler(
    mojo::RemoteSetElementId id) {
  auto it = remote_id_to_observer_data_.find(id.value());
  // The disconnect handler should only be called for observers that have been
  // successfully added.
  CHECK(it != remote_id_to_observer_data_.end());

  // Remove the observer's names from the map of all observed names.
  const auto& names_to_remove = it->value->names_to_observe;
  for (const String& name : names_to_remove) {
    auto count_it = all_metatag_name_counts_.find(name);
    CHECK(count_it != all_metatag_name_counts_.end());
    CHECK_GE(count_it->value, 1);
    count_it->value--;
    if (count_it->value == 0) {
      all_metatag_name_counts_.erase(count_it);
    }
  }
  remote_id_to_observer_data_.erase(it);

  UpdateMetaTagsObserver();
}

void FrameMetadataObserverRegistry::PaidContentDisconnectHandler(
    mojo::RemoteSetElementId id) {
  UpdatePaidContentObserver();
}

namespace {

template <typename ObserverSet, typename MutationObserver>
bool UpdateObserver(Document* document,
                    ObserverSet& observer_set,
                    MutationObserver& mutation_observer) {
  if (observer_set.empty()) {
    mutation_observer->Disconnect();
    return false;
  }
  HTMLHeadElement* head = document->head();
  if (head) {
    mutation_observer->ObserveHead(head);
  } else if (document->documentElement()) {
    mutation_observer->ObserveDocument(document->documentElement());
  }
  return true;
}

}  // namespace

}  // namespace blink
