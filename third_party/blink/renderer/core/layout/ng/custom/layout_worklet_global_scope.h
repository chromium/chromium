// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_LAYOUT_WORKLET_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_LAYOUT_WORKLET_GLOBAL_SCOPE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/layout/ng/custom/pending_layout_registry.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSLayoutDefinition;
class V8NoArgumentConstructor;
class WorkerReportingProxy;

class CORE_EXPORT LayoutWorkletGlobalScope final : public WorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(LayoutWorkletGlobalScope);

 public:
  static LayoutWorkletGlobalScope* Create(
      LocalFrame*,
      std::unique_ptr<GlobalScopeCreationParams>,
      WorkerReportingProxy&,
      PendingLayoutRegistry*);

  LayoutWorkletGlobalScope(LocalFrame*,
                           std::unique_ptr<GlobalScopeCreationParams>,
                           WorkerReportingProxy&,
                           PendingLayoutRegistry*,
                           Agent*);
  ~LayoutWorkletGlobalScope() override;
  void Dispose() final;

  bool IsLayoutWorkletGlobalScope() const final { return true; }

  // Implements LayoutWorkletGlobalScope.idl
  void registerLayout(const AtomicString& name,
                      V8NoArgumentConstructor* layout_ctor,
                      ExceptionState&);

  CSSLayoutDefinition* FindDefinition(const AtomicString& name);

  void Trace(blink::Visitor*) override;

 private:
  // https://drafts.css-houdini.org/css-layout-api/#layout-definitions
  typedef HeapHashMap<String, Member<CSSLayoutDefinition>> DefinitionMap;
  DefinitionMap layout_definitions_;
  Member<PendingLayoutRegistry> pending_layout_registry_;
};

template <>
struct DowncastTraits<LayoutWorkletGlobalScope> {
  static bool AllowFrom(const ExecutionContext& context) {
    return context.IsLayoutWorkletGlobalScope();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_LAYOUT_WORKLET_GLOBAL_SCOPE_H_
