// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_download_manager.h"

#include "components/download/public/common/download_create_info.h"
#include "content/browser/byte_stream.h"

namespace content {

MockDownloadManager::CreateDownloadItemAdapter::CreateDownloadItemAdapter(
    const std::string& guid,
    uint32_t id,
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    const base::Optional<url::Origin>& request_initiator,
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
      site_url(site_url),
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
      site_url(rhs.site_url),
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
  return (
      guid == rhs.guid && id == rhs.id && current_path == rhs.current_path &&
      target_path == rhs.target_path && url_chain == rhs.url_chain &&
      referrer_url == rhs.referrer_url && site_url == rhs.site_url &&
      tab_url == rhs.tab_url && tab_referrer_url == rhs.tab_referrer_url &&
      request_initiator == rhs.request_initiator &&
      mime_type == rhs.mime_type &&
      original_mime_type == rhs.original_mime_type &&
      start_time == rhs.start_time && end_time == rhs.end_time &&
      etag == rhs.etag && last_modified == rhs.last_modified &&
      received_bytes == rhs.received_bytes && total_bytes == rhs.total_bytes &&
      state == rhs.state && danger_type == rhs.danger_type &&
      interrupt_reason == rhs.interrupt_reason && opened == rhs.opened &&
      last_access_time == rhs.last_access_time && transient == rhs.transient &&
      received_slices == rhs.received_slices);
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
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    const base::Optional<url::Origin>& request_initiator,
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
      guid, id, current_path, target_path, url_chain, referrer_url, site_url,
      tab_url, tab_referrer_url, request_initiator, mime_type,
      original_mime_type, start_time, end_time, etag, last_modified,
      received_bytes, total_bytes, hash, state, danger_type, interrupt_reason,
      opened, last_access_time, transient, received_slices);
  return MockCreateDownloadItem(adapter);
}

void MockDownloadManager::OnHistoryQueryComplete(
    base::OnceClosure load_history_downloads_cb) {
  std::move(load_history_downloads_cb).Run();
}

}  // namespace content
