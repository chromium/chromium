// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_text_session_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
class AITextSession;
class V8AIModelAvailability;

// The class that manages the exposed model APIs that load model assets and
// create AITextSession.
class AI final : public ScriptWrappable, public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // LINT.IfChange(AIModelAvailability)
  enum class ModelAvailability {
    kReadily = 0,
    kAfterDownload = 1,
    kNo = 2,

    kMaxValue = kNo,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ai/enums.xml:AIModelAvailability)

  explicit AI(ExecutionContext* context);
  ~AI() override = default;

  void Trace(Visitor* visitor) const override;

  // model_manager.idl implementation.
  ScriptPromise<V8AIModelAvailability> canCreateTextSession(
      ScriptState* script_state,
      ExceptionState& exception_state);
  ScriptPromise<AITextSession> createTextSession(
      ScriptState* script_state,
      AITextSessionOptions* options,
      ExceptionState& exception_state);
  ScriptPromise<AITextSessionOptions> defaultTextSessionOptions(
      ScriptState* script_state,
      ExceptionState& exception_state);

 private:
  HeapMojoRemote<mojom::blink::AIManager>& GetAIRemote();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::AIManager> ai_remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_H_
