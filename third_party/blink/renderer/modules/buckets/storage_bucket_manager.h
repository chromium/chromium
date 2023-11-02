// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_STORAGE_BUCKET_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_STORAGE_BUCKET_MANAGER_H_

#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExceptionState;
class NavigatorBase;
class StorageBucketOptions;

class MODULES_EXPORT StorageBucketManager final
    : public ScriptWrappable,
      public Supplement<NavigatorBase>,
      public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Web-exposed as navigator.storageBuckets
  static StorageBucketManager* storageBuckets(ScriptState* script_state,
                                              NavigatorBase& navigator,
                                              ExceptionState& exception_state);

  explicit StorageBucketManager(NavigatorBase& navigator);
  ~StorageBucketManager() override = default;

  ScriptPromise open(ScriptState* script_state,
                     const String& name,
                     const StorageBucketOptions* options,
                     ExceptionState& exception_state);
  ScriptPromise keys(ScriptState* script_state,
                     ExceptionState& exception_state);
  ScriptPromise Delete(ScriptState* script_state,
                       const String& name,
                       ExceptionState& exception_state);

  // GarbageCollected
  void Trace(Visitor*) const override;

 private:
  mojom::blink::BucketManagerHost* GetBucketManager(ScriptState* script_state);

  void DidOpen(ScriptPromiseResolver* resolver,
               mojo::PendingRemote<mojom::blink::BucketHost> bucket_remote);
  void DidGetKeys(ScriptPromiseResolver* resolver,
                  const Vector<String>& keys,
                  bool success);
  void DidDelete(ScriptPromiseResolver* resolver, bool success);

  HeapMojoRemote<mojom::blink::BucketManagerHost> manager_remote_;

  Member<NavigatorBase> navigator_base_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_STORAGE_BUCKET_MANAGER_H_
