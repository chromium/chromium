// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface.

#ifndef NET_DISK_CACHE_BLOCKFILE_STORAGE_BLOCK_H_
#define NET_DISK_CACHE_BLOCKFILE_STORAGE_BLOCK_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "net/disk_cache/blockfile/addr.h"
#include "net/disk_cache/blockfile/mapped_file.h"

namespace disk_cache {

// This class encapsulates common behavior of a single "block" of data that is
// stored on a block-file. It implements the FileBlock interface, so it can be
// serialized directly to the backing file.
// This object provides a memory buffer for the related data, and it can be used
// to actually share that memory with another instance of the class.
//
// The following example shows how to share storage with another object:
//    StorageBlock<TypeA> a(file, address);
//    StorageBlock<TypeB> b(file, address);
//    a.Load();
//    DoSomething(a.Data());
//    b.SetData(a.Data());
//    ModifySomething(b.Data());
//    // Data modified on the previous call will be saved by b's destructor.
//    b.set_modified();
template<typename T>
class StorageBlock : public FileBlock {
 public:
  StorageBlock(MappedFile* file, Addr address);
  virtual ~StorageBlock();

  // Deeps copies from another block. Neither this nor |other| should be
  // |modified|.
  void CopyFrom(StorageBlock<T>* other);

  // FileBlock interface.
  void* buffer() const override;
  size_t size() const override;
  int offset() const override;

  // Allows the overide of dummy values passed on the constructor.
  bool LazyInit(MappedFile* file, Addr address);

  // Sets the internal storage to share the memory provided by other instance.
  void SetData(T* other);

  // Deletes the data, even if it was modified and not saved. This object must
  // own the memory buffer (it cannot be shared).
  void Discard();

  // Stops sharing the data with another object.
  void StopSharingData();

  // Sets the object to lazily save the in-memory data on destruction.
  void set_modified();

  // Forgets that the data was modified, so it's not lazily saved.
  void clear_modified();

  // Gets a pointer to the internal storage (allocates storage if needed).
  T* Data();

  // Returns true if there is data associated with this object.
  bool HasData() const;

  // Returns true if the internal hash is correct.
  bool VerifyHash() const;

  // Returns true if this object owns the data buffer, false if it is shared.
  bool own_data() const;

  const Addr address() const;

  // Loads and store the data.
  bool Load();
  bool Store();
  bool Load(FileIOCallback* callback, bool* completed);
  bool Store(FileIOCallback* callback, bool* completed);

 private:
  void AllocateData();
  void DeleteData();
  uint32_t CalculateHash() const;

  T* data_;
  MappedFile* file_;
  Addr address_;
  bool modified_;
  bool own_data_;  // Is data_ owned by this object or shared with someone else.
  bool extended_;  // Used to store an entry of more than one block.

  DISALLOW_COPY_AND_ASSIGN(StorageBlock);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_STORAGE_BLOCK_H_
