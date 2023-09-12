// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_hibernation_handler.h"

#include "base/feature_list.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/codec/SkPngDecoder.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

namespace blink {

// static
HibernatedCanvasMemoryDumpProvider&
HibernatedCanvasMemoryDumpProvider::GetInstance() {
  static base::NoDestructor<HibernatedCanvasMemoryDumpProvider> instance;
  return *instance.get();
}

void HibernatedCanvasMemoryDumpProvider::Register(
    CanvasHibernationHandler* handler) {
  DCHECK(IsMainThread());
  base::AutoLock locker(lock_);
  DCHECK(handler->IsHibernating());
  handlers_.insert(handler);
}

void HibernatedCanvasMemoryDumpProvider::Unregister(
    CanvasHibernationHandler* handler) {
  DCHECK(IsMainThread());
  base::AutoLock locker(lock_);
  DCHECK(handlers_.Contains(handler));
  handlers_.erase(handler);
}

bool HibernatedCanvasMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(IsMainThread());

  size_t total_hibernated_size = 0;
  size_t total_original_size = 0;
  auto* dump = pmd->CreateAllocatorDump("canvas/hibernated");

  {
    base::AutoLock locker(lock_);
    int index = 0;
    for (CanvasHibernationHandler* handler : handlers_) {
      DCHECK(handler->IsHibernating());
      total_original_size += handler->original_memory_size();
      total_hibernated_size += handler->memory_size();

      if (args.level_of_detail ==
          base::trace_event::MemoryDumpLevelOfDetail::kDetailed) {
        auto* canvas_dump = pmd->CreateAllocatorDump(
            base::StringPrintf("canvas/hibernated/canvas_%d", index));
        canvas_dump->AddScalar("memory_size", "bytes", handler->memory_size());
        canvas_dump->AddScalar("is_encoded", "boolean", handler->is_encoded());
        canvas_dump->AddScalar("original_memory_size", "bytes",
                               handler->original_memory_size());
        canvas_dump->AddScalar("height", "pixels", handler->height());
        canvas_dump->AddScalar("width", "pixels", handler->width());
      }
      index++;
    }
  }

  dump->AddScalar("size", "bytes", total_hibernated_size);
  dump->AddScalar("original_size", "bytes", total_original_size);

  return true;
}

HibernatedCanvasMemoryDumpProvider::HibernatedCanvasMemoryDumpProvider() {
  DCHECK(IsMainThread());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "hibernated_canvas",
      blink::Thread::MainThread()->GetTaskRunner(
          MainThreadTaskRunnerRestricted()));
}

CanvasHibernationHandler::~CanvasHibernationHandler() {
  DCheckInvariant();
  if (IsHibernating()) {
    HibernatedCanvasMemoryDumpProvider::GetInstance().Unregister(this);
  }
}

void CanvasHibernationHandler::TakeHibernationImage(sk_sp<SkImage>&& image) {
  DCheckInvariant();
  epoch_++;
  image_ = image;

  width_ = image_->width();
  height_ = image_->height();
  bytes_per_pixel_ = image_->imageInfo().bytesPerPixel();

  // If we had an encoded version, discard it.
  encoded_.reset();

  HibernatedCanvasMemoryDumpProvider::GetInstance().Register(this);

  // Don't bother compressing very small canvases.
  if (ImageMemorySize(*image) < 16 * 1024 ||
      !base::FeatureList::IsEnabled(features::kCanvasCompressHibernatedImage)) {
    return;
  }

  // Don't post the compression task to the thread pool with a delay right away.
  // The task increases the reference count on the SkImage. In the case of rapid
  // foreground / background transitions, each transition allocates a new
  // SkImage. If we post a compression task right away with a sk_sp<SkImage> as
  // a parameter, this takes a reference on the underlying SkImage, keeping it
  // alive until the task runs. This means that posting the compression task
  // right away would increase memory usage by a lot in these cases.
  //
  // Rather, post a main thread task later that will check whether we are still
  // in hibernation mode, and in the same hibernation "epoch" as last time. If
  // this is the case, then compress.
  //
  // This simplifies tracking of background / foreground cycles, at the cost of
  // running one extra trivial task for each cycle.
  //
  // Note: not using a delayed idle tasks, because idle tasks do not run when
  // the renderer is idle. In other words, a delayed idle task would not execute
  // as long as the renderer is in background, which completely defeats the
  // purpose.
  GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CanvasHibernationHandler::OnAfterHibernation,
                     weak_ptr_factory_.GetWeakPtr(), epoch_),
      kBeforeCompressionDelay);
}

