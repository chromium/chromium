// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_manager.h"

#include "base/task/sequenced_task_runner.h"
#include "components/download/public/common/download_task_runner.h"

namespace content {

// static
scoped_refptr<base::SequencedTaskRunner> DownloadManager::GetTaskRunner() {
  return download::GetDownloadTaskRunner();
}

download::DownloadItem* DownloadManager::CreateDownloadItem(
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
  return CreateDownloadItem(
      guid, id, current_path, target_path, url_chain, referrer_url,
      storage_partition_config, tab_url, tab_referrer_url, request_initiator,
      mime_type, original_mime_type, start_time, end_time, etag, last_modified,
      received_bytes, total_bytes, hash, state, danger_type, interrupt_reason,
      opened, last_access_time, transient, received_slices, base::FilePath());
}

}  // namespace content
