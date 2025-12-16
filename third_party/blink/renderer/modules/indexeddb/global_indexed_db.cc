// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/global_indexed_db.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_factory.h"

namespace blink {

const char GlobalIndexedDB::kSupplementName[] = "GlobalIndexedDB";

GlobalIndexedDB::GlobalIndexedDB(ExecutionContext& context)
    : Supplement<ExecutionContext>(context) {}

GlobalIndexedDB& GlobalIndexedDB::From(ExecutionContext& context) {
  GlobalIndexedDB* supplement =
      Supplement<ExecutionContext>::From<GlobalIndexedDB>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<GlobalIndexedDB>(context);
    Supplement<ExecutionContext>::ProvideTo(context, supplement);
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
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
