// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_utils.h"

#include "base/check.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/GraphiteTypes.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"

namespace gpu {
namespace {

struct ReadPixelsContext {
  std::unique_ptr<const SkImage::AsyncReadResult> async_result;
  bool finished = false;
};

void OnReadPixelsDone(
    void* raw_ctx,
    std::unique_ptr<const SkImage::AsyncReadResult> async_result) {
  ReadPixelsContext* context = reinterpret_cast<ReadPixelsContext*>(raw_ctx);
  context->async_result = std::move(async_result);
  context->finished = true;
}

// Synchronously read pixels from a graphite image.
template <typename T>
bool GraphiteReadPixelsSyncImpl(skgpu::graphite::Context* context,
                                skgpu::graphite::Recorder* recorder,
                                T* imageOrSurface,
                                const SkImageInfo& dst_info,
                                void* dst_pointer,
                                size_t dst_bytes_per_row,
                                int src_x,
                                int src_y) {
  GraphiteFlush(context, recorder);

  ReadPixelsContext read_context;
  const SkIRect src_rect =
      SkIRect::MakeXYWH(src_x, src_x, dst_info.width(), dst_info.height());

  context->asyncRescaleAndReadPixels(
      imageOrSurface, dst_info, src_rect, SkImage::RescaleGamma::kSrc,
      SkImage::RescaleMode::kRepeatedLinear, &OnReadPixelsDone, &read_context);

  context->submit(skgpu::graphite::SyncToCpu::kYes);
  CHECK(read_context.finished);

  if (!read_context.async_result) {
    return false;
  }

  SkPixmap pixmap(dst_info, read_context.async_result->data(0),
                  read_context.async_result->rowBytes(0));
  return pixmap.readPixels(dst_info, dst_pointer, dst_bytes_per_row);
}

}  // namespace

void GraphiteFlush(skgpu::graphite::Context* context,
                   skgpu::graphite::Recorder* recorder) {
  auto recording = recorder->snap();
  if (recording) {
    context->insertRecording({recording.get()});
  }
}

void GraphiteFlushAndSubmit(skgpu::graphite::Context* context,
                            skgpu::graphite::Recorder* recorder) {
  GraphiteFlush(context, recorder);
  context->submit();
}

bool GraphiteReadPixelsSync(skgpu::graphite::Context* context,
                            skgpu::graphite::Recorder* recorder,
                            SkImage* image,
                            const SkImageInfo& dst_info,
                            void* dst_pointer,
                            size_t dst_bytes_per_row,
                            int src_x,
                            int src_y) {
  return GraphiteReadPixelsSyncImpl(context, recorder, image, dst_info,
                                    dst_pointer, dst_bytes_per_row, src_x,
                                    src_y);
}

bool GraphiteReadPixelsSync(skgpu::graphite::Context* context,
                            skgpu::graphite::Recorder* recorder,
                            SkSurface* surface,
                            const SkImageInfo& dst_info,
                            void* dst_pointer,
                            size_t dst_bytes_per_row,
                            int src_x,
                            int src_y) {
  return GraphiteReadPixelsSyncImpl(context, recorder, surface, dst_info,
                                    dst_pointer, dst_bytes_per_row, src_x,
                                    src_y);
}

}  // namespace gpu
