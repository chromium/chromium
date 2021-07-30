// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_H_

#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom-blink.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class ScriptState;
class SharedStorageWorklet;
class SharedStorageSetMethodOptions;
class SharedStorageRunOperationMethodOptions;

class MODULES_EXPORT SharedStorage final : public ScriptWrappable {
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

  ScriptPromise runURLSelectionOperation(ScriptState*,
                                         const String& name,
                                         const Vector<String>& urls,
                                         ExceptionState&);
  ScriptPromise runURLSelectionOperation(
      ScriptState*,
      const String& name,
      const Vector<String>& urls,
      const SharedStorageRunOperationMethodOptions* options,
      ExceptionState&);

  ScriptPromise runOperation(ScriptState*, const String& name, ExceptionState&);
  ScriptPromise runOperation(
      ScriptState*,
      const String& name,
      const SharedStorageRunOperationMethodOptions* options,
      ExceptionState&);

  SharedStorageWorklet* worklet(ScriptState*, ExceptionState&);

 private:
  mojom::blink::SharedStorageDocumentService* GetSharedStorageDocumentService(
      ExecutionContext* execution_context);

  mojo::Remote<mojom::blink::SharedStorageDocumentService>
      shared_storage_document_service_;

  Member<SharedStorageWorklet> shared_storage_worklet_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_H_
