// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_GLOBAL_INDEXED_DB_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_GLOBAL_INDEXED_DB_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;
class IDBFactory;

class GlobalIndexedDB final : public GarbageCollected<GlobalIndexedDB>,
                              public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];
  static GlobalIndexedDB& From(ExecutionContext&);
  static IDBFactory* indexedDB(ExecutionContext&);

  explicit GlobalIndexedDB(ExecutionContext&);

  void Trace(Visitor* visitor) const override;

 private:
  IDBFactory* IdbFactory(ExecutionContext&);
  Member<IDBFactory> idb_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_GLOBAL_INDEXED_DB_H_
