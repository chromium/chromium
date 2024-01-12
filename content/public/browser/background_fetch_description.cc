// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/background_fetch_description.h"

namespace content {

BackgroundFetchDescription::BackgroundFetchDescription(
    const std::string& job_unique_id,
    const url::Origin& origin,
    const std::string& title,
    const SkBitmap& icon,
    int completed_requests,
    int total_requests,
    uint64_t downloaded_bytes,
    uint64_t uploaded_bytes,
    uint64_t download_total_bytes,
    uint64_t upload_total_bytes,
    std::vector<std::string> outstanding_guids,
    bool start_paused,
    std::optional<net::IsolationInfo> isolation_info)
    : job_unique_id(job_unique_id),
      origin(origin),
      title(title),
      icon(icon),
      completed_requests(completed_requests),
      total_requests(total_requests),
      downloaded_bytes(downloaded_bytes),
      uploaded_bytes(uploaded_bytes),
      download_total_bytes(download_total_bytes),
      upload_total_bytes(upload_total_bytes),
      outstanding_guids(std::move(outstanding_guids)),
      start_paused(start_paused),
      isolation_info(std::move(isolation_info)) {}

BackgroundFetchDescription::~BackgroundFetchDescription() = default;

}  // namespace content
