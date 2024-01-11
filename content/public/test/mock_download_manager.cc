// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_download_manager.h"

#include "base/strings/string_split.h"
#include "components/download/public/common/download_create_info.h"
#include "content/browser/byte_stream.h"
#include "content/test/storage_partition_test_helpers.h"

namespace content {

namespace {
const char* kDelimiter = "|";
const char* kInMemorySetValue = "1";
const char* kFallbackModeNoneSetValue = "0";
const char* kFallbackModePartitionInMemorySetValue = "1";
const char* kFallbackModePartitionOnDiskSetValue = "2";
}  // namespace

MockDownloadManager::CreateDownloadItemAdapter::CreateDownloadItemAdapter(
    const std::string& guid,
    uint32_t id,
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const std::string& serialized_embedder_download_data,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    const std::optional<url::Origin>& request_initiator,
    const std::string& mime_type,
    const std::string& original_mime_type,
    base::Time start_time,
    base::Time end_time,
    const std::string& etag,
    const std::string& last_modified,
    int64_t received_bytes,
    int64_t total_bytes,
    const std::string& hash,
    download::DownloadItem::DownloadState state,
    download::DownloadDangerType danger_type,
    download::DownloadInterruptReason interrupt_reason,
    bool opened,
    base::Time last_access_time,
    bool transient,
    const std::vector<download::DownloadItem::ReceivedSlice>& received_slices)
    : guid(guid),
      id(id),
      current_path(current_path),
      target_path(target_path),
      url_chain(url_chain),
      referrer_url(referrer_url),
      serialized_embedder_download_data(serialized_embedder_download_data),
      tab_url(tab_url),
      tab_referrer_url(tab_referrer_url),
      request_initiator(request_initiator),
      mime_type(mime_type),
      original_mime_type(original_mime_type),
      start_time(start_time),
      end_time(end_time),
      received_bytes(received_bytes),
      total_bytes(total_bytes),
      hash(hash),
      state(state),
      danger_type(danger_type),
      interrupt_reason(interrupt_reason),
      opened(opened),
      last_access_time(last_access_time),
      transient(transient),
      received_slices(received_slices) {}

MockDownloadManager::CreateDownloadItemAdapter::CreateDownloadItemAdapter(
    const CreateDownloadItemAdapter& rhs)
    : guid(rhs.guid),
      id(rhs.id),
      current_path(rhs.current_path),
      target_path(rhs.target_path),
      url_chain(rhs.url_chain),
      referrer_url(rhs.referrer_url),
      serialized_embedder_download_data(rhs.serialized_embedder_download_data),
      tab_url(rhs.tab_url),
      tab_referrer_url(rhs.tab_referrer_url),
      request_initiator(rhs.request_initiator),
      start_time(rhs.start_time),
      end_time(rhs.end_time),
      etag(rhs.etag),
      last_modified(rhs.last_modified),
      received_bytes(rhs.received_bytes),
      total_bytes(rhs.total_bytes),
      state(rhs.state),
      danger_type(rhs.danger_type),
      interrupt_reason(rhs.interrupt_reason),
      opened(rhs.opened),
      last_access_time(rhs.last_access_time),
      transient(rhs.transient),
      received_slices(rhs.received_slices) {}

MockDownloadManager::CreateDownloadItemAdapter::~CreateDownloadItemAdapter() {}

bool MockDownloadManager::CreateDownloadItemAdapter::operator==(
    const CreateDownloadItemAdapter& rhs) const {
  return (guid == rhs.guid && id == rhs.id &&
          current_path == rhs.current_path && target_path == rhs.target_path &&
          url_chain == rhs.url_chain && referrer_url == rhs.referrer_url &&
          serialized_embedder_download_data ==
              rhs.serialized_embedder_download_data &&
          tab_url == rhs.tab_url && tab_referrer_url == rhs.tab_referrer_url &&
          request_initiator == rhs.request_initiator &&
          mime_type == rhs.mime_type &&
          original_mime_type == rhs.original_mime_type &&
          start_time == rhs.start_time && end_time == rhs.end_time &&
          etag == rhs.etag && last_modified == rhs.last_modified &&
          received_bytes == rhs.received_bytes &&
          total_bytes == rhs.total_bytes && state == rhs.state &&
          danger_type == rhs.danger_type &&
          interrupt_reason == rhs.interrupt_reason && opened == rhs.opened &&
          last_access_time == rhs.last_access_time &&
          transient == rhs.transient && received_slices == rhs.received_slices);
}

MockDownloadManager::MockDownloadManager() {}

MockDownloadManager::~MockDownloadManager() {}

download::DownloadItem* MockDownloadManager::CreateDownloadItem(
    const std::string& guid,
    uint32_t id,
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const StoragePartitionConfig& storage_partition_config,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    const std::optional<url::Origin>& request_initiator,
    const std::string& mime_type,
    const std::string& original_mime_type,
    base::Time start_time,
    base::Time end_time,
    const std::string& etag,
    const std::string& last_modified,
    int64_t received_bytes,
    int64_t total_bytes,
    const std::string& hash,
    download::DownloadItem::DownloadState state,
    download::DownloadDangerType danger_type,
    download::DownloadInterruptReason interrupt_reason,
    bool opened,
    base::Time last_access_time,
    bool transient,
    const std::vector<download::DownloadItem::ReceivedSlice>& received_slices) {
  CreateDownloadItemAdapter adapter(
      guid, id, current_path, target_path, url_chain, referrer_url,
      StoragePartitionConfigToSerializedEmbedderDownloadData(
          storage_partition_config),
      tab_url, tab_referrer_url, request_initiator, mime_type,
      original_mime_type, start_time, end_time, etag, last_modified,
      received_bytes, total_bytes, hash, state, danger_type, interrupt_reason,
      opened, last_access_time, transient, received_slices);
  return MockCreateDownloadItem(adapter);
}

std::string
MockDownloadManager::StoragePartitionConfigToSerializedEmbedderDownloadData(
    const StoragePartitionConfig& storage_partition_config) {
  std::string fallback_mode_str;
  switch (
      storage_partition_config.fallback_to_partition_domain_for_blob_urls()) {
    case StoragePartitionConfig::FallbackMode::kNone:
      fallback_mode_str = kFallbackModeNoneSetValue;
      break;
    case StoragePartitionConfig::FallbackMode::kFallbackPartitionInMemory:
      fallback_mode_str = kFallbackModePartitionInMemorySetValue;
      break;
    case StoragePartitionConfig::FallbackMode::kFallbackPartitionOnDisk:
      fallback_mode_str = kFallbackModePartitionOnDiskSetValue;
      break;
    default:
      fallback_mode_str = kFallbackModeNoneSetValue;
      break;
  }

  return storage_partition_config.partition_domain() + std::string(kDelimiter) +
         storage_partition_config.partition_name() + std::string(kDelimiter) +
         (storage_partition_config.in_memory() ? std::string(kInMemorySetValue)
                                               : "") +
         std::string(kDelimiter) + fallback_mode_str;
}

StoragePartitionConfig
MockDownloadManager::SerializedEmbedderDownloadDataToStoragePartitionConfig(
    const std::string& serialized_embedder_download_data) {
  std::vector<std::string> fields =
      base::SplitString(serialized_embedder_download_data, kDelimiter,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string partition_domain;
  std::string partition_name;
  bool in_memory = false;
  StoragePartitionConfig::FallbackMode fallback_mode =
      StoragePartitionConfig::FallbackMode::kNone;

  auto itr = fields.begin();
  if (itr != fields.end())
    partition_domain = *itr++;

  if (itr != fields.end())
    partition_name = *itr++;

  if (itr != fields.end())
    in_memory = (*itr++ == kInMemorySetValue);

  if (itr != fields.end()) {
    if (*itr == kFallbackModeNoneSetValue) {
      fallback_mode = StoragePartitionConfig::FallbackMode::kNone;
    } else if (*itr == kFallbackModePartitionInMemorySetValue) {
      fallback_mode =
          StoragePartitionConfig::FallbackMode::kFallbackPartitionInMemory;
    } else if (*itr == kFallbackModePartitionOnDiskSetValue) {
      fallback_mode =
          StoragePartitionConfig::FallbackMode::kFallbackPartitionOnDisk;
    }
  }

  auto config = CreateStoragePartitionConfigForTesting(
      in_memory, partition_domain, partition_name);
  config.set_fallback_to_partition_domain_for_blob_urls(fallback_mode);
  return config;
}

void MockDownloadManager::OnHistoryQueryComplete(
    base::OnceClosure load_history_downloads_cb) {
  std::move(load_history_downloads_cb).Run();
}

}  // namespace content
