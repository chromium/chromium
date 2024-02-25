// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/model/favicon_client_impl.h"

#import <memory>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/task/cancelable_task_tracker.h"
#import "base/task/single_thread_task_runner.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon_base/favicon_types.h"
#import "components/favicon_base/select_favicon_frames.h"
#import "components/grit/components_scaled_resources.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/resource/resource_scale_factor.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

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

  // Use ui::GetSupportedResourceScaleFactors() because native URL favicon comes
  // from resources.
  const std::vector<ui::ResourceScaleFactor>& scale_factors =
      ui::GetSupportedResourceScaleFactors();

  std::vector<gfx::Size> candidate_sizes;
  for (const auto scale_factor : scale_factors) {
    float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    int candidate_size = static_cast<int>(gfx::kFaviconSize * scale + 0.5f);
    candidate_sizes.push_back(gfx::Size(candidate_size, candidate_size));
  }

  std::vector<size_t> selected_indices;
  SelectFaviconFrameIndices(candidate_sizes, desired_sizes_in_pixel,
                            &selected_indices, nullptr);

  for (size_t selected_index : selected_indices) {
    ui::ResourceScaleFactor scale_factor = scale_factors[selected_index];
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

bool FaviconClientImpl::IsReaderModeURL(const GURL& url) {
  // iOS does not yet support Reader Mode.
  return false;
}

const GURL FaviconClientImpl::GetOriginalUrlFromReaderModeUrl(const GURL& url) {
  return url;
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
      base::SingleThreadTaskRunner::GetCurrentDefault().get(), FROM_HERE,
      base::BindOnce(std::move(callback), std::move(favicon_bitmap_results)));
}
