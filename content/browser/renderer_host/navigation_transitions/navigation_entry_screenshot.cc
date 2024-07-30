// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "third_party/android_opengl/etc1/etc1.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#endif

namespace content {

#if BUILDFLAG(IS_ANDROID)
namespace {

BASE_FEATURE(kNavigationEntryScreenshotCompression,
             "NavigationEntryScreenshotCompression",
             base::FEATURE_DISABLED_BY_DEFAULT);

static bool g_disable_compression_for_testing = false;

using CompressionDoneCallback = base::OnceCallback<void(sk_sp<SkPixelRef>)>;
void CompressNavigationScreenshotOnWorkerThread(
    SkBitmap bitmap,
    CompressionDoneCallback done_callback) {
  TRACE_EVENT0("navigation", "CompressNavigationScreenshotOnWorkerThread");

  if (g_disable_compression_for_testing) {
    return;
  }

  gfx::Size bitmap_bounds(bitmap.width(), bitmap.height());
  constexpr size_t kPixelSize = 4;  // For kARGB_8888_Config.
  size_t stride = kPixelSize * bitmap_bounds.width();

  size_t encoded_bytes =
      etc1_get_encoded_data_size(bitmap_bounds.width(), bitmap_bounds.height());
  SkImageInfo info =
      SkImageInfo::Make(bitmap_bounds.width(), bitmap_bounds.height(),
                        kUnknown_SkColorType, kUnpremul_SkAlphaType);
  sk_sp<SkData> etc1_pixel_data(SkData::MakeUninitialized(encoded_bytes));
  const size_t row_bytes = bitmap_bounds.width() / 2;
  sk_sp<SkPixelRef> etc1_pixel_ref(SkMallocPixelRef::MakeWithData(
      info, row_bytes, std::move(etc1_pixel_data)));

  if (!etc1_encode_image(
          reinterpret_cast<unsigned char*>(bitmap.getPixels()),
          bitmap_bounds.width(), bitmap_bounds.height(), kPixelSize, stride,
          reinterpret_cast<unsigned char*>(etc1_pixel_ref->pixels()),
          bitmap_bounds.width(), bitmap_bounds.height())) {
    return;
  }

  std::move(done_callback).Run(std::move(etc1_pixel_ref));
}

}  // namespace
#endif

// static
const void* const NavigationEntryScreenshot::kUserDataKey =
    &NavigationEntryScreenshot::kUserDataKey;

// static
void NavigationEntryScreenshot::DisableCompressionForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(IS_ANDROID)
  g_disable_compression_for_testing = true;
#endif
}

NavigationEntryScreenshot::NavigationEntryScreenshot(const SkBitmap& bitmap,
                                                     int navigation_entry_id)
    : bitmap_(cc::UIResourceBitmap(bitmap)),
      navigation_entry_id_(navigation_entry_id) {
  CHECK(AreBackForwardTransitionsEnabled());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StartCompression(bitmap);
}

NavigationEntryScreenshot::~NavigationEntryScreenshot() {
  if (cache_) {
    cache_->OnNavigationEntryGone(navigation_entry_id_);
  }
}

cc::UIResourceBitmap NavigationEntryScreenshot::GetBitmap(cc::UIResourceId uid,
                                                          bool resource_lost) {
  // TODO(liuwilliam): Currently none of the impls of `GetBitmap` uses `uid` or
  // `resource_lost`. Consider deleting them from the interface.
  return GetBitmap();
}

size_t NavigationEntryScreenshot::SetCache(
    NavigationEntryScreenshotCache* cache) {
  CHECK(!cache_ || !cache);
  cache_ = cache;

  if (cache_ && compressed_bitmap_) {
    bitmap_.reset();
  }

  return GetBitmap().SizeInBytes();
}

gfx::Size NavigationEntryScreenshot::GetDimensions() const {
  return GetBitmap().GetSize();
}

SkBitmap NavigationEntryScreenshot::GetBitmapForTesting() const {
  return GetBitmap().GetBitmapForTesting();  // IN-TEST
}

size_t NavigationEntryScreenshot::CompressedSizeForTesting() const {
  return !bitmap_ ? compressed_bitmap_->SizeInBytes() : 0u;
}

void NavigationEntryScreenshot::StartCompression(const SkBitmap& bitmap) {
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(kNavigationEntryScreenshotCompression)) {
    return;
  }

  CompressionDoneCallback done_callback = base::BindPostTask(
      GetUIThreadTaskRunner(),
      base::BindOnce(&NavigationEntryScreenshot::OnCompressionFinished,
                     weak_factory_.GetWeakPtr()));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CompressNavigationScreenshotOnWorkerThread, bitmap,
                     std::move(done_callback)));
#endif
}

void NavigationEntryScreenshot::OnCompressionFinished(
    sk_sp<SkPixelRef> compressed_bitmap) {
  CHECK(!compressed_bitmap_);
  CHECK(bitmap_);
  CHECK(compressed_bitmap);

  const auto size = GetDimensions();
  compressed_bitmap_ = cc::UIResourceBitmap(std::move(compressed_bitmap), size);
  TRACE_EVENT2("navigation", "NavigationEntryScreenshot::OnCompressionFinished",
               "old_size", bitmap_->SizeInBytes(), "new_size",
               compressed_bitmap_->SizeInBytes());

  // We defer discarding the uncompressed bitmap if there is no cache since it
  // may still be in use in the UI.
  if (cache_) {
    bitmap_.reset();
    cache_->OnScreenshotCompressed(navigation_entry_id_,
                                   GetBitmap().SizeInBytes());
  }
}

const cc::UIResourceBitmap& NavigationEntryScreenshot::GetBitmap() const {
  return bitmap_ ? *bitmap_ : *compressed_bitmap_;
}

}  // namespace content
