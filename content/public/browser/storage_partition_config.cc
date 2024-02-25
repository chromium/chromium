// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/storage_partition_config.h"

#include <sstream>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace content {

StoragePartitionConfig::StoragePartitionConfig() = default;
StoragePartitionConfig::StoragePartitionConfig(const StoragePartitionConfig&) =
    default;
StoragePartitionConfig& StoragePartitionConfig::operator=(
    const StoragePartitionConfig&) = default;

// static
StoragePartitionConfig StoragePartitionConfig::CreateDefault(
    BrowserContext* browser_context) {
  return StoragePartitionConfig("", "", browser_context->IsOffTheRecord());
}

// static
StoragePartitionConfig StoragePartitionConfig::Create(
    BrowserContext* browser_context,
    const std::string& partition_domain,
    const std::string& partition_name,
    bool in_memory) {
  // If a caller tries to pass an empty partition_domain something is seriously
  // wrong or the calling code is not explicitly signalling its desire to create
  // a default partition by calling CreateDefault().
  CHECK(!partition_domain.empty());
  return StoragePartitionConfig(partition_domain, partition_name,
                                in_memory || browser_context->IsOffTheRecord());
}

StoragePartitionConfig::StoragePartitionConfig(
    const std::string& partition_domain,
    const std::string& partition_name,
    bool in_memory)
    : partition_domain_(partition_domain),
      partition_name_(partition_name),
      in_memory_(in_memory) {}

std::optional<StoragePartitionConfig>
StoragePartitionConfig::GetFallbackForBlobUrls() const {
  if (fallback_to_partition_domain_for_blob_urls_ == FallbackMode::kNone)
    return std::nullopt;

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

std::ostream& operator<<(std::ostream& out,
                         const StoragePartitionConfig& config) {
  out << "{";
  if (config.is_default()) {
    out << "default";
  } else {
    out << "partition_domain='" << config.partition_domain() << "'";
    out << " partition_name='" << config.partition_name() << "'";

    if (config.in_memory())
      out << " in_memory";

    auto fallback_mode = config.fallback_to_partition_domain_for_blob_urls();
    if (fallback_mode != StoragePartitionConfig::FallbackMode::kNone) {
      out << " fallback_mode="
          << base::NumberToString(static_cast<int>(fallback_mode));
    }
  }
  out << "}";
  return out;
}

}  // namespace content
