// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/global_indexed_db.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_factory.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

template <typename T>
class GlobalIndexedDBImpl final
    : public GarbageCollected<GlobalIndexedDBImpl<T>>,
      public Supplement<T> {
 public:
  static const char kSupplementName[];

  static GlobalIndexedDBImpl& From(T& supplementable) {
    GlobalIndexedDBImpl* supplement =
        Supplement<T>::template From<GlobalIndexedDBImpl>(supplementable);
    if (!supplement) {
      supplement = MakeGarbageCollected<GlobalIndexedDBImpl>(supplementable);
      Supplement<T>::ProvideTo(supplementable, supplement);
    }
    return *supplement;
  }

  explicit GlobalIndexedDBImpl(T& supplementable)
      : Supplement<T>(supplementable) {}

  IDBFactory* IdbFactory(ExecutionContext* context) {
    if (!idb_factory_)
      idb_factory_ = MakeGarbageCollected<IDBFactory>(context);
    return idb_factory_.Get();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(idb_factory_);
    Supplement<T>::Trace(visitor);
  }

 private:
  Member<IDBFactory> idb_factory_;
};

// static
template <typename T>
const char GlobalIndexedDBImpl<T>::kSupplementName[] = "GlobalIndexedDBImpl";

}  // namespace

IDBFactory* GlobalIndexedDB::indexedDB(LocalDOMWindow& window) {
  return GlobalIndexedDBImpl<LocalDOMWindow>::From(window).IdbFactory(&window);
}

IDBFactory* GlobalIndexedDB::indexedDB(WorkerGlobalScope& worker) {
  return GlobalIndexedDBImpl<WorkerGlobalScope>::From(worker).IdbFactory(
      &worker);
}

}  // namespace blink
