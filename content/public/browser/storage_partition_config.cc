// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/storage_partition_config.h"

#include "base/check.h"

namespace content {

StoragePartitionConfig::StoragePartitionConfig(const StoragePartitionConfig&) =
    default;
StoragePartitionConfig& StoragePartitionConfig::operator=(
    const StoragePartitionConfig&) = default;

// static
StoragePartitionConfig StoragePartitionConfig::CreateDefault() {
  return StoragePartitionConfig("", "", false);
}

// static
StoragePartitionConfig StoragePartitionConfig::Create(
    const std::string& partition_domain,
    const std::string& partition_name,
    bool in_memory) {
  // If a caller tries to pass an empty partition_domain something is seriously
  // wrong or the calling code is not explicitly signalling its desire to create
  // a default partition by calling CreateDefault().
  CHECK(!partition_domain.empty());
  return StoragePartitionConfig(partition_domain, partition_name, in_memory);
}

StoragePartitionConfig::StoragePartitionConfig(
    const std::string& partition_domain,
    const std::string& partition_name,
    bool in_memory)
    : partition_domain_(partition_domain),
      partition_name_(partition_name),
      in_memory_(in_memory) {}

StoragePartitionConfig StoragePartitionConfig::CopyWithInMemorySet() const {
  if (in_memory_)
    return *this;

  auto result = StoragePartitionConfig(partition_domain_, partition_name_,
                                       true /* in_memory */);
  result.set_fallback_to_partition_domain_for_blob_urls(
      fallback_to_partition_domain_for_blob_urls_);
  return result;
}

base::Optional<StoragePartitionConfig>
StoragePartitionConfig::GetFallbackForBlobUrls() const {
  if (fallback_to_partition_domain_for_blob_urls_ == FallbackMode::kNone)
    return base::nullopt;

  return StoragePartitionConfig(
      partition_domain_, "",
      /*in_memory=*/fallback_to_partition_domain_for_blob_urls_ ==
          FallbackMode::kFallbackPartitionInMemory);
}

bool StoragePartitionConfig::operator<(
    const StoragePartitionConfig& rhs) const {
  if (partition_domain_ != rhs.partition_domain_)
    return partition_domain_ < rhs.partition_domain_;

  if (partition_name_ != rhs.partition_name_)
    return partition_name_ < rhs.partition_name_;

  if (in_memory_ != rhs.in_memory_)
    return in_memory_ < rhs.in_memory_;

  if (fallback_to_partition_domain_for_blob_urls_ !=
      rhs.fallback_to_partition_domain_for_blob_urls_) {
    return fallback_to_partition_domain_for_blob_urls_ <
           rhs.fallback_to_partition_domain_for_blob_urls_;
  }

  return false;
}

bool StoragePartitionConfig::operator==(
    const StoragePartitionConfig& rhs) const {
  return partition_domain_ == rhs.partition_domain_ &&
         partition_name_ == rhs.partition_name_ &&
         in_memory_ == rhs.in_memory_ &&
         fallback_to_partition_domain_for_blob_urls_ ==
             rhs.fallback_to_partition_domain_for_blob_urls_;
}

bool StoragePartitionConfig::operator!=(
    const StoragePartitionConfig& rhs) const {
  return !(*this == rhs);
}

}  // namespace content