void CanvasHibernationHandler::OnAfterHibernation(uint64_t epoch) {
  DCheckInvariant();
  DCHECK(
      base::FeatureList::IsEnabled(features::kCanvasCompressHibernatedImage));
  // Either we no longer have the image (because we are not hibernating), or we
  // went through another visible / not visible cycle (in which case it is too
  // early to compress).
  if (epoch_ != epoch || !image_) {
    return;
  }
  auto task_runner = GetMainThreadTaskRunner();
  auto params = std::make_unique<BackgroundTaskParams>(
      image_, epoch_, weak_ptr_factory_.GetWeakPtr(), task_runner);

  if (background_thread_task_runner_for_testing_) {
    background_thread_task_runner_for_testing_->PostTask(
        FROM_HERE,
        base::BindOnce(&CanvasHibernationHandler::Encode, std::move(params)));
  } else {
    worker_pool::PostTask(FROM_HERE, {base::TaskPriority::BEST_EFFORT},
                          CrossThreadBindOnce(&CanvasHibernationHandler::Encode,
                                              std::move(params)));
  }
}

void CanvasHibernationHandler::OnEncoded(
    std::unique_ptr<CanvasHibernationHandler::BackgroundTaskParams> params,
    sk_sp<SkData> encoded) {
  DCheckInvariant();
  DCHECK(
      base::FeatureList::IsEnabled(features::kCanvasCompressHibernatedImage));
  // Discard the compressed image, it is no longer current.
  if (params->epoch != epoch_ || !IsHibernating()) {
    return;
  }

  DCHECK_EQ(image_.get(), params->image.get());
  encoded_ = encoded;
  image_ = nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
CanvasHibernationHandler::GetMainThreadTaskRunner() const {
  return main_thread_task_runner_for_testing_
             ? main_thread_task_runner_for_testing_
             : Thread::MainThread()->GetTaskRunner(
                   MainThreadTaskRunnerRestricted());
}

void CanvasHibernationHandler::Encode(
    std::unique_ptr<CanvasHibernationHandler::BackgroundTaskParams> params) {
  TRACE_EVENT0("blink", __PRETTY_FUNCTION__);
  DCHECK(
      base::FeatureList::IsEnabled(features::kCanvasCompressHibernatedImage));
  sk_sp<SkData> encoded =
      SkPngEncoder::Encode(nullptr, params->image.get(), {});

  size_t original_memory_size = ImageMemorySize(*params->image);
  int compression_ratio_percentage = static_cast<int>(
      (static_cast<size_t>(100) * encoded->size()) / original_memory_size);
  UMA_HISTOGRAM_PERCENTAGE("Blink.Canvas.2DLayerBridge.Compression.Ratio",
                           compression_ratio_percentage);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Blink.Canvas.2DLayerBridge.Compression.SnapshotSizeKb",
      static_cast<int>(original_memory_size / 1024), 10, 500000, 50);

  auto* reply_task_runner = params->reply_task_runner.get();
  reply_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CanvasHibernationHandler::OnEncoded,
                     params->weak_instance, std::move(params), encoded));
}

sk_sp<SkImage> CanvasHibernationHandler::GetImage() {
  TRACE_EVENT0("blink", __PRETTY_FUNCTION__);
  DCheckInvariant();
  if (image_) {
    return image_;
  }

  CHECK(encoded_);
  CHECK(SkPngDecoder::IsPng(encoded_->data(), encoded_->size()));
  DCHECK(
      base::FeatureList::IsEnabled(features::kCanvasCompressHibernatedImage));

  base::TimeTicks before = base::TimeTicks::Now();
  // Note: not discarding the encoded image.
  sk_sp<SkImage> image = nullptr;
  std::unique_ptr<SkCodec> codec = SkPngDecoder::Decode(encoded_, nullptr);
  if (codec) {
    image = std::get<0>(codec->getImage());
  }

  base::TimeTicks after = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES(
      "Blink.Canvas.2DLayerBridge.Compression.DecompressionTime",
      after - before);
  return image;
  ;
}

void CanvasHibernationHandler::Clear() {
  DCheckInvariant();
  HibernatedCanvasMemoryDumpProvider::GetInstance().Unregister(this);
  encoded_ = nullptr;
  image_ = nullptr;
}

size_t CanvasHibernationHandler::memory_size() const {
  DCheckInvariant();
  DCHECK(IsHibernating());
  if (is_encoded()) {
    return encoded_->size();
  } else {
    return original_memory_size();
  }
}

// static
size_t CanvasHibernationHandler::ImageMemorySize(const SkImage& image) {
  return static_cast<size_t>(image.height()) * image.width() *
         image.imageInfo().bytesPerPixel();
}

size_t CanvasHibernationHandler::original_memory_size() const {
  return static_cast<size_t>(width_) * height_ * bytes_per_pixel_;
}

}  // namespace blink
