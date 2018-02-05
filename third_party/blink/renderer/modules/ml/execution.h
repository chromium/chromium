// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Execution_h
#define Execution_h

#include "bindings/core/v8/ScriptPromise.h"
#include "platform/bindings/ScriptWrappable.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "services/ml/public/interfaces/execution.mojom-blink.h"
#include "services/ml/public/interfaces/constants.mojom-blink.h"

namespace blink {

class Execution final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
 public:
  Execution(ml::mojom::blink::ExecutionPtrInfo);
  ~Execution() override;

  ScriptPromise setInput(ScriptState*, uint32_t, MaybeShared<DOMArrayBufferView>);
  ScriptPromise setOutput(ScriptState*, uint32_t, MaybeShared<DOMArrayBufferView>);
  ScriptPromise startCompute(ScriptState*);

  void Trace(blink::Visitor*);

 private:
  void OnResultCode(ScriptPromiseResolver*, const String&, int32_t);
  void OnStartCompute(ScriptPromiseResolver*, int32_t);
  void OnConnectionError();

 private:
  ml::mojom::blink::ExecutionPtr execution_;
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;
  std::map<uint32_t, mojo::ScopedSharedBufferHandle> input_shared_buffers_;
  std::map<uint32_t, mojo::ScopedSharedBufferHandle> output_shared_buffers_;
  HeapHashMap<uint32_t, Member<DOMArrayBufferView>> output_buffer_views_;
};

}  // namespace blink

#endif  // Execution_h