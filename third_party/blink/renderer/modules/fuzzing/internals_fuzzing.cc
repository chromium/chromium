// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/fuzzing/internals_fuzzing.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/testing/renderer_fuzzing_support.h"

namespace blink {

static void ResolvePromise(ScriptPromiseResolver<IDLUndefined>* resolver) {
  resolver->Resolve();
}

// static
ScriptPromise<IDLUndefined> InternalsFuzzing::runFuzzer(
    ScriptState* script_state,
    Internals&,
    const String& fuzzer_id,
    V8BufferSource* fuzzer_data) {
  auto* context = ExecutionContext::From(script_state);
  const uint8_t* bytes = nullptr;
  size_t num_bytes = 0;

  switch (fuzzer_data->GetContentType()) {
    case V8BufferSource::ContentType::kArrayBuffer: {
      DOMArrayBuffer* array = fuzzer_data->GetAsArrayBuffer();
      bytes = static_cast<uint8_t*>(array->Data());
      num_bytes = array->ByteLength();
      break;
    }
    case V8BufferSource::ContentType::kArrayBufferView: {
      const auto& view = fuzzer_data->GetAsArrayBufferView();
      bytes = static_cast<uint8_t*>(view->BaseAddress());
      num_bytes = view->byteLength();
      break;
    }
  }

  std::vector<uint8_t> data(bytes, bytes + num_bytes);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  RendererFuzzingSupport::Run(
      &context->GetBrowserInterfaceBroker(),
      Platform::Current()->GetBrowserInterfaceBroker(), fuzzer_id.Utf8(),
      std::move(data),
      WTF::BindOnce(&ResolvePromise, WrapPersistent(resolver)));

  return promise;
}

}  // namespace blink
