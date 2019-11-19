// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/global_indexed_db.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_factory.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

template <typename T>
class GlobalIndexedDBImpl final
    : public GarbageCollected<GlobalIndexedDBImpl<T>>,
      public Supplement<T> {
  USING_GARBAGE_COLLECTED_MIXIN(GlobalIndexedDBImpl);

 public:
  static const char kSupplementName[];

  static GlobalIndexedDBImpl& From(T& supplementable) {
    GlobalIndexedDBImpl* supplement =
        Supplement<T>::template From<GlobalIndexedDBImpl>(supplementable);
    if (!supplement) {
      supplement = MakeGarbageCollected<GlobalIndexedDBImpl>();
      Supplement<T>::ProvideTo(supplementable, supplement);
    }
    return *supplement;
  }

  GlobalIndexedDBImpl() = default;

  IDBFactory* IdbFactory(T& fetching_scope) {
    if (!idb_factory_)
      idb_factory_ = MakeGarbageCollected<IDBFactory>();
    return idb_factory_;
  }

  void Trace(blink::Visitor* visitor) override {
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
  return GlobalIndexedDBImpl<LocalDOMWindow>::From(window).IdbFactory(window);
}

IDBFactory* GlobalIndexedDB::indexedDB(WorkerGlobalScope& worker) {
  return GlobalIndexedDBImpl<WorkerGlobalScope>::From(worker).IdbFactory(
      worker);
}

}  // namespace blink
