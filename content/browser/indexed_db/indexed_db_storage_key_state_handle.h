// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_STORAGE_KEY_STATE_HANDLE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_STORAGE_KEY_STATE_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {
class IndexedDBStorageKeyState;

// This handle tells the IndexedDBStorageKeyState that there is still something
// using the storage key, and the IndexedDBStorageKeyState won't close until all
// handles are destroyed. Destroying this handle can cause the storage key state
// to synchronously destruct, which modifies the `factories_per_storage_key_`
// map in IndexedDBFactoryImpl.
class CONTENT_EXPORT IndexedDBStorageKeyStateHandle {
 public:
  IndexedDBStorageKeyStateHandle();
  explicit IndexedDBStorageKeyStateHandle(
      base::WeakPtr<IndexedDBStorageKeyState> storage_key_state);
  IndexedDBStorageKeyStateHandle(IndexedDBStorageKeyStateHandle&&);
  IndexedDBStorageKeyStateHandle& operator=(IndexedDBStorageKeyStateHandle&&);

  IndexedDBStorageKeyStateHandle(const IndexedDBStorageKeyStateHandle&) =
      delete;
  IndexedDBStorageKeyStateHandle& operator=(
      const IndexedDBStorageKeyStateHandle&) = delete;

  ~IndexedDBStorageKeyStateHandle();

  bool IsHeld() const;

  void Release();

  // Returns null if the factory was destroyed, which should only happen on
  // context destruction.
  IndexedDBStorageKeyState* storage_key_state() {
    return storage_key_state_.get();
  }

 private:
  base::WeakPtr<IndexedDBStorageKeyState> storage_key_state_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_STORAGE_KEY_STATE_HANDLE_H_
