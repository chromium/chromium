// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_storage_key_state_handle.h"

#include "content/browser/indexed_db/indexed_db_storage_key_state.h"

namespace content {

IndexedDBStorageKeyStateHandle::IndexedDBStorageKeyStateHandle() = default;
IndexedDBStorageKeyStateHandle::IndexedDBStorageKeyStateHandle(
    base::WeakPtr<IndexedDBStorageKeyState> storage_key_state)
    : storage_key_state_(storage_key_state) {}
IndexedDBStorageKeyStateHandle::IndexedDBStorageKeyStateHandle(
    IndexedDBStorageKeyStateHandle&&) = default;
IndexedDBStorageKeyStateHandle& IndexedDBStorageKeyStateHandle::operator=(
    IndexedDBStorageKeyStateHandle&&) = default;

IndexedDBStorageKeyStateHandle::~IndexedDBStorageKeyStateHandle() {
  if (storage_key_state_)
    storage_key_state_->OnHandleDestruction();
}

void IndexedDBStorageKeyStateHandle::Release() {
  if (storage_key_state_) {
    storage_key_state_->OnHandleDestruction();
    storage_key_state_.reset();
  }
}

bool IndexedDBStorageKeyStateHandle::IsHeld() const {
  return !!storage_key_state_;
}

}  // namespace content
