// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Execution_h
#define Execution_h

#include "bindings/core/v8/ScriptPromise.h"
#include "platform/bindings/ScriptWrappable.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/DOMTypedArray.h"

namespace blink {

class Compilation;
class ExceptionState;

class Execution final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
 public:
  Execution(Compilation*);
  ~Execution() override;

  void setInput(uint32_t index, MaybeShared<DOMArrayBufferView> data, ExceptionState& state);
  void setOutput(uint32_t index, MaybeShared<DOMArrayBufferView> data, ExceptionState& state);
  ScriptPromise startCompute(ScriptState*);

  void Trace(blink::Visitor*);
};

}  // namespace blink

#endif  // Execution_h