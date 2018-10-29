// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_callbacks_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_database_callbacks_impl.h"

namespace blink {

WebIDBFactoryImpl::WebIDBFactoryImpl(
    mojom::blink::IDBFactoryPtrInfo factory_info)
    : factory_(std::move(factory_info)) {}

WebIDBFactoryImpl::~WebIDBFactoryImpl() = default;

void WebIDBFactoryImpl::GetDatabaseInfo(
    WebIDBCallbacks* callbacks,
    const WebSecurityOrigin& origin,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), IndexedDBCallbacksImpl::kNoTransaction,
      nullptr);
  factory_->GetDatabaseInfo(GetCallbacksProxy(std::move(callbacks_impl)),
                            origin);
}

void WebIDBFactoryImpl::GetDatabaseNames(
    WebIDBCallbacks* callbacks,
    const WebSecurityOrigin& origin,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), IndexedDBCallbacksImpl::kNoTransaction,
      nullptr);
  factory_->GetDatabaseNames(GetCallbacksProxy(std::move(callbacks_impl)),
                             origin);
}

void WebIDBFactoryImpl::Open(
    const WebString& name,
    long long version,
    long long transaction_id,
    WebIDBCallbacks* callbacks,
    WebIDBDatabaseCallbacks* database_callbacks,
    const WebSecurityOrigin& origin,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  auto database_callbacks_impl =
      std::make_unique<IndexedDBDatabaseCallbacksImpl>(
          base::WrapUnique(database_callbacks));
  DCHECK(!name.IsNull());
  factory_->Open(GetCallbacksProxy(std::move(callbacks_impl)),
                 GetDatabaseCallbacksProxy(std::move(database_callbacks_impl)),
                 origin, name, version, transaction_id);
}

void WebIDBFactoryImpl::DeleteDatabase(
    const WebString& name,
    WebIDBCallbacks* callbacks,
    const WebSecurityOrigin& origin,
    bool force_close,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), IndexedDBCallbacksImpl::kNoTransaction,
      nullptr);
  DCHECK(!name.IsNull());
  factory_->DeleteDatabase(GetCallbacksProxy(std::move(callbacks_impl)), origin,
                           name, force_close);
}

mojom::blink::IDBCallbacksAssociatedPtrInfo
WebIDBFactoryImpl::GetCallbacksProxy(
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  mojom::blink::IDBCallbacksAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request));
  return ptr_info;
}

mojom::blink::IDBDatabaseCallbacksAssociatedPtrInfo
WebIDBFactoryImpl::GetDatabaseCallbacksProxy(
    std::unique_ptr<IndexedDBDatabaseCallbacksImpl> callbacks) {
  mojom::blink::IDBDatabaseCallbacksAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request));
  return ptr_info;
}

}  // namespace blink
