// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_IMPL_MAP_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_IMPL_MAP_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_set>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace content {

class BrowserContext;

// A std::string to StoragePartition map for use with SupportsUserData APIs.
class CONTENT_EXPORT StoragePartitionImplMap
  : public base::SupportsUserData::Data {
 public:
  explicit StoragePartitionImplMap(BrowserContext* browser_context);

  ~StoragePartitionImplMap() override;

  // This map retains ownership of the returned StoragePartition objects.
  StoragePartitionImpl* Get(const StoragePartitionConfig& partition_config,
                            bool can_create);

  // Starts an asynchronous best-effort attempt to delete all on-disk storage
  // related to |partition_domain|, avoiding any directories that are known to
  // be in use.
  //
  // |on_gc_required| is called if the AsyncObliterate() call was unable to
  // fully clean the on-disk storage requiring a call to GarbageCollect() on
  // the next browser start.
  void AsyncObliterate(const std::string& partition_domain,
                       base::OnceClosure on_gc_required);

  // Examines the on-disk storage and removes any entires that are not listed
  // in the |active_paths|, or in use by current entries in the storage
  // partition.
  //
  // The |done| closure is executed on the calling thread when garbage
  // collection is complete.
  void GarbageCollect(
      std::unique_ptr<std::unordered_set<base::FilePath>> active_paths,
      base::OnceClosure done);

  void ForEach(BrowserContext::StoragePartitionCallback callback);

  size_t size() const { return partitions_.size(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionConfigTest, OperatorLess);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplMapTest, GarbageCollect);

  typedef std::map<StoragePartitionConfig,
                   std::unique_ptr<StoragePartitionImpl>>
      PartitionMap;

  // Returns the relative path from the profile's base directory, to the
  // directory that holds all the state for storage contexts in the given
  // |partition_domain| and |partition_name|.
  static base::FilePath GetStoragePartitionPath(
      const std::string& partition_domain,
      const std::string& partition_name);

  // This must always be called *after* |partition| has been added to the
  // partitions_.
  //
  // TODO(ajwong): Is there a way to make it so that Get()'s implementation
  // doesn't need to be aware of this ordering?  Revisit when refactoring
  // ResourceContext and AppCache to respect storage partitions.
  void PostCreateInitialization(StoragePartitionImpl* partition,
                                bool in_memory);

  BrowserContext* browser_context_;  // Not Owned.
  scoped_refptr<base::SequencedTaskRunner> file_access_runner_;
  PartitionMap partitions_;

  // Set to true when the ResourceContext for the associated |browser_context_|
  // is initialized. Can never return to false.
  bool resource_context_initialized_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionImplMap);
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_PARTITION_IMPL_MAP_H_
