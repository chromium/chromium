// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_DESCRIPTION_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_DESCRIPTION_H_

#include <vector>
#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/origin.h"

namespace content {

// Contains all information necessary to create
// a BackgroundFetch download (and in the future, upload) job.
struct CONTENT_EXPORT BackgroundFetchDescription {
  BackgroundFetchDescription(std::string job_unique_id,
                             std::string title,
                             url::Origin origin,
                             SkBitmap icon,
                             int completed_parts,
                             int total_parts,
                             int completed_parts_size,
                             int total_parts_size,
                             std::vector<std::string> outstanding_guids,
                             bool start_paused);
  ~BackgroundFetchDescription();

  const std::string job_unique_id;
  std::string title;
  const url::Origin origin;
  SkBitmap icon;
  int completed_parts;
  int total_parts;
  int completed_parts_size;
  int total_parts_size;
  std::vector<std::string> outstanding_guids;
  bool start_paused;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDescription);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_DESCRIPTION_H
