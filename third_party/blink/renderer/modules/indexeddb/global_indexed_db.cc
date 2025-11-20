// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/global_indexed_db.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_factory.h"

namespace blink {

GlobalIndexedDB& GlobalIndexedDB::From(ExecutionContext& context) {
  GlobalIndexedDB* supplement = context.GetGlobalIndexedDB();
  if (!supplement) {
    supplement = MakeGarbageCollected<GlobalIndexedDB>();
    context.SetGlobalIndexedDB(supplement);
  }
  return *supplement;
}

IDBFactory* GlobalIndexedDB::indexedDB(ExecutionContext& context) {
  return GlobalIndexedDB::From(context).IdbFactory(context);
}

IDBFactory* GlobalIndexedDB::IdbFactory(ExecutionContext& context) {
  if (!idb_factory_) {
    idb_factory_ = MakeGarbageCollected<IDBFactory>(&context);
  }
  return idb_factory_.Get();
}

void GlobalIndexedDB::Trace(Visitor* visitor) const {
  visitor->Trace(idb_factory_);
}

}  // namespace blink
