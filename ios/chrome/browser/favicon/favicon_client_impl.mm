// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/favicon/favicon_client_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "components/grit/components_scaled_resources.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

int GetFaviconResourceIdForNativeURL(const GURL& url) {
  if (url.host_piece() == kChromeUICrashesHost)
    return IDR_CRASH_SAD_FAVICON;
  if (url.host_piece() == kChromeUIFlagsHost)
    return IDR_FLAGS_FAVICON;
  return -1;
}

void GetFaviconBitmapForNativeURL(
    const GURL& url,
    const std::vector<int>& desired_sizes_in_pixel,
    std::vector<favicon_base::FaviconRawBitmapResult>* favicon_bitmap_results) {
  const int resource_id = GetFaviconResourceIdForNativeURL(url);
  if (resource_id == -1)
    return;

  // Use ui::GetSupportedScaleFactors() because native URL favicon comes from
  // resources.
  std::vector<ui::ScaleFactor> scale_factors = ui::GetSupportedScaleFactors();

  std::vector<gfx::Size> candidate_sizes;
  for (ui::ScaleFactor scale_factor : scale_factors) {
    float scale = ui::GetScaleForScaleFactor(scale_factor);
    int candidate_size = static_cast<int>(gfx::kFaviconSize * scale + 0.5f);
    candidate_sizes.push_back(gfx::Size(candidate_size, candidate_size));
  }

  std::vector<size_t> selected_indices;
  SelectFaviconFrameIndices(candidate_sizes, desired_sizes_in_pixel,
                            &selected_indices, nullptr);

  for (size_t selected_index : selected_indices) {
    ui::ScaleFactor scale_factor = scale_factors[selected_index];
    favicon_base::FaviconRawBitmapResult favicon_bitmap;
    favicon_bitmap.icon_type = favicon_base::IconType::kFavicon;
    favicon_bitmap.pixel_size = candidate_sizes[selected_index];
    favicon_bitmap.bitmap_data =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
            resource_id, scale_factor);

    if (!favicon_bitmap.is_valid())
      continue;

    favicon_bitmap_results->push_back(favicon_bitmap);
  }
}

}  // namespace

FaviconClientImpl::FaviconClientImpl() {}

FaviconClientImpl::~FaviconClientImpl() {}

bool FaviconClientImpl::IsNativeApplicationURL(const GURL& url) {
  return url.SchemeIs(kChromeUIScheme);
}

base::CancelableTaskTracker::TaskId
FaviconClientImpl::GetFaviconForNativeApplicationURL(
    const GURL& url,
    const std::vector<int>& desired_sizes_in_pixel,
    favicon_base::FaviconResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(tracker);
  DCHECK(IsNativeApplicationURL(url));

  std::vector<favicon_base::FaviconRawBitmapResult> favicon_bitmap_results;
  GetFaviconBitmapForNativeURL(url, desired_sizes_in_pixel,
                               &favicon_bitmap_results);

  return tracker->PostTask(
      base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
      base::BindOnce(std::move(callback), std::move(favicon_bitmap_results)));
}
