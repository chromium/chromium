// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_USER_DATA_H_
#define CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_USER_DATA_H_

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/supports_user_data.h"
#include "content/public/browser/storage_partition.h"

namespace content {

// A base class for classes attached to, and scoped to, the lifetime of a
// content::StoragePartition.

// StoragePartitionUserData is created when a user of an API inherits this class
// and calls CreateForStoragePartition.
//
// StoragePartitionUserData is similar to DocumentUserData, but is attached to a
// storage partition instead of any document.
//
// Example usage of StoragePartitionUserData:
//
// --- in foo_storage_partition_helper.h ---
// class FooStoragePartitionHelper : public
// content::StoragePartitionUserData<FooStoragePartitionHelper> {
//  public:
//   ~FooStoragePartitionHelper() override;
//
//   // ... more public stuff here ...
//
//  private:
//   explicit FooStoragePartitionHelper(content::StoragePartition*
//   storage_partition);
//
//   friend StoragePartitionUserData;
//   STORAGE_PARTITION_USER_DATA_KEY_DECL();
//
//   // ... more private stuff here ...
// };
//
// --- in foo_storage_partition_helper.cc ---
// STORAGE_PARTITION_USER_DATA_KEY_IMPL(FooStoragePartitionHelper);
//
// FooStoragePartitionHelper::FooStoragePartitionHelper(
//   content::StoragePartition* storage_partition)
//     : StoragePartitionUserData(storage_partition) {}
//
// FooStoragePartitionHelper::~FooStoragePartitionHelper() {}
//
template <typename T>
class StoragePartitionUserData : public base::SupportsUserData::Data {
 public:
  template <typename... Args>
  static void CreateForStoragePartition(StoragePartition* storage_partition,
                                        Args&&... args) {
    DCHECK(storage_partition);
    if (!GetForStoragePartition(storage_partition)) {
      T* data = new T(storage_partition, std::forward<Args>(args)...);
      storage_partition->SetUserData(UserDataKey(), base::WrapUnique(data));
    }
  }

  static T* GetOrCreateForStoragePartition(
      StoragePartition* storage_partition) {
    DCHECK(storage_partition);
    if (auto* data = GetForStoragePartition(storage_partition)) {
      return data;
    }

    CreateForStoragePartition(storage_partition);
    return GetForStoragePartition(storage_partition);
  }

  static T* GetForStoragePartition(StoragePartition* storage_partition) {
    DCHECK(storage_partition);
    return static_cast<T*>(storage_partition->GetUserData(UserDataKey()));
  }

  static void DeleteForStoragePartition(StoragePartition* storage_partition) {
    DCHECK(GetForStoragePartition(storage_partition));
    storage_partition->RemoveUserData(UserDataKey());
  }

  StoragePartition& storage_partition() const { return *storage_partition_; }

  static const void* UserDataKey() { return &T::kUserDataKey; }

 protected:
  explicit StoragePartitionUserData(StoragePartition* storage_partition)
      : storage_partition_(storage_partition) {}

 private:
  // StoragePartition associated with subclass which inherits this
  // StoragePartitionUserData.
  const raw_ptr<StoragePartition> storage_partition_;
};

// Users won't be able to instantiate the template if they miss declaring the
// user data key.
// This macro declares a static variable inside the class that inherits from
// StoragePartitionUserData. The address of this static variable is used as
// the key to store/retrieve an instance of the class.
#define STORAGE_PARTITION_USER_DATA_KEY_DECL() static const int kUserDataKey = 0

// This macro instantiates the static variable declared by the previous macro.
// It must live in a .cc file to ensure that there is only one instantiation
// of the static variable.
#define STORAGE_PARTITION_USER_DATA_KEY_IMPL(Type) const int Type::kUserDataKey

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_USER_DATA_H_
