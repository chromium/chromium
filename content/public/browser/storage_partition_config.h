// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_CONFIG_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "content/common/content_export.h"

namespace content {
class BrowserContext;

// Each StoragePartition is uniquely identified by which partition domain
// it belongs to (such as an app or the browser itself), the user supplied
// partition name and the bit indicating whether it should be persisted on
// disk or not. This structure contains those elements and is used as
// uniqueness key to lookup StoragePartition objects in the global map.
class CONTENT_EXPORT StoragePartitionConfig {
 public:
  StoragePartitionConfig(const StoragePartitionConfig&);
  StoragePartitionConfig& operator=(const StoragePartitionConfig&);

  // Creates a default config for |browser_context|. If |browser_context| is an
  // off-the-record profile, then the config will have |in_memory_| set to true.
  static StoragePartitionConfig CreateDefault(BrowserContext* browser_context);

  // Creates a config tied to a specific domain.
  // The |partition_domain| is [a-z]* UTF-8 string, specifying the domain in
  // which partitions live (similar to namespace). |partition_domain| must NOT
  // be an empty string. Within a domain, partitions can be uniquely identified
  // by the combination of |partition_name| and |in_memory| values. When a
  // partition is not to be persisted, the |in_memory| value must be set to
  // true. If |browser_context| is an off-the-record profile, then the config
  // will have |in_memory_| set to true independent of what is specified in
  // the |in_memory| parameter. This is because these profiles are not allowed
  // to persist information on disk.
  static StoragePartitionConfig Create(BrowserContext* browser_context,
                                       const std::string& partition_domain,
                                       const std::string& partition_name,
                                       bool in_memory);

  std::string partition_domain() const { return partition_domain_; }
  std::string partition_name() const { return partition_name_; }
  bool in_memory() const { return in_memory_; }

  // Returns true if this config was created by CreateDefault() or is
  // a copy of a config created with that method.
  bool is_default() const { return partition_domain_.empty(); }

  // In some cases we want a "child" storage partition to resolve blob URLs that
  // were created by their "parent", while not allowing the reverse. To enable
  // this, set this flag to a value other than kNone, which will result in the
  // storage partition with the same partition_domain but empty partition_name
  // being used as fallback for the purpose of resolving blob URLs.
  enum class FallbackMode {
    kNone,
    kFallbackPartitionOnDisk,
    kFallbackPartitionInMemory,
  };
  void set_fallback_to_partition_domain_for_blob_urls(FallbackMode fallback) {
    if (fallback != FallbackMode::kNone) {
      DCHECK(!is_default());
      DCHECK(!partition_domain_.empty());
      // TODO(acollwell): Ideally we shouldn't have storage partition configs
      // that differ only in their fallback mode, but unfortunately that isn't
      // true. When that is fixed this can be made more robust by disallowing
      // fallback from storage partitions with an empty partition name.
      // DCHECK(!partition_name_.empty());
    }
    fallback_to_partition_domain_for_blob_urls_ = fallback;
  }
  FallbackMode fallback_to_partition_domain_for_blob_urls() const {
    return fallback_to_partition_domain_for_blob_urls_;
  }
  base::Optional<StoragePartitionConfig> GetFallbackForBlobUrls() const;

  bool operator<(const StoragePartitionConfig& rhs) const;
  bool operator==(const StoragePartitionConfig& rhs) const;
  bool operator!=(const StoragePartitionConfig& rhs) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionConfigTest, OperatorLess);

  StoragePartitionConfig(const std::string& partition_domain,
                         const std::string& partition_name,
                         bool in_memory);

  std::string partition_domain_;
  std::string partition_name_;
  bool in_memory_ = false;
  FallbackMode fallback_to_partition_domain_for_blob_urls_ =
      FallbackMode::kNone;
};

CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                        const StoragePartitionConfig& config);

// Represents the storage partition ID that is used as the key for the
// SessionStorageNamespaceMap. This type is to help facilitate migrating the
// map key away from a string to a StoragePartitionConfig.
class CONTENT_EXPORT StoragePartitionId {
 public:
  explicit StoragePartitionId(BrowserContext* browser_context);
  StoragePartitionId(const std::string& partition_id,
                     const StoragePartitionConfig& config);
  StoragePartitionId(const StoragePartitionId&) = default;
  StoragePartitionId& operator=(const StoragePartitionId&) = default;

  const StoragePartitionConfig& config() const { return config_; }

  bool operator==(const StoragePartitionId& rhs) const {
    return id_ == rhs.id_;
  }
  bool operator!=(const StoragePartitionId& rhs) const {
    return id_ != rhs.id_;
  }
  bool operator<(const StoragePartitionId& rhs) const { return id_ < rhs.id_; }

  // String representation of this object for debug logging purposes.
  std::string ToString() const;

 private:
  std::string id_;

  // Config generated with the same information used to generate `id_`.
  // Currently this field is being used to determine if we can replace the
  // string representation with a StoragePartitionConfig. This field is
  // intentionally left out of the comparison operators because we want equality
  // and less-than to work the exact same way they did before this field was
  // added. This field is only intended to be used by code in
  // NavigationControllerImpl to establish whether it is safe to change the
  // StoragePartitionId representation.
  StoragePartitionConfig config_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STORAGE_PARTITION_CONFIG_H_
