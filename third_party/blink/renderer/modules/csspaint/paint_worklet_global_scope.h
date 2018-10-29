// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_GLOBAL_SCOPE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_pending_generator_registry.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSPaintDefinition;
class ExceptionState;
class WorkerReportingProxy;

class MODULES_EXPORT PaintWorkletGlobalScope final : public WorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PaintWorkletGlobalScope);

 public:
  static PaintWorkletGlobalScope* Create(
      LocalFrame*,
      std::unique_ptr<GlobalScopeCreationParams>,
      WorkerReportingProxy&,
      PaintWorkletPendingGeneratorRegistry*,
      size_t global_scope_number);
  ~PaintWorkletGlobalScope() override;
  void Dispose() final;

  bool IsPaintWorkletGlobalScope() const final { return true; }
  void registerPaint(const String& name,
                     const ScriptValue& constructor_value,
                     ExceptionState&);

  CSSPaintDefinition* FindDefinition(const String& name);
  double devicePixelRatio() const;

  void Trace(blink::Visitor*) override;

 private:
  PaintWorkletGlobalScope(LocalFrame*,
                          std::unique_ptr<GlobalScopeCreationParams>,
                          WorkerReportingProxy&,
                          PaintWorkletPendingGeneratorRegistry*);

  // The implementation of the "paint definition" concept:
  // https://drafts.css-houdini.org/css-paint-api/#paint-definition
  typedef HeapHashMap<String, TraceWrapperMember<CSSPaintDefinition>>
      DefinitionMap;
  DefinitionMap paint_definitions_;

  Member<PaintWorkletPendingGeneratorRegistry> pending_generator_registry_;
};

template <>
struct DowncastTraits<PaintWorkletGlobalScope> {
  static bool AllowFrom(const ExecutionContext& context) {
    return context.IsPaintWorkletGlobalScope();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_GLOBAL_SCOPE_H_
