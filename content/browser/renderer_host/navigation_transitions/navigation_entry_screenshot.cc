// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/page_size.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "cc/slim/texture_layer.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/skia_span_util.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
#include <sys/mman.h>

#ifndef MADV_POPULATE_WRITE
// https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/asm-generic/mman-common.h
#define MADV_POPULATE_WRITE 23
#endif

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
#include "ui/android/resources/etc1_utils.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kNavigationEntryScreenshotCompression,
             base::FEATURE_ENABLED_BY_DEFAULT);

static bool g_disable_compression_for_testing = false;

using CompressionDoneCallback = base::OnceCallback<void(sk_sp<SkPixelRef>)>;
void CompressNavigationScreenshotOnWorkerThread(
    SkBitmap bitmap,
    bool supports_etc_non_power_of_two,
    CompressionDoneCallback done_callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Navigation.GestureTransition.CompressionTime");
  TRACE_EVENT0("navigation", "CompressNavigationScreenshotOnWorkerThread");

  sk_sp<SkPixelRef> compressed_bitmap =
      ui::Etc1::CompressBitmap(bitmap, supports_etc_non_power_of_two);

  if (compressed_bitmap) {
    std::move(done_callback).Run(std::move(compressed_bitmap));
  }
}

#endif  // BUILDFLAG(IS_ANDROID)

void AdviseBitmap(SkBitmap& bitmap) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  size_t size = bitmap.info().computeByteSize(bitmap.info().minRowBytes());
  if (madvise(bitmap.getPixels(), size, MADV_POPULATE_WRITE) == 0) {
    return;
  }
  if (EINVAL == errno) {
    // MADV_POPULATE_WRITE is only supported in kernels 5.14 or newer.
    // If it's not supported, we don't want the GPU read back to hit all of the
    // page faults, as it could end up being a long task in the UI thread.
    // Manually pre-fault all pages by writing one byte.
    size_t page_size = base::GetPageSize();
    auto span = gfx::SkPixmapToWritableSpan(bitmap.pixmap());
    for (size_t i = 0; i < span.size(); i += page_size) {
      // Write a value to the first byte of each page
      span[i] = 0;
    }
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace

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
    cache_ = nullptr;
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

  if (IsBitmapReady()) {
    return GetBitmap().SizeInBytes();
  }

  size_t pixel_size = SkColorTypeBytesPerPixel(kN32_SkColorType);
  if (shared_image_) {
    return pixel_size * shared_image_->size().Area64();
  }

  // The shared image was lost, but this is still occupying some space.
  return pixel_size;
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

std::pair<scoped_refptr<cc::slim::TextureLayer>, base::ScopedClosureRunner>
NavigationEntryScreenshot::CreateTextureLayer() {
  CHECK(shared_image_);
  CHECK(texture_transferable_resource_.is_empty());
  // By the time the screenshot is created, the shared_image is already
  // finalized, so no sync token is necessary.
  gpu::SyncToken sync_token;
  texture_transferable_resource_ = viz::TransferableResource::Make(
      shared_image_, viz::TransferableResource::ResourceSource::kUI,
      sync_token);
  // Storing a reference to the shared image in the callback so that it's alive
  // while it's still in use.
  texture_release_callback_ = base::BindOnce(
      [](scoped_refptr<gpu::ClientSharedImage> shared_image,
         const gpu::SyncToken& sync_token, bool lost_resource) {
        shared_image->UpdateDestructionSyncToken(sync_token);
      },
      shared_image_);
  auto layer = cc::slim::TextureLayer::Create(this);
  layer->SetContentsOpaque(true);
  layer->NotifyUpdatedResource();
  return std::make_pair(
      std::move(layer),
      base::ScopedClosureRunner(
          base::BindOnce(&NavigationEntryScreenshot::OnTextureLayerToBeDeleted,
                         weak_factory_.GetMutableWeakPtr())));
}

bool NavigationEntryScreenshot::PrepareTransferableResource(
    viz::TransferableResource* transferable_resource,
    viz::ReleaseCallback* release_callback) {
  CHECK(!texture_transferable_resource_.is_empty());
  if (!texture_release_callback_) {
    return false;
  }
  *transferable_resource = texture_transferable_resource_;
  *release_callback = std::move(texture_release_callback_);
  return true;
}

void NavigationEntryScreenshot::OnTextureLayerToBeDeleted() {
  DCHECK(!texture_transferable_resource_.is_empty());
  texture_transferable_resource_ = viz::TransferableResource();
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
  SkBitmap read_back_bitmap;
  if (!read_back_bitmap.tryAllocPixels(info)) {
    OnReadBack(SkBitmap(), false);
    return;
  }
  AdviseBitmap(read_back_bitmap);
  if (!context_provider_) {
    OnReadBack(SkBitmap(), false);
    return;
  }

  gfx::Point src_point;
  auto* raster_interface = context_provider_->RasterInterface();
  DCHECK(raster_interface);
  auto scoped_access = shared_image_->BeginRasterAccess(
      raster_interface, shared_image_->creation_sync_token(),
      /*readonly=*/true);
  auto span = gfx::SkPixmapToWritableSpan(read_back_bitmap.pixmap());
  raster_interface->ReadbackARGBPixelsAsync(
      shared_image_->mailbox(), shared_image_->GetTextureTarget(),
      shared_image_->surface_origin(), shared_image_->size(), src_point, info,
      info.minRowBytes(), span,
      base::BindOnce(&NavigationEntryScreenshot::OnReadBack,
                     weak_factory_.GetWeakPtr(), std::move(read_back_bitmap)));
}

void NavigationEntryScreenshot::OnReadBack(SkBitmap bitmap, bool success) {
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
  shared_image_.reset();
  if (!success) {
    if (screenshot_callback_) {
      SkBitmap override_unused;
      screenshot_callback_.Run({}, false, override_unused);
    }
    if (cache_) {
      // Destroys this.
      cache_->RemoveFailedScreenshot(this);
    }
    return;
  }
  if (screenshot_callback_) {
    SkBitmap bitmap_copy(bitmap);
    bitmap_copy.setImmutable();
    SkBitmap bitmap_override;
    screenshot_callback_.Run(bitmap_copy, true, bitmap_override);
    if (!bitmap_override.drawsNothing()) {
      bitmap = bitmap_override;
    }
  }
  bitmap.setImmutable();
  bitmap_ = cc::UIResourceBitmap(bitmap);

  SetupCompressionTask(bitmap, supports_etc_non_power_of_two_);
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

bool NavigationEntryScreenshot::IsValid() const {
  return shared_image_ || IsBitmapReady();
}

bool NavigationEntryScreenshot::IsBitmapReady() const {
  return bitmap_ || compressed_bitmap_;
}

const cc::UIResourceBitmap& NavigationEntryScreenshot::GetBitmap() const {
  return bitmap_ ? *bitmap_ : *compressed_bitmap_;
}

}  // namespace content
