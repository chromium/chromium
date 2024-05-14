// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BLOCKFILE_STORAGE_BLOCK_INL_H_
#define NET_DISK_CACHE_BLOCKFILE_STORAGE_BLOCK_INL_H_

#include "net/disk_cache/blockfile/storage_block.h"

#include <stddef.h>
#include <stdint.h>

#include <type_traits>

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/notreached.h"

namespace disk_cache {

template <typename T>
StorageBlock<T>::StorageBlock(MappedFile* file, Addr address)
    : file_(file), address_(address) {
  static_assert(std::is_trivial_v<T>);  // T is loaded as bytes from a file.
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
  DCHECK(!modified_);
  DCHECK(!other->modified_);
  Discard();
  address_ = other->address_;
  file_ = other->file_;
  *Data() = *other->Data();
}

template<typename T> void* StorageBlock<T>::buffer() const {
  return data_;
}

template<typename T> size_t StorageBlock<T>::size() const {
  return address_.num_blocks() * sizeof(T);
}

template<typename T> int StorageBlock<T>::offset() const {
  return address_.start_block() * address_.BlockSize();
}

template<typename T> bool StorageBlock<T>::LazyInit(MappedFile* file,
                                                    Addr address) {
  if (file_ || address_.is_initialized()) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }
  file_ = file;
  address_.set_value(address.value());
  DCHECK(sizeof(T) == address.BlockSize());
  return true;
}

template<typename T> void StorageBlock<T>::SetData(T* other) {
  DCHECK(!modified_);
  DeleteData();
  data_ = other;
}

template<typename T> void  StorageBlock<T>::Discard() {
  if (!data_)
    return;
  if (!own_data_) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  DeleteData();
  data_ = nullptr;
  modified_ = false;
}

template<typename T> void  StorageBlock<T>::StopSharingData() {
  if (!data_ || own_data_)
    return;
  DCHECK(!modified_);
  data_ = nullptr;
}

template<typename T> void StorageBlock<T>::set_modified() {
  DCHECK(data_);
  modified_ = true;
}

template<typename T> void StorageBlock<T>::clear_modified() {
  modified_ = false;
}

template<typename T> T* StorageBlock<T>::Data() {
  if (!data_)
    AllocateData();
  return data_;
}

template<typename T> bool StorageBlock<T>::HasData() const {
  return (nullptr != data_);
}

template<typename T> bool StorageBlock<T>::VerifyHash() const {
  uint32_t hash = CalculateHash();
  return (!data_->self_hash || data_->self_hash == hash);
}

template<typename T> bool StorageBlock<T>::own_data() const {
  return own_data_;
}

template<typename T> const Addr StorageBlock<T>::address() const {
  return address_;
}

template<typename T> bool StorageBlock<T>::Load() {
  if (file_) {
    if (!data_)
      AllocateData();

    if (file_->Load(this)) {
      modified_ = false;
      return true;
    }
  }
  LOG(WARNING) << "Failed data load.";
  return false;
}

template<typename T> bool StorageBlock<T>::Store() {
  if (file_ && data_) {
    data_->self_hash = CalculateHash();
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
    if (!data_)
      AllocateData();

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
  if (file_ && data_) {
    data_->self_hash = CalculateHash();
    if (file_->Store(this, callback, completed)) {
      modified_ = false;
      return true;
    }
  }
  LOG(ERROR) << "Failed data store.";
  return false;
}

template<typename T> void StorageBlock<T>::AllocateData() {
  DCHECK(!data_);
  data_ = new T[address_.num_blocks()];
  own_data_ = true;
}

template<typename T> void StorageBlock<T>::DeleteData() {
  if (own_data_) {
    data_.ClearAndDeleteArray();
    own_data_ = false;
  }
}

template <typename T>
uint32_t StorageBlock<T>::CalculateHash() const {
  base::span<const uint8_t> bytes = base::as_bytes(base::span_from_ref(*data_));
  return base::PersistentHash(bytes.first(offsetof(T, self_hash)));
}

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_STORAGE_BLOCK_INL_H_
