// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/background_fetch_description.h"

namespace content {

BackgroundFetchDescription::BackgroundFetchDescription(
    std::string job_unique_id,
    std::string title,
    url::Origin origin,
    SkBitmap icon,
    int completed_parts,
    int total_parts,
    int completed_parts_size,
    int total_parts_size,
    std::vector<std::string> outstanding_guids,
    bool start_paused)
    : job_unique_id(job_unique_id),
      title(title),
      origin(origin),
      icon(icon),
      completed_parts(completed_parts),
      total_parts(total_parts),
      completed_parts_size(completed_parts_size),
      total_parts_size(total_parts_size),
      outstanding_guids(std::move(outstanding_guids)),
      start_paused(start_paused) {}

BackgroundFetchDescription::~BackgroundFetchDescription() = default;

}  // namespace content
