// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"

#include <GLES2/gl2.h>
#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>
#include <sys/mman.h>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/page_size.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "cc/slim/texture_layer.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "content/browser/renderer_host/navigation_controller_delegate.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/android/resources/etc1_utils.h"
#include "ui/gfx/skia_span_util.h"

#ifndef MADV_POPULATE_WRITE
// https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/asm-generic/mman-common.h
#define MADV_POPULATE_WRITE 23
#endif

namespace content {

namespace {

using base::android::ScopedHardwareBufferHandle;

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

SkBitmap PrepareReadBackBitmap(SkImageInfo info) {
  TRACE_EVENT("content", "PrepareReadBackBitmap");
  SkBitmap read_back_bitmap;
  if (!read_back_bitmap.tryAllocPixels(info)) {
    return SkBitmap();
  }
  AdviseBitmap(read_back_bitmap);
  return read_back_bitmap;
}

gfx::Size GetSizeFromHardwareBuffer(AHardwareBuffer* hardware_buffer) {
  AHardwareBuffer_Desc desc;
  AHardwareBuffer_describe(hardware_buffer, &desc);
  return gfx::Size(desc.width, desc.height);
}

}  // namespace

scoped_refptr<cc::slim::TextureLayer>
NavigationEntryScreenshot::SharedImageProvider::CreateTextureLayer() {
  DCHECK(IsValid());
  auto layer = cc::slim::TextureLayer::Create(this);
  pending_transferable_resource_ = true;
  layer->SetContentsOpaque(true);
  layer->NotifyUpdatedResource();
  return layer;
}

bool NavigationEntryScreenshot::SharedImageProvider::
    PrepareTransferableResource(
        viz::TransferableResource* transferable_resource,
        viz::ReleaseCallback* release_callback) {
  if (!pending_transferable_resource_) {
    return false;
  }
  pending_transferable_resource_ = false;
  auto shared_image = Get();
  if (!shared_image) {
    return false;
  }
  // By the time the screenshot is created, the shared_image is already
  // finalized, so no sync token is necessary.
  gpu::SyncToken sync_token;
  *transferable_resource = viz::TransferableResource::Make(
      shared_image, viz::TransferableResource::ResourceSource::kUI, sync_token);
  *release_callback =
      base::BindOnce(&NavigationEntryScreenshot::SharedImageProvider::DoRelease,
                     base::WrapRefCounted(this));
  return true;
}

void NavigationEntryScreenshot::SharedImageProvider::DoRelease(
    const gpu::SyncToken& sync_token,
    bool is_lost) {}

NavigationEntryScreenshot::SharedImageProvider::SharedImageProvider() = default;
NavigationEntryScreenshot::SharedImageProvider::~SharedImageProvider() =
    default;

// static
scoped_refptr<NavigationEntryScreenshot::SharedImageProvider>
NavigationEntryScreenshot::SharedImageHolder::Create(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    viz::ReleaseCallback release_callback) {
  return new SharedImageHolder(std::move(context_provider),
                               std::move(shared_image),
                               std::move(release_callback));
}

bool NavigationEntryScreenshot::SharedImageHolder::IsValid() const {
  return !!context_provider_;
}

scoped_refptr<gpu::ClientSharedImage>
NavigationEntryScreenshot::SharedImageHolder::Get() {
  return shared_image_;
}

gfx::Size NavigationEntryScreenshot::SharedImageHolder::Size() const {
  return shared_image_->size();
}

scoped_refptr<viz::RasterContextProvider>
NavigationEntryScreenshot::SharedImageHolder::GetContextProvider() {
  return context_provider_;
}

void NavigationEntryScreenshot::SharedImageHolder::OnContextLost() {
  context_provider_->RemoveObserver(this);
  context_provider_.reset();
  shared_image_.reset();
}

NavigationEntryScreenshot::SharedImageHolder::~SharedImageHolder() {
  if (release_callback_) {
    std::move(release_callback_).Run(destruction_sync_token_, is_lost_);
  }
  if (context_provider_) {
    context_provider_->RemoveObserver(this);
  }
}

NavigationEntryScreenshot::SharedImageHolder::SharedImageHolder(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    viz::ReleaseCallback release_callback)
    : context_provider_(std::move(context_provider)),
      shared_image_(std::move(shared_image)),
      release_callback_(std::move(release_callback)) {
  context_provider_->AddObserver(this);
}

void NavigationEntryScreenshot::SharedImageHolder::DoRelease(
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  destruction_sync_token_ = sync_token;
  is_lost_ = is_lost_ || is_lost;
  pending_transferable_resource_ = true;
}

// static
scoped_refptr<NavigationEntryScreenshot::SharedImageProvider>
NavigationEntryScreenshot::HardwareBufferHolder::Create(
    NavigationControllerDelegate* nav_controller_delegate,
    ScopedHardwareBufferHandle hardware_buffer,
    base::ScopedClosureRunner release_callback) {
  return new HardwareBufferHolder(nav_controller_delegate,
                                  std::move(hardware_buffer),
                                  std::move(release_callback));
}

bool NavigationEntryScreenshot::HardwareBufferHolder::IsValid() const {
  return true;
}

scoped_refptr<gpu::ClientSharedImage>
NavigationEntryScreenshot::HardwareBufferHolder::Get() {
  if (!cached_shared_image_) {
    auto context_provider = GetContextProvider();
    if (!context_provider) {
      return nullptr;
    }
    auto color_space = nav_controller_delegate_->GetOutputColorSpace(
        gfx::ContentColorUsage::kSRGB,
        /*needs_alpha=*/false);
    cached_shared_image_ =
        context_provider->SharedImageInterface()->CreateSharedImage(
            {viz::SinglePlaneFormat::kRGBA_8888, size_, color_space,
             gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                 // TODO(crbug.com/461842857): Add
                 // gpu::SHARED_IMAGE_USAGE_SCANOUT.
                 gpu::SHARED_IMAGE_USAGE_RASTER_READ,
             "NESHardwareBufferHolderGet"},
            gfx::GpuMemoryBufferHandle(hardware_buffer_.Clone()));
  }
  return cached_shared_image_;
}

gfx::Size NavigationEntryScreenshot::HardwareBufferHolder::Size() const {
  return size_;
}

scoped_refptr<viz::RasterContextProvider>
NavigationEntryScreenshot::HardwareBufferHolder::GetContextProvider() {
  if (!cached_context_provider_) {
    cached_context_provider_ =
        nav_controller_delegate_->GetRasterContextProvider();
    if (cached_context_provider_) {
      cached_context_provider_->AddObserver(this);
    }
  }
  return cached_context_provider_;
}

void NavigationEntryScreenshot::HardwareBufferHolder::DoRelease(
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  pending_transferable_resource_ = true;
}

void NavigationEntryScreenshot::HardwareBufferHolder::OnContextLost() {
  cached_context_provider_->RemoveObserver(this);
  cached_context_provider_.reset();
  cached_shared_image_.reset();
  pending_transferable_resource_ = true;
}

NavigationEntryScreenshot::HardwareBufferHolder::~HardwareBufferHolder() {
  if (cached_context_provider_) {
    cached_context_provider_->RemoveObserver(this);
  }
}

NavigationEntryScreenshot::HardwareBufferHolder::HardwareBufferHolder(
    NavigationControllerDelegate* nav_controller_delegate,
    ScopedHardwareBufferHandle hardware_buffer,
    base::ScopedClosureRunner release_callback)
    : nav_controller_delegate_(nav_controller_delegate),
      hardware_buffer_(std::move(hardware_buffer)),
      size_(GetSizeFromHardwareBuffer(hardware_buffer_.get())),
      release_callback_(std::move(release_callback)) {}

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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SetupCompressionTask(bitmap, supports_etc_non_power_of_two);
}

