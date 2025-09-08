// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define TODO_BASE_FEATURE_MACROS_NEED_MIGRATION

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/skia_span_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/android/resources/etc1_utils.h"
#endif

namespace content {

#if BUILDFLAG(IS_ANDROID)
namespace {

BASE_FEATURE(NavigationEntryScreenshotCompression,
             base::FEATURE_ENABLED_BY_DEFAULT);

static bool g_disable_compression_for_testing = false;

using CompressionDoneCallback = base::OnceCallback<void(sk_sp<SkPixelRef>)>;
void CompressNavigationScreenshotOnWorkerThread(
    SkBitmap bitmap,
    bool supports_etc_non_power_of_two,
    CompressionDoneCallback done_callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Navigation.GestureTransition.CompressionTime");
  TRACE_EVENT0("navigation", "CompressNavigationScreenshotOnWorkerThread");

  sk_sp<SkPixelRef> compressed_bitmap = nullptr;
  if (base::FeatureList::IsEnabled(ui::kCompressBitmapAtBackgroundPriority)) {
    compressed_bitmap = ui::Etc1::CompressBitmapAtBackgroundPriority(
        bitmap, supports_etc_non_power_of_two);
  } else {
    compressed_bitmap =
        ui::Etc1::CompressBitmap(bitmap, supports_etc_non_power_of_two);
  }

  if (compressed_bitmap) {
    std::move(done_callback).Run(std::move(compressed_bitmap));
  }
}

}  // namespace
#endif

// static
const void* const NavigationEntryScreenshot::kUserDataKey =
    &NavigationEntryScreenshot::kUserDataKey;

// static
void NavigationEntryScreenshot::SetDisableCompressionForTesting(bool disable) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(IS_ANDROID)
  g_disable_compression_for_testing = disable;
#endif
}

NavigationEntryScreenshot::NavigationEntryScreenshot(
    const SkBitmap& bitmap,
    NavigationTransitionData::UniqueId unique_id,
    bool supports_etc_non_power_of_two)
    : performance_scenarios::MatchingScenarioObserver(
          performance_scenarios::kDefaultIdleScenarios),
      bitmap_(cc::UIResourceBitmap(bitmap)),
      unique_id_(unique_id),
      dimensions_without_compression_(bitmap_->GetSize()),
      supports_etc_non_power_of_two_(supports_etc_non_power_of_two) {
  CHECK(NavigationTransitionConfig::AreBackForwardTransitionsEnabled());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SetupCompressionTask(bitmap, supports_etc_non_power_of_two);
}

NavigationEntryScreenshot::NavigationEntryScreenshot(
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    NavigationTransitionData::UniqueId unique_id,
    bool supports_etc_non_power_of_two,
    scoped_refptr<viz::RasterContextProvider> context_provider,
    ScreenshotCallback screenshot_callback)
    : performance_scenarios::MatchingScenarioObserver(
          performance_scenarios::kDefaultIdleScenarios),
      shared_image_(std::move(shared_image)),
      unique_id_(unique_id),
      dimensions_without_compression_(shared_image_->size()),
      supports_etc_non_power_of_two_(supports_etc_non_power_of_two),
      context_provider_(std::move(context_provider)),
      screenshot_callback_(std::move(screenshot_callback)) {
  DCHECK(NavigationTransitionConfig::AreBackForwardTransitionsEnabled());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  context_provider_->AddObserver(this);

  auto observer_list =
      performance_scenarios::PerformanceScenarioObserverList::GetForScope(
          performance_scenarios::ScenarioScope::kGlobal);
  if (observer_list) {
    observer_list->AddMatchingObserver(this);
    read_back_needed_ = true;
    return;
  }
  ReadBack();
}

NavigationEntryScreenshot::~NavigationEntryScreenshot() {
  if (cache_) {
    cache_->OnNavigationEntryGone(unique_id_);
  }
  if (read_back_needed_ || compression_task_) {
    auto observer_list =
        performance_scenarios::PerformanceScenarioObserverList::GetForScope(
            performance_scenarios::ScenarioScope::kGlobal);
    if (observer_list) {
      observer_list->RemoveMatchingObserver(this);
    }
  }
  ResetContextProvider();
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

  if (shared_image_) {
    return SkColorTypeBytesPerPixel(kN32_SkColorType) *
           shared_image_->size().Area64();
  }

  return GetBitmap().SizeInBytes();
}

void NavigationEntryScreenshot::OnScenarioMatchChanged(
    performance_scenarios::ScenarioScope scope,
    bool matches_pattern) {
  if (!matches_pattern) {
    return;
  }

  if (read_back_needed_) {
    ReadBack();
    read_back_needed_ = false;
    performance_scenarios::PerformanceScenarioObserverList::GetForScope(
        performance_scenarios::ScenarioScope::kGlobal)
        ->RemoveMatchingObserver(this);
    return;
  }

  if (compression_task_) {
    StartCompression();
    performance_scenarios::PerformanceScenarioObserverList::GetForScope(
        performance_scenarios::ScenarioScope::kGlobal)
        ->RemoveMatchingObserver(this);
  }
}

void NavigationEntryScreenshot::OnContextLost() {
  ResetContextProvider();
}

SkBitmap NavigationEntryScreenshot::GetBitmapForTesting() const {
  return GetBitmap().GetBitmapForTesting();  // IN-TEST
}

