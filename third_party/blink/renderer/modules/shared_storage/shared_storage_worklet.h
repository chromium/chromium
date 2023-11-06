// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_WORKLET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_WORKLET_H_

#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"

namespace blink {

class SharedStorage;
class SharedStorageUrlWithMetadata;
class SharedStorageRunOperationMethodOptions;

// Implement the worklet attribute under window.sharedStorage.
class MODULES_EXPORT SharedStorageWorklet final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit SharedStorageWorklet(SharedStorage*);
  ~SharedStorageWorklet() override = default;

  void Trace(Visitor*) const override;

  // shared_storage_worklet.idl
  // addModule() imports ES6 module scripts.
  ScriptPromise addModule(ScriptState*,
                          const String& module_url,
                          ExceptionState&);

  ScriptPromise SelectURL(ScriptState*,
                          const String& name,
                          HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
                          const SharedStorageRunOperationMethodOptions* options,
                          ExceptionState&);
  ScriptPromise Run(ScriptState*,
                    const String& name,
                    const SharedStorageRunOperationMethodOptions* options,
                    ExceptionState&);

 private:
  // Set when addModule() was called and passed early renderer checks.
  HeapMojoAssociatedRemote<mojom::blink::SharedStorageWorkletHost>
      worklet_host_{nullptr};

  Member<SharedStorage> shared_storage_;
  bool keep_alive_after_operation_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_WORKLET_H_