NavigationEntryScreenshot::NavigationEntryScreenshot(
    scoped_refptr<SharedImageProvider> shared_image_provider,
    NavigationTransitionData::UniqueId unique_id,
    bool supports_etc_non_power_of_two,
    ScreenshotCallback screenshot_callback)
    : performance_scenarios::MatchingScenarioObserver(
          performance_scenarios::kDefaultIdleScenarios),
      shared_image_provider_(std::move(shared_image_provider)),
      unique_id_(unique_id),
      dimensions_without_compression_(shared_image_provider_->Size()),
      supports_etc_non_power_of_two_(supports_etc_non_power_of_two),
      screenshot_callback_(std::move(screenshot_callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto observer_list =
      performance_scenarios::PerformanceScenarioObserverList::GetForScope(
          performance_scenarios::ScenarioScope::kGlobal);
  if (observer_list) {
    observer_list->AddMatchingObserver(this);
    read_back_needed_ = true;
    return;
  }
  StartReadBack();
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
    // SetCache is called when an back-forward transition animation ends. If the
    // readback has finished by now, we no longer need the shared image
    // provider.
    shared_image_provider_.reset();
    return GetBitmap().SizeInBytes();
  }

  return SkColorTypeBytesPerPixel(kN32_SkColorType) *
         dimensions_without_compression_.Area64();
}

