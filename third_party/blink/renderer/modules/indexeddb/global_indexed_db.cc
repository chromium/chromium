// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/global_indexed_db.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_factory.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

template <typename T>
class GlobalIndexedDBImpl final
    : public GarbageCollected<GlobalIndexedDBImpl<T>>,
      public GarbageCollectedMixin {
 public:
  static GlobalIndexedDBImpl& From(T& supplementable) {
    GlobalIndexedDBImpl* supplement = supplementable.GetGlobalIndexedDBImpl();
    if (!supplement) {
      supplement = MakeGarbageCollected<GlobalIndexedDBImpl>();
      supplementable.SetGlobalIndexedDBImpl(supplement);
    }
    return *supplement;
  }

  GlobalIndexedDBImpl() = default;

  IDBFactory* IdbFactory(ExecutionContext* context) {
    if (!idb_factory_)
      idb_factory_ = MakeGarbageCollected<IDBFactory>(context);
    return idb_factory_.Get();
  }

  void Trace(Visitor* visitor) const override { visitor->Trace(idb_factory_); }

 private:
  Member<IDBFactory> idb_factory_;
};

IDBFactory* GlobalIndexedDB::indexedDB(ExecutionContext& context) {
  return GlobalIndexedDBImpl<ExecutionContext>::From(context).IdbFactory(
      &context);
}

IDBFactory* GlobalIndexedDB::indexedDB(LocalDOMWindow& window) {
  return GlobalIndexedDBImpl<LocalDOMWindow>::From(window).IdbFactory(&window);
}

IDBFactory* GlobalIndexedDB::indexedDB(WorkerGlobalScope& worker) {
  return GlobalIndexedDBImpl<WorkerGlobalScope>::From(worker).IdbFactory(
      &worker);
}

}  // namespace blink
