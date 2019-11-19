// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_OBSERVER_CHANGES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_OBSERVER_CHANGES_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_observation.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_transaction.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ScriptState;

class IDBObserverChanges final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  IDBObserverChanges(IDBDatabase*,
                     IDBTransaction*,
                     const Vector<Persistent<IDBObservation>>& observations,
                     const Vector<int32_t>& observation_indices);

  void Trace(blink::Visitor*) override;

  // Implement IDL
  IDBTransaction* transaction() const { return transaction_.Get(); }
  IDBDatabase* database() const { return database_.Get(); }
  ScriptValue records(ScriptState*);

 private:
  void ExtractChanges(const Vector<Persistent<IDBObservation>>& observations,
                      const Vector<int32_t>& observation_indices);

  Member<IDBDatabase> database_;
  Member<IDBTransaction> transaction_;
  // Map object_store_id to IDBObservation list.
  HeapHashMap<int64_t, HeapVector<Member<IDBObservation>>> records_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_OBSERVER_CHANGES_H_
