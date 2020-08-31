// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.+

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_OBSERVATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_OBSERVATION_H_

#include <memory>

#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class IDBAny;
class IDBKeyRange;
class IDBValue;
class ScriptState;

class IDBObservation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static mojom::IDBOperationType StringToOperationType(const String&);

  IDBObservation(int64_t object_store_id,
                 mojom::IDBOperationType type,
                 IDBKeyRange* key_range,
                 std::unique_ptr<IDBValue> value);
  ~IDBObservation() override;

  void SetIsolate(v8::Isolate* isolate);
  void Trace(Visitor*) const override;

  // Implement the IDL
  ScriptValue key(ScriptState*);
  ScriptValue value(ScriptState*);
  const String& type() const;

  // Helpers.
  int64_t object_store_id() const { return object_store_id_; }

 private:
  int64_t object_store_id_;
  const mojom::IDBOperationType operation_type_;
  Member<IDBKeyRange> key_range_;
  Member<IDBAny> value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_OBSERVATION_H_