void NavigationEntryScreenshot::OnScenarioMatchChanged(
    performance_scenarios::ScenarioScope scope,
    bool matches_pattern) {
  if (!matches_pattern) {
    return;
  }

  if (read_back_needed_) {
    StartReadBack();
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

scoped_refptr<cc::slim::TextureLayer>
NavigationEntryScreenshot::CreateTextureLayer() {
  CHECK(shared_image_provider_);
  DCHECK(!cache_);
  return shared_image_provider_->CreateTextureLayer();
}

bool NavigationEntryScreenshot::PrepareTransferableResource(
    viz::TransferableResource* transferable_resource,
    viz::ReleaseCallback* release_callback) {
  CHECK(shared_image_provider_);
  return shared_image_provider_->PrepareTransferableResource(
      transferable_resource, release_callback);
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

  if (!performance_scenarios::CurrentScenariosMatch(
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

void NavigationEntryScreenshot::MaybeResetSharedImageProvider() {
  // The shared image provider won't be needed anymore, unless it is currently
  // used by an animation. In that case, cache_ is null.
  // Once the animation finishes, it calls SetCache(), which resets the provider
  // if it's no longer needed.
  if (cache_) {
    shared_image_provider_.reset();
  }
}

void NavigationEntryScreenshot::StartReadBack() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(shared_image_provider_);
  auto shared_image = shared_image_provider_->Get();
  if (!shared_image) {
    OnReadBack(SkBitmap(), false);
    return;
  }

  SkImageInfo info = SkImageInfo::MakeN32(shared_image->size().width(),
                                          shared_image->size().height(),
                                          shared_image->alpha_type());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&PrepareReadBackBitmap, info),
      base::BindOnce(&NavigationEntryScreenshot::DoReadBack,
                     weak_factory_.GetWeakPtr()));
}

void NavigationEntryScreenshot::DoReadBack(SkBitmap bitmap) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(shared_image_provider_);
  auto context_provider = shared_image_provider_->GetContextProvider();
  auto shared_image = shared_image_provider_->Get();

  if (bitmap.empty() || !context_provider || !shared_image) {
    OnReadBack(SkBitmap(), false);
    return;
  }

  gfx::Point src_point;
  SkImageInfo info = bitmap.info();
  auto* raster_interface = context_provider->RasterInterface();
  DCHECK(raster_interface);
  auto scoped_access = shared_image->BeginRasterAccess(
      raster_interface, shared_image->creation_sync_token(),
      /*readonly=*/true);
  auto span = gfx::SkPixmapToWritableSpan(bitmap.pixmap());
  raster_interface->ReadbackARGBPixelsAsync(
      shared_image->mailbox(), shared_image->GetTextureTarget(),
      shared_image->surface_origin(), shared_image->size(), src_point, info,
      info.minRowBytes(), span,
      base::BindOnce(&NavigationEntryScreenshot::OnReadBack,
                     weak_factory_.GetWeakPtr(), std::move(bitmap)));
}

void NavigationEntryScreenshot::OnReadBack(SkBitmap bitmap, bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // This has to run after the readback is completed, otherwise, this operation
  // might destroy the context provider, attempting a re-entry to this same
  // callback (crbug.com/456887685).
  GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&NavigationEntryScreenshot::MaybeResetSharedImageProvider,
                     weak_factory_.GetWeakPtr()));
  if (!success) {
    if (screenshot_callback_) {
      SkBitmap override_unused;
      screenshot_callback_.Run({}, false, override_unused);
    }
    // This has to run after the readback is completed, otherwise, if this
    // operation destroys the context provider, it will crash trying to clean
    // up this ReadBack callback that is currently being processed.
    GetUIThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&NavigationEntryScreenshot::MaybeDestroyOnFailure,
                       weak_factory_.GetWeakPtr()));
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

void NavigationEntryScreenshot::MaybeDestroyOnFailure() {
  // We can only destroy the screenshot now if its not in use by an animation.
  // When the animation ends, it attempts to insert the screenshot back into the
  // cache. The screenshot with a pending destruction is discarded at that
  // point.
  pending_destruction_ = true;
  if (cache_) {
    // Destroys this.
    cache_->RemoveFailedScreenshot(this);
  }
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
  return !pending_destruction_ &&
         (IsBitmapReady() ||
          (shared_image_provider_ && shared_image_provider_->IsValid()));
}

bool NavigationEntryScreenshot::IsBitmapReady() const {
  return bitmap_ || compressed_bitmap_;
}

const cc::UIResourceBitmap& NavigationEntryScreenshot::GetBitmap() const {
  return bitmap_ ? *bitmap_ : *compressed_bitmap_;
}

}  // namespace content
