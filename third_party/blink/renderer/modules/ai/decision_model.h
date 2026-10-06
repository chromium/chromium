// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_DECISION_MODEL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_DECISION_MODEL_H_

#include "third_party/blink/public/mojom/ai/ai_decision_model.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_availability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_create_core_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decision_model_decide_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_performance_preference.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DecisionModelDecision;

// Web-exposed Built-In AI DecisionModel session bound to
// `blink::mojom::blink::AIDecisionModel`.
class MODULES_EXPORT DecisionModel final : public ScriptWrappable,
                                           public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DecisionModel(
      ScriptState* script_state,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      mojo::PendingRemote<mojom::blink::AIDecisionModel> pending_remote,
      DecisionModelCreateOptions* options);
  ~DecisionModel() override = default;

  void Trace(Visitor* visitor) const override;

  // decision_model.idl implementation.
  static ScriptPromise<V8Availability> availability(
      ScriptState* script_state,
      DecisionModelCreateCoreOptions* options,
      ExceptionState& exception_state);
  static ScriptPromise<DecisionModel> create(
      ScriptState* script_state,
      DecisionModelCreateOptions* options,
      ExceptionState& exception_state);
  ScriptPromise<IDLRecord<IDLString, DecisionModelDecision>> decide(
      ScriptState* script_state,
      const String& input,
      const DecisionModelDecideOptions* options,
      ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);
  V8PerformancePreference preference() const { return preference_; }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::AIDecisionModel> decision_model_remote_;
  V8PerformancePreference preference_{V8PerformancePreference::Enum::kAuto};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_DECISION_MODEL_H_
