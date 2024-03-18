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
class WorkletOptions;

class MODULES_EXPORT SharedStorage final
    : public ScriptWrappable,
      public PairAsyncIterable<SharedStorage> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SharedStorage();
  ~SharedStorage() override;

  void Trace(Visitor*) const override;

  // SharedStorage IDL
  ScriptPromiseTyped<IDLAny> set(ScriptState*,
                                 const String& key,
                                 const String& value,
                                 ExceptionState&);
  ScriptPromiseTyped<IDLAny> set(ScriptState*,
                                 const String& key,
                                 const String& value,
                                 const SharedStorageSetMethodOptions* options,
                                 ExceptionState&);
  ScriptPromiseTyped<IDLAny> append(ScriptState*,
                                    const String& key,
                                    const String& value,
                                    ExceptionState&);
  ScriptPromiseTyped<IDLAny> Delete(ScriptState*,
                                    const String& key,
                                    ExceptionState&);
  ScriptPromiseTyped<IDLAny> clear(ScriptState*, ExceptionState&);
  ScriptPromiseTyped<IDLString> get(ScriptState*,
                                    const String& key,
                                    ExceptionState&);
  ScriptPromiseTyped<IDLUnsignedLong> length(ScriptState*, ExceptionState&);
  ScriptPromiseTyped<IDLDouble> remainingBudget(ScriptState*, ExceptionState&);
  ScriptValue context(ScriptState*, ExceptionState&) const;
  ScriptPromiseTyped<V8SharedStorageResponse> selectURL(
      ScriptState*,
      const String& name,
      HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
      ExceptionState&);
  ScriptPromiseTyped<V8SharedStorageResponse> selectURL(
      ScriptState*,
      const String& name,
      HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
      const SharedStorageRunOperationMethodOptions* options,
      ExceptionState&);
  ScriptPromiseTyped<IDLAny> run(ScriptState*,
                                 const String& name,
                                 ExceptionState&);
  ScriptPromiseTyped<IDLAny> run(
      ScriptState*,
      const String& name,
      const SharedStorageRunOperationMethodOptions* options,
      ExceptionState&);
  ScriptPromiseTyped<SharedStorageWorklet> createWorklet(
      ScriptState*,
      const String& module_url,
      const WorkletOptions* options,
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
