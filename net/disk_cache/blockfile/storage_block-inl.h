// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BLOCKFILE_STORAGE_BLOCK_INL_H_
#define NET_DISK_CACHE_BLOCKFILE_STORAGE_BLOCK_INL_H_

#include <stddef.h>
#include <stdint.h>

#include <type_traits>

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "net/disk_cache/blockfile/storage_block.h"

namespace disk_cache {

template <typename T>
StorageBlock<T>::StorageBlock(MappedFile* file, Addr address)
    : file_(file), address_(address) {
  static_assert(
      std::is_trivially_copyable_v<T>);  // T is loaded as bytes from a file.
  DCHECK_NE(address.num_blocks(), 0);
  DCHECK(!address.is_initialized() || sizeof(T) == address.BlockSize())
      << address.value();
}

template<typename T> StorageBlock<T>::~StorageBlock() {
  if (modified_)
    Store();
  DeleteData();
}

template <typename T>
void StorageBlock<T>::CopyFrom(StorageBlock<T>* other) {
  // Note: this operation only makes sense to use when we're pointing to a
  // single-block entry; and its only used for a type (RankingsNode) where
  // that's normally the case; but we can't actually CHECK against that since
  // it may get corrupted.
  DCHECK(!modified_);
  DCHECK(!other->modified_);
  Discard();
  address_ = other->address_;
  file_ = other->file_;
  *Data() = *other->Data();
}

template <typename T>
base::span<uint8_t> StorageBlock<T>::as_span() const {
  return base::as_writable_bytes(data_);
}

template<typename T> int StorageBlock<T>::offset() const {
  return address_.start_block() * address_.BlockSize();
}

template<typename T> bool StorageBlock<T>::LazyInit(MappedFile* file,
                                                    Addr address) {
  if (file_ || address_.is_initialized()) {
    NOTREACHED();
  }
  file_ = file;
  address_.set_value(address.value());
  DCHECK(sizeof(T) == address.BlockSize());
  return true;
}

template <typename T>
void StorageBlock<T>::SetData(base::span<T> other) {
  DCHECK(!modified_);
  DeleteData();
  data_ = other;
}

template <typename T>
void StorageBlock<T>::Discard() {
  if (data_.empty()) {
    return;
  }
  if (owned_data_.empty()) {
    NOTREACHED();
  }
  DeleteData();
  modified_ = false;
}

template <typename T>
void StorageBlock<T>::StopSharingData() {
  if (data_.empty() || !owned_data_.empty()) {
    return;
  }
  DCHECK(!modified_);
  data_ = base::span<T>();
}

template<typename T> void StorageBlock<T>::set_modified() {
  DCHECK(!data_.empty());
  modified_ = true;
}

template<typename T> void StorageBlock<T>::clear_modified() {
  modified_ = false;
}

template<typename T> T* StorageBlock<T>::Data() {
  if (data_.empty()) {
    AllocateData();
  }
  return &data_[0];
}

template<typename T> bool StorageBlock<T>::HasData() const {
  return !data_.empty();
}

template<typename T> bool StorageBlock<T>::VerifyHash() const {
  uint32_t hash = CalculateHash();
  return (!data_[0].self_hash || data_[0].self_hash == hash);
}

template<typename T> bool StorageBlock<T>::own_data() const {
  return !owned_data_.empty();
}

template<typename T> const Addr StorageBlock<T>::address() const {
  return address_;
}

template<typename T> bool StorageBlock<T>::Load() {
  if (file_) {
    if (data_.empty()) {
      AllocateData();
    }

    if (file_->Load(this)) {
      modified_ = false;
      return true;
    }
  }
  LOG(WARNING) << "Failed data load.";
  return false;
}

template<typename T> bool StorageBlock<T>::Store() {
  if (file_ && !data_.empty()) {
    data_[0].self_hash = CalculateHash();
    if (file_->Store(this)) {
      modified_ = false;
      return true;
    }
  }
  LOG(ERROR) << "Failed data store.";
  return false;
}

template<typename T> bool StorageBlock<T>::Load(FileIOCallback* callback,
                                                bool* completed) {
  if (file_) {
    if (data_.empty()) {
      AllocateData();
    }

    if (file_->Load(this, callback, completed)) {
      modified_ = false;
      return true;
    }
  }
  LOG(WARNING) << "Failed data load.";
  return false;
}

template<typename T> bool StorageBlock<T>::Store(FileIOCallback* callback,
                                                 bool* completed) {
  if (file_ && !data_.empty()) {
    data_[0]->self_hash = CalculateHash();
    if (file_->Store(this, callback, completed)) {
      modified_ = false;
      return true;
    }
  }
  LOG(ERROR) << "Failed data store.";
  return false;
}

template<typename T> void StorageBlock<T>::AllocateData() {
  DCHECK(data_.empty());
  owned_data_ = base::HeapArray<T>::WithSize(address_.num_blocks());
  data_ = owned_data_.as_span();
}

template<typename T> void StorageBlock<T>::DeleteData() {
  data_ = base::span<T>();
  owned_data_ = base::HeapArray<T>();
}

template <typename T>
uint32_t StorageBlock<T>::CalculateHash() const {
  base::span<const uint8_t> bytes = base::byte_span_from_ref(data_[0]);
  return base::PersistentHash(bytes.first(offsetof(T, self_hash)));
}

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_STORAGE_BLOCK_INL_H_
