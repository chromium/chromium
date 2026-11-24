// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/active_template_mutation_scope.h"

#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/attribute_collection.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

ActiveTemplateMutationScope* ActiveTemplateMutationScope::current_ = nullptr;

ActiveTemplateFilterEntry::ActiveTemplateFilterEntry(
    Vector<std::pair<AtomicString, AtomicString>> filtered_attributes)
    : filtered_attributes_(std::move(filtered_attributes)) {}

ScriptObject ActiveTemplateFilterEntry::GetOrCreateObject(
    ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  const DOMWrapperWorld& world = script_state->World();
  if (primary_world_ == &world && !primary_object_.IsEmpty()) {
    return ScriptObject(isolate, primary_object_.Get(isolate));
  }
  if (other_worlds_) {
    auto it = other_worlds_->find(&world);
    if (it != other_worlds_->end() && !it->value.IsEmpty()) {
      return ScriptObject(isolate, it->value.Get(isolate));
    }
  }

  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Context::Scope context_scope(context);

  Vector<v8::Local<v8::Name>> names;
  Vector<v8::Local<v8::Value>> values;
  names.reserve(filtered_attributes_.size());
  values.reserve(filtered_attributes_.size());
  for (const auto& [name, value] : filtered_attributes_) {
    names.push_back(V8AtomicString(isolate, name));
    values.push_back(V8String(isolate, value));
  }

  v8::Local<v8::Object> object = v8::Object::New(
      isolate, v8::Null(isolate), names.data(), values.data(), names.size());
  bool frozen = false;
  if (!object->SetIntegrityLevel(context, v8::IntegrityLevel::kFrozen)
           .To(&frozen) ||
      !frozen) {
    return ScriptObject::CreateNull(isolate);
  }

  if (!primary_world_) {
    primary_world_ = &world;
    primary_object_.Reset(isolate, object);
  } else {
    if (!other_worlds_) {
      other_worlds_ = MakeGarbageCollected<WorldObjectMap>();
    }
    other_worlds_->Set(&world,
                       TraceWrapperV8Reference<v8::Object>(isolate, object));
  }
  return ScriptObject(isolate, object);
}

void ActiveTemplateFilterEntry::Trace(Visitor* visitor) const {
  visitor->Trace(primary_world_);
  visitor->Trace(primary_object_);
  visitor->Trace(other_worlds_);
}

// static
ActiveTemplateInvocation* ActiveTemplateInvocation::Create(
    const HTMLTemplateElement* template_element,
    const Node* target_node) {
  if (!template_element ||
      !RuntimeEnabledFeatures::MutationObserverActiveTemplateEnabled()) {
    return nullptr;
  }

  const bool hide_nonce =
      target_node && target_node->InActiveDocument() &&
      target_node->GetExecutionContext() &&
      target_node->GetExecutionContext()->GetContentSecurityPolicy() &&
      target_node->GetExecutionContext()
          ->GetContentSecurityPolicy()
          ->HasHeaderDeliveredPolicy();

  AttributeCollection collection = template_element->AttributesWithoutUpdate();
  Vector<std::pair<AtomicString, AtomicString>> attributes;
  attributes.reserve(collection.size());
  for (const Attribute& attr : collection) {
    if (!attr.NamespaceURI().IsNull()) {
      continue;
    }
    const AtomicString& value =
        (hide_nonce && attr.GetName() == html_names::kNonceAttr) ? g_empty_atom
                                                                 : attr.Value();
    attributes.emplace_back(attr.LocalName(), value);
  }
  return MakeGarbageCollected<ActiveTemplateInvocation>(std::move(attributes));
}

ActiveTemplateInvocation::ActiveTemplateInvocation(
    Vector<std::pair<AtomicString, AtomicString>> attributes)
    : attributes_(std::move(attributes)) {}

ActiveTemplateFilterEntry* ActiveTemplateInvocation::GetOrCreateEntry(
    MutationObserver& observer,
    Node& target) {
  auto it = entries_.find(&observer);
  if (it != entries_.end()) {
    return it->value.Get();
  }

  const HashSet<AtomicString>* single_filter =
      observer.GetSingleActiveTemplateAttributeFilter();
  HashSet<AtomicString> combined_filter;
  const HashSet<AtomicString>* effective_filter = single_filter;
  if (!effective_filter) {
    target.CollectActiveTemplateAttributeFilter(observer, combined_filter);
    effective_filter = &combined_filter;
  }

  Vector<std::pair<AtomicString, AtomicString>> filtered;
  if (!effective_filter->empty()) {
    for (const auto& [name, value] : attributes_) {
      if (effective_filter->Contains(name)) {
        filtered.emplace_back(name, value);
      }
    }
  }

  auto* entry =
      MakeGarbageCollected<ActiveTemplateFilterEntry>(std::move(filtered));
  entries_.insert(&observer, entry);
  return entry;
}

void ActiveTemplateInvocation::Trace(Visitor* visitor) const {
  visitor->Trace(entries_);
}

ActiveTemplateMutationScope::ActiveTemplateMutationScope(
    V8MutationRecordActiveTemplateStatus::Enum status,
    ActiveTemplateInvocation* invocation)
    : previous_(current_),
      status_(status),
      invocation_(
          RuntimeEnabledFeatures::MutationObserverActiveTemplateEnabled()
              ? invocation
              : nullptr) {
  DCHECK(IsMainThread());
  current_ = invocation_ ? this : nullptr;
}

}  // namespace blink
