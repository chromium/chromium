
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_PROOFREADER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_PROOFREADER_H_

#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/ai_proofreader.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_correction_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_proofread_correction.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_proofread_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_proofreader_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_proofreader_proofread_options.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/availability.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

using CanCreateCallback =
    base::OnceCallback<void(mojom::blink::ModelAvailabilityCheckResult)>;

// Describe an error in the original string that is corrected in the new string.
struct Correction {
  uint32_t error_start;
  uint32_t error_end;
  uint32_t correction_start;
  uint32_t correction_end;
  String correction;
};

// The class that represents a proofreader object.
class Proofreader final : public ScriptWrappable,
                          public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Proofreader(ScriptState* script_state,
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
  ScriptPromise<ProofreadResult> proofread(
      ScriptState* script_state,
      const String& proofread_task,
      const ProofreaderProofreadOptions* options,
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
  void DestroyImpl();
  void OnCreateAbortSignalAborted(ScriptState* script_state);
  AbortSignal* CreateCompositeSignal(
      ScriptState* script_state,
      const ProofreaderProofreadOptions* options);
  static bool ValidateAndCanonicalizeOptionLanguages(
      v8::Isolate* isolate,
      ProofreaderCreateCoreOptions* options);

  // Callback to resolve the `proofread()` promise with the full corrected text.
  void OnProofreadComplete(
      ScriptPromiseResolver<ProofreadResult>* resolver,
      ScriptState* script_state,
      AbortSignal* signal,
      const String& input,
      const String& corrected_input,
      mojom::blink::ModelExecutionContextInfoPtr context_info);

  void OnProofreadError(ScriptPromiseResolver<ProofreadResult>* resolver,
                        DOMException* exception);

  void OnProofreadAbort(ScriptPromiseResolver<ProofreadResult>* resolver,
                        AbortSignal* signal,
                        ScriptState* script_state);

  // Recursively fetch correction type labels for all corrections.
  // `correction_index` is the next correction to fetch the label for.
  // `raw_corrections` is passed to help annotate the error and correction from
  // the original input and the corrected input.
  void GetCorrectionTypes(ScriptPromiseResolver<ProofreadResult>* resolver,
                          ScriptState* script_state,
                          AbortSignal* signal,
                          ProofreadResult* proofread_result,
                          Vector<Correction> raw_corrections,
                          const String& input,
                          uint32_t correction_index);

  void OnLabelComplete(ScriptPromiseResolver<ProofreadResult>* resolver,
                       ScriptState* script_state,
                       AbortSignal* signal,
                       ProofreadResult* result,
                       Vector<Correction> raw_corrections,
                       const String& input,
                       uint32_t correction_index,
                       const String& label,
                       mojom::blink::ModelExecutionContextInfoPtr context_info);

  HeapMojoRemote<mojom::blink::AIProofreader> remote_;
  Member<ProofreaderCreateOptions> options_;
  Member<AbortController> destruction_abort_controller_;
  Member<AbortSignal> create_abort_signal_;
  Member<AbortSignal::AlgorithmHandle> create_abort_handle_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

// Get the corrections made on `input` that would produce `corrected_input`.
MODULES_EXPORT
Vector<Correction> GetCorrections(const String& input,
                                  const String& corrected_input);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_PROOFREADER_H_
