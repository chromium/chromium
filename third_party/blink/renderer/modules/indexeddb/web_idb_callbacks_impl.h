/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CALLBACKS_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CALLBACKS_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class IDBKey;
class IDBRequest;
class IDBValue;
class WebIDBCursorImpl;
struct IDBDatabaseMetadata;

class WebIDBCallbacksImpl final : public WebIDBCallbacks {
  USING_FAST_MALLOC(WebIDBCallbacksImpl);

 public:
  // |kNoTransaction| is used as the default transaction ID when instantiating
  // an WebIDBCallbacksImpl instance.  See web_idb_factory_impl.cc for those
  // cases.
  enum : int64_t { kNoTransaction = -1 };

  explicit WebIDBCallbacksImpl(IDBRequest*);
  ~WebIDBCallbacksImpl() override;

  void SetState(base::WeakPtr<WebIDBCursorImpl> cursor,
                int64_t transaction_id) override;

  // Pointers transfer ownership.
  void Error(mojom::blink::IDBException code, const String& message) override;
  void SuccessNamesAndVersionsList(
      Vector<mojom::blink::IDBNameAndVersionPtr>) override;
  void SuccessStringList(const Vector<String>&) override;
  void SuccessCursor(
      mojo::PendingAssociatedRemote<mojom::blink::IDBCursor> cursor_info,
      std::unique_ptr<IDBKey> key,
      std::unique_ptr<IDBKey> primary_key,
      base::Optional<std::unique_ptr<IDBValue>> optional_value) override;
  void SuccessCursorPrefetch(Vector<std::unique_ptr<IDBKey>> keys,
                             Vector<std::unique_ptr<IDBKey>> primary_keys,
                             Vector<std::unique_ptr<IDBValue>> values) override;
  void SuccessDatabase(
      mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
      const IDBDatabaseMetadata& metadata) override;
  void SuccessKey(std::unique_ptr<IDBKey>) override;
  void SuccessValue(mojom::blink::IDBReturnValuePtr) override;
  void SuccessArray(Vector<mojom::blink::IDBReturnValuePtr>) override;
  void SuccessInteger(int64_t) override;
  void Success() override;
  void SuccessCursorContinue(
      std::unique_ptr<IDBKey>,
      std::unique_ptr<IDBKey> primary_key,
      base::Optional<std::unique_ptr<IDBValue>>) override;
  void Blocked(int64_t old_version) override;
  void UpgradeNeeded(
      mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
      int64_t old_version,
      mojom::IDBDataLoss data_loss,
      const String& data_loss_message,
      const IDBDatabaseMetadata&) override;
  void DetachRequestFromCallback() override;

 private:
  void Detach();
  void DetachCallbackFromRequest();

  Persistent<IDBRequest> request_;
  base::WeakPtr<WebIDBCursorImpl> cursor_;
  int64_t transaction_id_;
  probe::AsyncTaskId async_task_id_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CALLBACKS_IMPL_H_
