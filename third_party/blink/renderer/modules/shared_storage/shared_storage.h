// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_H_

#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/async_iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_async_iterator_shared_storage.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class ScriptState;
class SharedStorageWorklet;
class SharedStorageSetMethodOptions;
class SharedStorageRunOperationMethodOptions;
class SharedStorageUrlWithMetadata;
class SharedStorageWorklet;
class SharedStorageWorkletOptions;

class MODULES_EXPORT SharedStorage final
    : public ScriptWrappable,
      public PairAsyncIterable<SharedStorage> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SharedStorage();
  ~SharedStorage() override;

  void Trace(Visitor*) const override;

  // SharedStorage IDL
  ScriptPromise<IDLAny> set(ScriptState*,
                            const String& key,
                            const String& value,
                            ExceptionState&);
  ScriptPromise<IDLAny> set(ScriptState*,
                            const String& key,
                            const String& value,
                            const SharedStorageSetMethodOptions* options,
                            ExceptionState&);
  ScriptPromise<IDLAny> append(ScriptState*,
                               const String& key,
                               const String& value,
                               ExceptionState&);
  ScriptPromise<IDLAny> Delete(ScriptState*,
                               const String& key,
                               ExceptionState&);
  ScriptPromise<IDLAny> clear(ScriptState*, ExceptionState&);
  ScriptPromise<IDLString> get(ScriptState*,
                               const String& key,
                               ExceptionState&);
  ScriptPromise<IDLUnsignedLong> length(ScriptState*, ExceptionState&);
  ScriptPromise<IDLDouble> remainingBudget(ScriptState*, ExceptionState&);
  ScriptValue context(ScriptState*, ExceptionState&) const;
  ScriptPromise<V8SharedStorageResponse> selectURL(
      ScriptState*,
      const String& name,
      HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
      ExceptionState&);
  ScriptPromise<V8SharedStorageResponse> selectURL(
      ScriptState*,
      const String& name,
      HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
      const SharedStorageRunOperationMethodOptions* options,
      ExceptionState&);
  ScriptPromise<IDLAny> run(ScriptState*, const String& name, ExceptionState&);
  ScriptPromise<IDLAny> run(
      ScriptState*,
      const String& name,
      const SharedStorageRunOperationMethodOptions* options,
      ExceptionState&);
  ScriptPromise<SharedStorageWorklet> createWorklet(
      ScriptState*,
      const String& module_url,
      const SharedStorageWorkletOptions* options,
      ExceptionState&);
  SharedStorageWorklet* worklet(ScriptState*, ExceptionState&);

 private:
  class IterationSource;

  // PairAsyncIterable<SharedStorage> overrides:
  PairAsyncIterable<SharedStorage>::IterationSource* CreateIterationSource(
      ScriptState* script_state,
      typename PairAsyncIterable<SharedStorage>::IterationSource::Kind kind,
      ExceptionState& exception_state) override;

  Member<SharedStorageWorklet> shared_storage_worklet_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_H_