size_t NavigationEntryScreenshot::CompressedSizeForTesting() const {
  return !bitmap_ ? compressed_bitmap_->SizeInBytes() : 0u;
}

void NavigationEntryScreenshot::SetupCompressionTask(
    const SkBitmap& bitmap,
    bool supports_etc_non_power_of_two) {
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(kNavigationEntryScreenshotCompression) ||
      g_disable_compression_for_testing) {
    return;
  }

  CompressionDoneCallback done_callback = base::BindPostTask(
      GetUIThreadTaskRunner(),
      base::BindOnce(&NavigationEntryScreenshot::OnCompressionFinished,
                     weak_factory_.GetWeakPtr()));

  compression_task_ =
      base::BindOnce(&CompressNavigationScreenshotOnWorkerThread, bitmap,
                     supports_etc_non_power_of_two, std::move(done_callback));

  if (NavigationTransitionConfig::ShouldCompressScreenshotWhenQuiet() &&
      !performance_scenarios::CurrentScenariosMatch(
          performance_scenarios::ScenarioScope::kGlobal, scenario_pattern())) {
    auto observer_list =
        performance_scenarios::PerformanceScenarioObserverList::GetForScope(
            performance_scenarios::ScenarioScope::kGlobal);
    if (observer_list) {
      observer_list->AddMatchingObserver(this);
      return;
    }
  }
  StartCompression();
#endif
}

void NavigationEntryScreenshot::StartCompression() {
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                             std::move(compression_task_));
}

void NavigationEntryScreenshot::ResetContextProvider() {
  if (context_provider_) {
    context_provider_->RemoveObserver(this);
    context_provider_.reset();
  }
}

void NavigationEntryScreenshot::ReadBack() {
  TRACE_EVENT("content", "NavigationEntryScreenshot::ReadBack");
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SkImageInfo info = SkImageInfo::MakeN32(shared_image_->size().width(),
                                          shared_image_->size().height(),
                                          shared_image_->alpha_type());
  read_back_bitmap_.emplace();
  if (!read_back_bitmap_->tryAllocPixels(info)) {
    OnReadBack(false);
    return;
  }
  if (!context_provider_) {
    OnReadBack(false);
    return;
  }

  gfx::Point src_point;
  auto* raster_interface = context_provider_->RasterInterface();
  DCHECK(raster_interface);
  auto scoped_access = shared_image_->BeginRasterAccess(
      raster_interface, shared_image_->creation_sync_token(),
      /*readonly=*/true);
  raster_interface->ReadbackARGBPixelsAsync(
      shared_image_->mailbox(), shared_image_->GetTextureTarget(),
      shared_image_->surface_origin(), shared_image_->size(), src_point, info,
      info.minRowBytes(),
      gfx::SkPixmapToWritableSpan(read_back_bitmap_->pixmap()),
      base::BindOnce(&NavigationEntryScreenshot::OnReadBack,
                     weak_factory_.GetWeakPtr()));
}

void NavigationEntryScreenshot::OnReadBack(bool success) {
  TRACE_EVENT("content", "NavigationEntryScreenshot::OnReadBack");
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // The context provider will no longer be used.
  // This has to run after the readback is completed, otherwise, the destruction
  // of the context provider will crash trying to clean up this request that is
  // currently being processed.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NavigationEntryScreenshot::ResetContextProvider,
                     weak_factory_.GetWeakPtr()));
  if (!success) {
    read_back_bitmap_.reset();
    shared_image_.reset();
    if (screenshot_callback_) {
      SkBitmap override_unused;
      screenshot_callback_.Run({}, false, override_unused);
    }
    return;
  }
  if (screenshot_callback_) {
    SkBitmap bitmap_copy(*read_back_bitmap_);
    bitmap_copy.setImmutable();
    SkBitmap bitmap_override;
    screenshot_callback_.Run(bitmap_copy, true, bitmap_override);
    if (!bitmap_override.drawsNothing()) {
      read_back_bitmap_ = bitmap_override;
    }
  }
  read_back_bitmap_->setImmutable();
  bitmap_ = cc::UIResourceBitmap(*read_back_bitmap_);
  shared_image_.reset();

  SetupCompressionTask(*read_back_bitmap_, supports_etc_non_power_of_two_);
  read_back_bitmap_.reset();
}

void NavigationEntryScreenshot::OnCompressionFinished(
    sk_sp<SkPixelRef> compressed_bitmap) {
  CHECK(!compressed_bitmap_);
  CHECK(bitmap_);
  CHECK(compressed_bitmap);

  const auto size =
      gfx::Size(compressed_bitmap->width(), compressed_bitmap->height());
  compressed_bitmap_ = cc::UIResourceBitmap(std::move(compressed_bitmap), size);
  TRACE_EVENT("navigation", "NavigationEntryScreenshot::OnCompressionFinished",
              "old_size", bitmap_->SizeInBytes(), "new_size",
              compressed_bitmap_->SizeInBytes());

  // We defer discarding the uncompressed bitmap if there is no cache since it
  // may still be in use in the UI.
  if (cache_) {
    bitmap_.reset();
    cache_->OnScreenshotCompressed(unique_id_, GetBitmap().SizeInBytes());
  }
}

bool NavigationEntryScreenshot::IsBitmapReady() const {
  return bitmap_ || compressed_bitmap_;
}

const cc::UIResourceBitmap& NavigationEntryScreenshot::GetBitmap() const {
  return bitmap_ ? *bitmap_ : *compressed_bitmap_;
}

}  // namespace content
