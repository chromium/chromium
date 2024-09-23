// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_hibernation_handler.h"

#include "base/feature_list.h"
#include "base/memory/post_delayed_memory_reduction_task.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "third_party/blink/renderer/platform/bindings/buildflags.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
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

#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
// "GN check" doesn't know that this file is only included when
// BUILDFLAG(HAS_ZSTD_COMPRESSION) is true. Disable it here.
#include "third_party/zstd/src/lib/zstd.h"  // nogncheck
#endif

namespace blink {

// Use ZSTD to compress the snapshot. This is faster to decompress, and much
// faster to compress. ZSTD may not be available on all platforms, so this
// feature will be a no-op on those.
BASE_FEATURE(kCanvasHibernationSnapshotZstd,
             "CanvasHibernationSnapshotZstd",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

void CanvasHibernationHandler::SaveForHibernation(
    sk_sp<SkImage>&& image,
    std::unique_ptr<MemoryManagedPaintRecorder> recorder) {
  DCheckInvariant();
  DCHECK(image);
  epoch_++;
  image_ = image;
  recorder_ = std::move(recorder);

  width_ = image_->width();
  height_ = image_->height();
  bytes_per_pixel_ = image_->imageInfo().bytesPerPixel();

  // If we had an encoded version, discard it.
  encoded_.reset();

  HibernatedCanvasMemoryDumpProvider::GetInstance().Register(this);

  // Don't bother compressing very small canvases.
  if (ImageMemorySize(*image_) < 16 * 1024) {
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
  base::PostDelayedMemoryReductionTask(
      GetMainThreadTaskRunner(), FROM_HERE,
      base::BindOnce(&CanvasHibernationHandler::OnAfterHibernation,
                     weak_ptr_factory_.GetWeakPtr(), epoch_),
      before_compression_delay_);
}

void CanvasHibernationHandler::OnAfterHibernation(uint64_t epoch) {
  DCheckInvariant();
  // Either we no longer have the image (because we are not hibernating), or we
  // went through another visible / not visible cycle (in which case it is too
  // early to compress).
  if (epoch_ != epoch || !image_) {
    return;
  }
  auto task_runner = GetMainThreadTaskRunner();
  algorithm_ = CompressionAlgorithm::kZlib;
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
  if (base::FeatureList::IsEnabled(kCanvasHibernationSnapshotZstd)) {
    algorithm_ = CompressionAlgorithm::kZstd;
  }
#endif
  auto params = std::make_unique<BackgroundTaskParams>(
      image_, epoch_, algorithm_, weak_ptr_factory_.GetWeakPtr(), task_runner);

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
  // Store the compressed image if it is still current.
  if (params->epoch == epoch_ && IsHibernating()) {
    DCHECK_EQ(image_.get(), params->image.get());
    encoded_ = encoded;
    image_ = nullptr;
  }

  if (on_encoded_callback_for_testing_) {
    on_encoded_callback_for_testing_.Run();
  }
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
  // Using thread time, since this is a BEST_EFFORT task, which may be
  // descheduled.
  base::ElapsedThreadTimer thread_timer;

  sk_sp<SkData> encoded = nullptr;

  switch (params->algorithm) {
    case CompressionAlgorithm::kZlib:
      encoded = SkPngEncoder::Encode(nullptr, params->image.get(), {});
      break;
    case CompressionAlgorithm::kZstd: {
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
      SkPngEncoder::Options options;
      // When the compression level is set to 0, no compression is done. Then we
      // can pass the result to ZSTD. This won't produce a valid PNG, but it
      // doesn't matter, as we don't write it to disk, and restore it ourselves.
      options.fZLibLevel = 0;
      sk_sp<SkData> encoded_uncompressed =
          SkPngEncoder::Encode(nullptr, params->image.get(), options);

      TRACE_EVENT_BEGIN2("blink", "ZstdCompression", "original_size", 0, "size",
                         0);
      size_t uncompressed_size = encoded_uncompressed->size();
      size_t buffer_size = ZSTD_compressBound(encoded_uncompressed->size());
      std::vector<char> compressed_buffer(buffer_size);
      // Field data show that compression ratios are very good in practice with
      // zlib (most often better than 10:1), so prefer fast (de)compression to
      // higher compression ratios.
      constexpr int kZstdCompressionLevel = 1;
      size_t compressed_size =
          ZSTD_compress(compressed_buffer.data(), compressed_buffer.size(),
                        encoded_uncompressed->data(),
                        encoded_uncompressed->size(), kZstdCompressionLevel);
      CHECK(!ZSTD_isError(compressed_size))
          << "Error: " << ZSTD_getErrorName(uncompressed_size);
      // Free the uncompressed version before making the allocation
      // below. Likely doesn't matter much though, as the compressed version is
      // much smaller.
      encoded_uncompressed = nullptr;
      encoded = SkData::MakeWithCopy(compressed_buffer.data(), compressed_size);
      TRACE_EVENT_END2("blink", "ZstdCompression", "original_size",
                       uncompressed_size, "size", compressed_size);
      break;
#else
      NOTREACHED();
#endif  // BUILDFLAG(HAS_ZSTD_COMPRESSION)
    }
  }

  size_t original_memory_size = ImageMemorySize(*params->image);
  int compression_ratio_percentage = static_cast<int>(
      (static_cast<size_t>(100) * encoded->size()) / original_memory_size);
  UMA_HISTOGRAM_TIMES("Blink.Canvas.2DLayerBridge.Compression.ThreadTime",
                      thread_timer.Elapsed());
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

  sk_sp<SkData> png_data = nullptr;
  switch (algorithm_) {
    case CompressionAlgorithm::kZlib:
      // Nothing to do to prepare the PNG data.
      png_data = encoded_;
      break;
    case CompressionAlgorithm::kZstd: {
#if BUILDFLAG(HAS_ZSTD_COMPRESSION)
      uint64_t content_size =
          ZSTD_getFrameContentSize(encoded_->data(), encoded_->size());
      // The CHECK()s below indicate memory corruption, terminate.
      CHECK_NE(content_size, ZSTD_CONTENTSIZE_UNKNOWN);
      CHECK_NE(content_size, ZSTD_CONTENTSIZE_ERROR);
      // Only needed when sizeof(size_t) == 4, but kept everywhere.
      CHECK_LE(content_size, std::numeric_limits<size_t>::max());
      png_data = SkData::MakeUninitialized(static_cast<size_t>(content_size));
      size_t uncompressed_size = ZSTD_decompress(
          static_cast<char*>(png_data->writable_data()), png_data->size(),
          static_cast<const char*>(encoded_->data()), encoded_->size());
      CHECK(!ZSTD_isError(uncompressed_size))
          << "Error: " << ZSTD_getErrorName(uncompressed_size);
      break;
#else
      NOTREACHED();
#endif
    }
  }

  CHECK(SkPngDecoder::IsPng(png_data->data(), png_data->size()));

  base::TimeTicks before = base::TimeTicks::Now();
  // Note: not discarding the encoded image.
  sk_sp<SkImage> image = nullptr;
  std::unique_ptr<SkCodec> codec = SkPngDecoder::Decode(png_data, nullptr);
  if (codec) {
    image = std::get<0>(codec->getImage());
  }

  base::TimeTicks after = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES(
      "Blink.Canvas.2DLayerBridge.Compression.DecompressionTime",
      after - before);
  return image;
}

void CanvasHibernationHandler::Clear() {
  DCheckInvariant();
  HibernatedCanvasMemoryDumpProvider::GetInstance().Unregister(this);
  encoded_ = nullptr;
  image_ = nullptr;
  recorder_ = nullptr;
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
