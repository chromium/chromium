// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_EXECUTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_EXECUTION_H_

#include <map>
#include <memory>
#include <utility>

#include "mojo/public/cpp/system/buffer.h"
#include "services/ml/public/interfaces/compilation.mojom-blink.h"
#include "services/ml/public/interfaces/execution.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct OperandInfo {
  OperandInfo(uint32_t offset,
              uint32_t length,
              mojo::ScopedSharedBufferMapping mapping)
      : offset(offset), length(length), mapping(std::move(mapping)) {}
  uint32_t offset;
  uint32_t length;
  mojo::ScopedSharedBufferMapping mapping;
};

class Execution final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Execution(ml::mojom::blink::ExecutionInitParamsPtr);
  ~Execution() override;

  void setInput(uint32_t, MaybeShared<DOMArrayBufferView>, ExceptionState&);
  void setOutput(uint32_t, MaybeShared<DOMArrayBufferView>, ExceptionState&);
  ScriptPromise startCompute(ScriptState*);

  void Trace(blink::Visitor*) override;

 private:
  void OnResultCode(ScriptPromiseResolver*, const String&, int32_t);
  void OnStartCompute(ScriptPromiseResolver*, int32_t);
  void OnConnectionError();

  ml::mojom::blink::ExecutionPtr execution_;
  mojo::ScopedSharedBufferHandle memory_;
  WTF::Vector<std::unique_ptr<OperandInfo>> inputs_;
  WTF::Vector<std::unique_ptr<OperandInfo>> outputs_;

  HeapHashSet<Member<ScriptPromiseResolver>> requests_;
  std::map<uint32_t, mojo::ScopedSharedBufferHandle> input_shared_buffers_;
  std::map<uint32_t, mojo::ScopedSharedBufferHandle> output_shared_buffers_;
  HeapVector<Member<DOMArrayBufferView>> output_buffer_views_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_EXECUTION_H_
