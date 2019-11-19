// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_database_callbacks_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

WebIDBFactoryImpl::WebIDBFactoryImpl(
    mojo::PendingRemote<mojom::blink::IDBFactory> pending_factory,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  factory_.Bind(std::move(pending_factory), task_runner_);
}

WebIDBFactoryImpl::~WebIDBFactoryImpl() = default;

void WebIDBFactoryImpl::GetDatabaseInfo(
    std::unique_ptr<WebIDBCallbacks> callbacks) {
  callbacks->SetState(nullptr, WebIDBCallbacksImpl::kNoTransaction);
  factory_->GetDatabaseInfo(GetCallbacksProxy(std::move(callbacks)));
}

void WebIDBFactoryImpl::GetDatabaseNames(
    std::unique_ptr<WebIDBCallbacks> callbacks) {
  callbacks->SetState(nullptr, WebIDBCallbacksImpl::kNoTransaction);
  factory_->GetDatabaseNames(GetCallbacksProxy(std::move(callbacks)));
}

void WebIDBFactoryImpl::Open(
    const String& name,
    int64_t version,
    mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
        transaction_receiver,
    int64_t transaction_id,
    std::unique_ptr<WebIDBCallbacks> callbacks,
    std::unique_ptr<WebIDBDatabaseCallbacks> database_callbacks) {
  callbacks->SetState(nullptr, WebIDBCallbacksImpl::kNoTransaction);
  auto database_callbacks_impl =
      std::make_unique<IndexedDBDatabaseCallbacksImpl>(
          std::move(database_callbacks));
  DCHECK(!name.IsNull());
  factory_->Open(GetCallbacksProxy(std::move(callbacks)),
                 GetDatabaseCallbacksProxy(std::move(database_callbacks_impl)),
                 name, version, std::move(transaction_receiver),
                 transaction_id);
}

void WebIDBFactoryImpl::DeleteDatabase(
    const String& name,
    std::unique_ptr<WebIDBCallbacks> callbacks,
    bool force_close) {
  callbacks->SetState(nullptr, WebIDBCallbacksImpl::kNoTransaction);
  DCHECK(!name.IsNull());
  factory_->DeleteDatabase(GetCallbacksProxy(std::move(callbacks)), name,
                           force_close);
}

mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks>
WebIDBFactoryImpl::GetCallbacksProxy(
    std::unique_ptr<WebIDBCallbacks> callbacks_impl) {
  mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks> pending_callbacks;
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::move(callbacks_impl),
      pending_callbacks.InitWithNewEndpointAndPassReceiver(), task_runner_);
  return pending_callbacks;
}

mojo::PendingAssociatedRemote<mojom::blink::IDBDatabaseCallbacks>
WebIDBFactoryImpl::GetDatabaseCallbacksProxy(
    std::unique_ptr<IndexedDBDatabaseCallbacksImpl> callbacks) {
  mojo::PendingAssociatedRemote<mojom::blink::IDBDatabaseCallbacks> remote;
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::move(callbacks), remote.InitWithNewEndpointAndPassReceiver(),
      task_runner_);
  return remote;
}

}  // namespace blink
