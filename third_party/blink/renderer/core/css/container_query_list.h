// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_LIST_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/css/container_selector.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"

namespace blink {
class ExecutionContext;
class Element;
class ContainerQuery;

class CORE_EXPORT ContainerQueryList final
    : public EventTarget,
      public ActiveScriptWrappable<ContainerQueryList>,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ContainerQueryList(ExecutionContext*, ContainerQuery*, Element* element);
  ContainerQueryList(const ContainerQueryList&) = delete;
  ContainerQueryList& operator=(const ContainerQueryList&) = delete;
  ~ContainerQueryList() override;

  bool matches();

  void Trace(Visitor*) const override;

  bool HasPendingActivity() const final;

  void ContextDestroyed() override;

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

 private:
  void UpdateMatches();
  Element* ResolveContainer();

  bool matches_ = false;
  Member<ContainerQuery> container_query_;
  Member<Element> element_;
  WeakMember<Element> container_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_LIST_H_
