// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_FACTORY_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_FACTORY_IMPL_H_

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_database_callbacks_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_factory.h"

namespace WTF {
class String;
}

namespace blink {

class WebIDBFactoryImpl : public WebIDBFactory {
 public:
  explicit WebIDBFactoryImpl(
      mojo::PendingRemote<mojom::blink::IDBFactory> pending_factory,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebIDBFactoryImpl() override;

  // See web_idb_factory.h for documentation on these functions.
  void GetDatabaseInfo(std::unique_ptr<WebIDBCallbacks> callbacks) override;
  void GetDatabaseNames(std::unique_ptr<WebIDBCallbacks> callbacks) override;
  void Open(
      const WTF::String& name,
      int64_t version,
      mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
          transaction_receiver,
      int64_t transaction_id,
      std::unique_ptr<WebIDBCallbacks> callbacks,
      std::unique_ptr<WebIDBDatabaseCallbacks> database_callbacks) override;
  void DeleteDatabase(const WTF::String& name,
                      std::unique_ptr<WebIDBCallbacks> callbacks,
                      bool force_close) override;

 private:
  mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks> GetCallbacksProxy(
      std::unique_ptr<WebIDBCallbacks> callbacks);
  mojo::PendingAssociatedRemote<mojom::blink::IDBDatabaseCallbacks>
  GetDatabaseCallbacksProxy(
      std::unique_ptr<IndexedDBDatabaseCallbacksImpl> callbacks);

  mojo::Remote<mojom::blink::IDBFactory> factory_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_FACTORY_IMPL_H_
