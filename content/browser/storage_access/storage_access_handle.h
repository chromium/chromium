// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_ACCESS_STORAGE_ACCESS_HANDLE_H_
#define CONTENT_BROWSER_STORAGE_ACCESS_STORAGE_ACCESS_HANDLE_H_

#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/storage_access/storage_access_handle.mojom.h"

namespace content {

class StorageAccessHandle
    : public DocumentService<blink::mojom::StorageAccessHandle> {
 public:
  static void Create(
      RenderFrameHost* host,
      mojo::PendingReceiver<blink::mojom::StorageAccessHandle> receiver);

  StorageAccessHandle(const StorageAccessHandle&) = delete;
  StorageAccessHandle& operator=(const StorageAccessHandle&) = delete;

  void BindIndexedDB(
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void BindLocks(
      mojo::PendingReceiver<blink::mojom::LockManager> receiver) override;

 private:
  StorageAccessHandle(
      RenderFrameHost& host,
      mojo::PendingReceiver<blink::mojom::StorageAccessHandle> receiver);
  ~StorageAccessHandle() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_ACCESS_STORAGE_ACCESS_HANDLE_H_
