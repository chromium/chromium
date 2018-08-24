// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_COMPILATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_COMPILATION_H_

#include "services/ml/public/interfaces/compilation.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Compilation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Compilation(ml::mojom::blink::CompilationPtrInfo);
  ~Compilation() override;

  void setPreference(int32_t, ExceptionState&);
  ScriptPromise finish(ScriptState*);
  ScriptPromise createExecution(ScriptState*);

  void Trace(blink::Visitor*) override;

 private:
  void OnResultCode(ScriptPromiseResolver*, const String&, int32_t);
  void OnCreateExecution(ScriptPromiseResolver*,
                         int32_t,
                         ml::mojom::blink::ExecutionInitParamsPtr);
  void OnConnectionError();

  bool is_finished_;
  int32_t preference_;
  ml::mojom::blink::CompilationPtr compilation_;
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_COMPILATION_H_
