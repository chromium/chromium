// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_H_

#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom-blink.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom-blink.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/async_iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_async_iterator_shared_storage.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class ScriptState;
class SharedStorageWorklet;
class SharedStorageSetMethodOptions;
class SharedStorageRunOperationMethodOptions;
class SharedStorageUrlWithMetadata;

class MODULES_EXPORT SharedStorage final
    : public ScriptWrappable,
      public PairAsyncIterable<SharedStorage> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SharedStorage();
  ~SharedStorage() override;

  void Trace(Visitor*) const override;

  // SharedStorage IDL
  ScriptPromise set(ScriptState*,
                    const String& key,
                    const String& value,
                    ExceptionState&);
  ScriptPromise set(ScriptState*,
                    const String& key,
                    const String& value,
                    const SharedStorageSetMethodOptions* options,
                    ExceptionState&);
  ScriptPromise append(ScriptState*,
                       const String& key,
                       const String& value,
                       ExceptionState&);
  ScriptPromise Delete(ScriptState*, const String& key, ExceptionState&);
  ScriptPromise clear(ScriptState*, ExceptionState&);
  ScriptPromise get(ScriptState*, const String& key, ExceptionState&);
  ScriptPromise length(ScriptState*, ExceptionState&);
  ScriptPromise remainingBudget(ScriptState*, ExceptionState&);
  ScriptValue context(ScriptState*, ExceptionState&) const;
  ScriptPromise selectURL(ScriptState*,
                          const String& name,
                          HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
                          ExceptionState&);
  ScriptPromise selectURL(ScriptState*,
                          const String& name,
                          HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
                          const SharedStorageRunOperationMethodOptions* options,
                          ExceptionState&);
  ScriptPromise run(ScriptState*, const String& name, ExceptionState&);
  ScriptPromise run(ScriptState*,
                    const String& name,
                    const SharedStorageRunOperationMethodOptions* options,
                    ExceptionState&);
  SharedStorageWorklet* worklet(ScriptState*, ExceptionState&);

  mojom::blink::SharedStorageDocumentService* GetSharedStorageDocumentService(
      ExecutionContext* execution_context);

  mojom::blink::SharedStorageWorkletServiceClient*
  GetSharedStorageWorkletServiceClient(ExecutionContext* execution_context);

 private:
  class IterationSource;

  // PairAsyncIterable<SharedStorage> overrides:
  PairAsyncIterable<SharedStorage>::IterationSource* CreateIterationSource(
      ScriptState* script_state,
      typename PairAsyncIterable<SharedStorage>::IterationSource::Kind kind,
      ExceptionState& exception_state) override;

  HeapMojoAssociatedRemote<mojom::blink::SharedStorageDocumentService>
      shared_storage_document_service_{nullptr};

  Member<SharedStorageWorklet> shared_storage_worklet_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_H_
