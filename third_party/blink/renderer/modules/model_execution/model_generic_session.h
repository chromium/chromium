// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_MODEL_GENERIC_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_MODEL_GENERIC_SESSION_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

// The class that represents a session with simple generic model execution.
class ModelGenericSession final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit ModelGenericSession(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~ModelGenericSession() override = default;

  void Trace(Visitor* visitor) const override;

  mojo::PendingReceiver<blink::mojom::blink::ModelGenericSession>
  GetModelSessionReceiver();

  // model_generic_session.idl implementation.
  ScriptPromise execute(ScriptState* script_state,
                        const WTF::String& input,
                        ExceptionState& exception_state);
  ReadableStream* executeStreaming(ScriptState* script_state,
                                   const WTF::String& input,
                                   ExceptionState& exception_state);

 private:
  class Responder;
  class StreamingResponder;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<blink::mojom::blink::ModelGenericSession>
      model_session_remote_{nullptr};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_MODEL_GENERIC_SESSION_H_
