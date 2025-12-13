// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/fuzzing/internals_fuzzing.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/testing/renderer_fuzzing_support.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

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
  base::span<const uint8_t> data_span;

  switch (fuzzer_data->GetContentType()) {
    case V8BufferSource::ContentType::kArrayBuffer: {
      DOMArrayBuffer* array = fuzzer_data->GetAsArrayBuffer();
      data_span = array->ByteSpan();
      break;
    }
    case V8BufferSource::ContentType::kArrayBufferView: {
      const auto& view = fuzzer_data->GetAsArrayBufferView();
      data_span = view->ByteSpan();
      break;
    }
  }

  std::vector<uint8_t> data = base::ToVector(data_span);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  AssociatedInterfaceProvider* associated_provider = nullptr;
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    if (auto* frame = window->GetFrame()) {
      associated_provider = frame->GetRemoteNavigationAssociatedInterfaces();
    }
  }

  RendererFuzzingSupport::Run(
      &context->GetBrowserInterfaceBroker(),
      Platform::Current()->GetBrowserInterfaceBroker(), associated_provider,
      fuzzer_id.Utf8(), std::move(data),
      BindOnce(&ResolvePromise, WrapPersistent(resolver)));

  return promise;
}

}  // namespace blink
