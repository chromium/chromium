// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_SEMANTIC_EMBEDDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_SEMANTIC_EMBEDDER_H_

#include <cstdint>

#include "third_party/blink/public/mojom/ai/ai_semantic_embedder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_availability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_semantic_embedder_embed_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class SemanticEmbedderCreateCoreOptions;
class SemanticEmbedderResult;

class MODULES_EXPORT SemanticEmbedder final : public ScriptWrappable,
                                              public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SemanticEmbedder(
      ScriptState* script_state,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      mojo::PendingRemote<mojom::blink::AISemanticEmbedder> pending_remote,
      SemanticEmbedderCreateOptions* options);
  ~SemanticEmbedder() override = default;

  void Trace(Visitor* visitor) const override;

  // semantic_embedder.idl implementation.
  static ScriptPromise<V8Availability> availability(
      ScriptState* script_state,
      SemanticEmbedderCreateCoreOptions* options,
      ExceptionState& exception_state);

  static ScriptPromise<SemanticEmbedder> create(
      ScriptState* script_state,
      SemanticEmbedderCreateOptions* options,
      ExceptionState& exception_state);

  ScriptPromise<SemanticEmbedderResult> embed(
      ScriptState* script_state,
      const V8SemanticEmbedderInputs* input,
      const SemanticEmbedderEmbedOptions* options,
      ExceptionState& exception_state);

  void destroy(ScriptState* script_state, ExceptionState& exception_state);

  const String& embeddingSpace() const { return embedding_space_; }

  // Asks the browser to drop the in-flight embed() call `request_id` (an id
  // handed out by embed()). Called when its AbortSignal fires.
  void CancelEmbed(uint64_t request_id);
  // Called once per embed() call when the browser replies or the pipe closes.
  void OnEmbedRequestSettled();

  // embed() calls beyond this many in flight reject with QuotaExceededError.
  static constexpr int kMaxInFlightEmbedRequests = 32;

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::AISemanticEmbedder> embedder_remote_;
  // Always set; the value passed to create().
  const String embedding_space_;
  // Ids for embed() calls, unique per embedder.
  uint64_t next_request_id_ = 0;
  int in_flight_embed_requests_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_SEMANTIC_EMBEDDER_H_
