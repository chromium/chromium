// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITER_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/ai_writer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blink {

class AIWriterWriteOptions;
class ExecutionContext;
class ReadableStream;

// The class that represents a writer object.
class AIWriter final : public ScriptWrappable, public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AIWriter(ExecutionContext* execution_context,
           scoped_refptr<base::SequencedTaskRunner> task_runner,
           mojo::PendingRemote<mojom::blink::AIWriter> pending_remote,
           const String& shared_context_string);
  void Trace(Visitor* visitor) const override;

  // ai_writer.idl implementation.
  ScriptPromise<IDLString> write(ScriptState* script_state,
                                 const String& input,
                                 const AIWriterWriteOptions* options,
                                 ExceptionState& exception_state);
  ReadableStream* writeStreaming(ScriptState* script_state,
                                 const String& input,
                                 const AIWriterWriteOptions* options,
                                 ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);
  String sharedContext() const { return shared_context_string_; }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::AIWriter> remote_;
  const String shared_context_string_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITER_H_
