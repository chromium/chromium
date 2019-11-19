// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_OBSERVER_H_

#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ExceptionState;
class IDBDatabase;
class IDBObserverInit;
class IDBTransaction;
class V8IDBObserverCallback;

class MODULES_EXPORT IDBObserver final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBObserver* Create(V8IDBObserverCallback*);

  V8IDBObserverCallback* Callback() { return callback_; }

  explicit IDBObserver(V8IDBObserverCallback*);

  // Implement the IDBObserver IDL.
  void observe(IDBDatabase*,
               IDBTransaction*,
               const IDBObserverInit*,
               ExceptionState&);
  void unobserve(IDBDatabase*, ExceptionState&);

  void Trace(blink::Visitor*) override;

 private:
  Member<V8IDBObserverCallback> callback_;
  HeapHashMap<int32_t, WeakMember<IDBDatabase>> observer_ids_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_OBSERVER_H_
