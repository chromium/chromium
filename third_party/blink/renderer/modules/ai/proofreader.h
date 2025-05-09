// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_PROOFREADER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_PROOFREADER_H_

#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/ai_proofreader.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_proofread_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_proofreader_create_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/availability.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

using CanCreateCallback =
    base::OnceCallback<void(mojom::blink::ModelAvailabilityCheckResult)>;

// The class that represents a proofreader object.
class Proofreader final : public ScriptWrappable,
                          public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Proofreader(ExecutionContext* execution_context,
              scoped_refptr<base::SequencedTaskRunner> task_runner,
              mojo::PendingRemote<mojom::blink::AIProofreader> pending_remote,
              ProofreaderCreateOptions* options);

  void Trace(Visitor* visitor) const override;

  static ScriptPromise<V8Availability> availability(
      ScriptState* script_state,
      ProofreaderCreateCoreOptions* options,
      ExceptionState& exception_state);

  static ScriptPromise<Proofreader> create(ScriptState* script_state,
                                           ProofreaderCreateOptions* options,
                                           ExceptionState& exception_state);

  // proofreader.idl:
  ScriptPromise<ProofreadResult> proofread(ScriptState* script_state,
                                           const String& proofread_task,
                                           ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);

  bool includeCorrectionTypes() const {
    return options_->includeCorrectionTypes();
  }

  bool includeCorrectionExplanations() const {
    return options_->includeCorrectionExplanations();
  }

  std::optional<Vector<String>> expectedInputLanguages() const {
    if (options_->hasExpectedInputLanguages()) {
      return options_->expectedInputLanguages();
    }
    return std::nullopt;
  }

  String correctionExplanationLanguage() const {
    return options_->getCorrectionExplanationLanguageOr(g_empty_string);
  }

 private:
  HeapMojoRemote<mojom::blink::AIProofreader> remote_;
  Member<ProofreaderCreateOptions> options_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_PROOFREADER_H_
