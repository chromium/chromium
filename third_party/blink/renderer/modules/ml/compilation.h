// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Compilation_h
#define Compilation_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "platform/bindings/ScriptWrappable.h"
#include "services/ml/public/interfaces/compilation.mojom-blink.h"
#include "services/ml/public/interfaces/constants.mojom-blink.h"

namespace blink {

class Compilation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
 public:
  Compilation(ml::mojom::blink::CompilationPtrInfo);
  ~Compilation() override;

  ScriptPromise setPreference(ScriptState*, int32_t);
  ScriptPromise finish(ScriptState*);
  ScriptPromise createExecution(ScriptState*);

  void Trace(blink::Visitor*);

 private:
  void OnResultCode(ScriptPromiseResolver*, const String&, int32_t);
  void OnCreateExecution(ScriptPromiseResolver*, int32_t, ml::mojom::blink::ExecutionInitParamsPtr);
  void OnConnectionError();

 private:
  ml::mojom::blink::CompilationPtr compilation_;
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;
};

}  // namespace blink

#endif  // Compilation_h