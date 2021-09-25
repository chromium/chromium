// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_DESCRIPTION_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_DESCRIPTION_H_

#include <stdint.h>
#include <vector>

#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/origin.h"

namespace content {

// Contains all information necessary to create a BackgroundFetch download job.
struct CONTENT_EXPORT BackgroundFetchDescription {
  BackgroundFetchDescription(const std::string& job_unique_id,
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
                             bool start_paused);
  BackgroundFetchDescription(const BackgroundFetchDescription&) = delete;
  BackgroundFetchDescription& operator=(const BackgroundFetchDescription&) =
      delete;

  ~BackgroundFetchDescription();

  // Fetch identifiers.
  const std::string job_unique_id;
  const url::Origin origin;

  // UI params.
  std::string title;
  SkBitmap icon;

  // Progress trackers.
  int completed_requests;
  int total_requests;
  uint64_t downloaded_bytes;
  uint64_t uploaded_bytes;
  uint64_t download_total_bytes;
  uint64_t upload_total_bytes;

  // Initialization params.
  std::vector<std::string> outstanding_guids;
  bool start_paused;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_DESCRIPTION_H_
