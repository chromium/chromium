// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ACTIVE_TEMPLATE_MUTATION_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ACTIVE_TEMPLATE_MUTATION_SCOPE_H_

#include <utility>

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_record_active_template_status.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class HTMLTemplateElement;
class MutationObserver;
class Node;
class ScriptState;

// Per-(invocation, observer) filtered attribute snapshot and cached frozen,
// null-prototype JS object.
class CORE_EXPORT ActiveTemplateFilterEntry final
    : public GarbageCollected<ActiveTemplateFilterEntry> {
 public:
  explicit ActiveTemplateFilterEntry(
      Vector<std::pair<AtomicString, AtomicString>> filtered_attributes);

  ScriptObject GetOrCreateObject(ScriptState* script_state);

  void Trace(Visitor* visitor) const;

 private:
  using WorldObjectMap = GCedHeapHashMap<Member<const DOMWrapperWorld>,
                                         TraceWrapperV8Reference<v8::Object>>;

  Vector<std::pair<AtomicString, AtomicString>> filtered_attributes_;
  Member<const DOMWrapperWorld> primary_world_;
  TraceWrapperV8Reference<v8::Object> primary_object_;
  Member<WorldObjectMap> other_worlds_;
};

// Snapshots a <template for> element's attributes at patch start (or abort)
// without retaining the HTMLTemplateElement itself.
class CORE_EXPORT ActiveTemplateInvocation final
    : public GarbageCollected<ActiveTemplateInvocation> {
 public:
  static ActiveTemplateInvocation* Create(
      const HTMLTemplateElement* template_element,
      const Node* target_node);

  explicit ActiveTemplateInvocation(
      Vector<std::pair<AtomicString, AtomicString>> attributes);

  ActiveTemplateFilterEntry* GetOrCreateEntry(MutationObserver& observer,
                                              Node& target);

  void Trace(Visitor* visitor) const;

 private:
  Vector<std::pair<AtomicString, AtomicString>> attributes_;
  HeapHashMap<WeakMember<MutationObserver>, Member<ActiveTemplateFilterEntry>>
      entries_;
};

// Marks direct HTML parser DOM mutations belonging to an active <template for>
// invocation. Nested scopes override outer ones (an invocation of nullptr
// shadows any outer scope, e.g. for inert <template> content or suspended JS
// execution).
//
// The current scope is tracked in a static rather than threaded through the
// insertion paths, so anything that can run author script synchronously while a
// scope is live (event dispatch, custom element reactions, post-insertion
// steps, frame disconnection, ...) must open a SuspendScope. Otherwise
// mutations made by that script would be attributed to the template.
class CORE_EXPORT ActiveTemplateMutationScope final {
  STACK_ALLOCATED();

 public:
  // Hides the current scope, if any, for the lifetime of this object.
  class CORE_EXPORT SuspendScope final {
    STACK_ALLOCATED();

   public:
    SuspendScope() : previous_(current_) { current_ = nullptr; }
    SuspendScope(const SuspendScope&) = delete;
    SuspendScope& operator=(const SuspendScope&) = delete;
    ~SuspendScope() { current_ = previous_; }

   private:
    ActiveTemplateMutationScope* previous_;
  };

  ActiveTemplateMutationScope(V8MutationRecordActiveTemplateStatus::Enum status,
                              ActiveTemplateInvocation* invocation);
  ActiveTemplateMutationScope(const ActiveTemplateMutationScope&) = delete;
  ActiveTemplateMutationScope& operator=(const ActiveTemplateMutationScope&) =
      delete;
  ~ActiveTemplateMutationScope() { current_ = previous_; }

  static ActiveTemplateMutationScope* Current() { return current_; }

  V8MutationRecordActiveTemplateStatus::Enum status() const { return status_; }
  ActiveTemplateInvocation* invocation() const { return invocation_; }
  void MarkUsed() { used_ = true; }
  bool WasUsed() const { return used_; }

 private:
  static ActiveTemplateMutationScope* current_;

  ActiveTemplateMutationScope* previous_;
  V8MutationRecordActiveTemplateStatus::Enum status_;
  ActiveTemplateInvocation* invocation_;
  bool used_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ACTIVE_TEMPLATE_MUTATION_SCOPE_H_
