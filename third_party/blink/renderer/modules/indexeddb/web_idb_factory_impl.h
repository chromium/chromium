// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_FACTORY_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_FACTORY_IMPL_H_

#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_callbacks_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_database_callbacks_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_factory.h"

namespace blink {
class WebSecurityOrigin;
class WebString;

class WebIDBFactoryImpl : public blink::WebIDBFactory {
 public:
  explicit WebIDBFactoryImpl(mojom::blink::IDBFactoryPtrInfo factory_info);
  ~WebIDBFactoryImpl() override;

  // See WebIDBFactory.h for documentation on these functions.
  void GetDatabaseInfo(
      blink::WebIDBCallbacks* callbacks,
      const blink::WebSecurityOrigin& origin,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void GetDatabaseNames(
      blink::WebIDBCallbacks* callbacks,
      const blink::WebSecurityOrigin& origin,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void Open(const blink::WebString& name,
            long long version,
            long long transaction_id,
            blink::WebIDBCallbacks* callbacks,
            blink::WebIDBDatabaseCallbacks* databaseCallbacks,
            const blink::WebSecurityOrigin& origin,
            scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void DeleteDatabase(
      const blink::WebString& name,
      blink::WebIDBCallbacks* callbacks,
      const blink::WebSecurityOrigin& origin,
      bool force_close,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;

 private:
  mojom::blink::IDBCallbacksAssociatedPtrInfo GetCallbacksProxy(
      std::unique_ptr<blink::IndexedDBCallbacksImpl> callbacks);
  mojom::blink::IDBDatabaseCallbacksAssociatedPtrInfo GetDatabaseCallbacksProxy(
      std::unique_ptr<blink::IndexedDBDatabaseCallbacksImpl> callbacks);

  mojom::blink::IDBFactoryPtr factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_FACTORY_IMPL_H_
