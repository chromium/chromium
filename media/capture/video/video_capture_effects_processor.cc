// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_effects_processor.h"

#include <optional>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/video_types.h"
#include "media/capture/video/video_capture_gpu_channel_host.h"
#include "ui/gl/gl_bindings.h"

namespace media {
namespace {

// Helper, creates VideoBufferHandle for the given buffer.
// `shared_image` will be populated if the returned buffer handle refers to a
// GPU memory buffer. This is needed to ensure that the caller can maintain
// ownership of the shared image while it's in use.
mojom::VideoBufferHandlePtr CreateBufferHandle(
    const VideoCaptureDevice::Client::Buffer& buffer,
    const mojom::VideoFrameInfo& frame_info,
    VideoCaptureBufferType buffer_type,
    scoped_refptr<gpu::ClientSharedImage>& shared_image) {
  switch (buffer_type) {
    case VideoCaptureBufferType::kSharedMemory:
      // TODO(https://crbug.com/40222341): we don't need to return an
      // `UnsafeShmemRegion` here but `buffer.handle_provider` does not have an
      // option to return `ReadOnlySharedMemoryRegion`.
      return mojom::VideoBufferHandle::NewUnsafeShmemRegion(
          buffer.handle_provider->DuplicateAsUnsafeRegion());
    case VideoCaptureBufferType::kGpuMemoryBuffer: {
      CHECK_EQ(frame_info.pixel_format, VideoPixelFormat::PIXEL_FORMAT_NV12);

      auto* sii =
          VideoCaptureGpuChannelHost::GetInstance().SharedImageInterface();
      CHECK(sii);

      // Create a single shared image to back a multiplanar video frame.
      gpu::SharedImageInfo info(
          viz::MultiPlaneFormat::kNV12, frame_info.coded_size,
          frame_info.color_space,
          gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
              gpu::SHARED_IMAGE_USAGE_RASTER_READ,
          "VideoCaptureEffectsProcessorMultiPlanarSharedImage");
      shared_image = sii->CreateSharedImage(
          std::move(info), buffer.handle_provider->GetGpuMemoryBufferHandle());
      CHECK(shared_image);

      auto sync_token = sii->GenVerifiedSyncToken();
      auto shared_image_set = mojom::SharedImageBufferHandleSet::New(
          shared_image->Export(), sync_token);

      return mojom::VideoBufferHandle::NewSharedImageHandle(
          std::move(shared_image_set));
    }
    default:
      NOTREACHED();
  }
}

}  // namespace

PostProcessDoneInfo::PostProcessDoneInfo(
    VideoCaptureDevice::Client::Buffer buffer,
    mojom::VideoFrameInfoPtr info)
    : buffer(std::move(buffer)), info(std::move(info)) {}

PostProcessDoneInfo::PostProcessDoneInfo(PostProcessDoneInfo&& other) = default;
PostProcessDoneInfo& PostProcessDoneInfo::operator=(
    PostProcessDoneInfo&& other) = default;

PostProcessDoneInfo::~PostProcessDoneInfo() = default;

VideoCaptureEffectsProcessor::VideoCaptureEffectsProcessor(
    mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
        video_effects_processor)
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      effects_processor_(std::move(video_effects_processor)) {}

VideoCaptureEffectsProcessor::~VideoCaptureEffectsProcessor() {
  // Make sure that the remote is destroyed from the same sequence that it was
  // created on.
  task_runner_->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(effects_processor_)));
}

void VideoCaptureEffectsProcessor::PostProcessData(
    base::span<const uint8_t> data,
    mojom::VideoFrameInfoPtr frame_info,
    VideoCaptureDevice::Client::Buffer out_buffer,
    const VideoCaptureFormat& out_buffer_format,
    VideoCaptureBufferType out_buffer_type,
    VideoCaptureEffectsProcessor::PostProcessDoneCallback post_process_cb) {
  CHECK(!data.empty());

  auto in_buffer_mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(data.size());
  if (!in_buffer_mapped_region.IsValid()) {
    // TODO(bialpio): this was not a post-processing error but we have to claim
    // that it was, we have painted ourselves into a corner here. It may be OK
    // to leave as-is if we think shmem creation failure is extremely unlikely.
    std::move(post_process_cb)
        .Run(
            base::unexpected(video_effects::mojom::PostProcessError::kUnknown));
    return;
  }

  in_buffer_mapped_region.mapping.GetMemoryAsSpan<uint8_t>().copy_from(data);

  mojom::VideoBufferHandlePtr in_buffer_handle =
      mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
          std::move(in_buffer_mapped_region.region));

  auto out_frame_info = frame_info->Clone();
  out_frame_info->pixel_format = out_buffer_format.pixel_format;

  scoped_refptr<gpu::ClientSharedImage> out_shared_image;
  mojom::VideoBufferHandlePtr out_buffer_handle = CreateBufferHandle(
      out_buffer, *out_frame_info, out_buffer_type, out_shared_image);

  PostProcessContext context(std::nullopt, {}, std::move(out_buffer),
                             std::move(out_shared_image),
                             std::move(post_process_cb));

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // VideoCaptureDeviceClient can call us from any thread (the only guarantee
    // we get is that one thread at a time will call us, which is enforced by
    // `DFAKE_SCOPED_RECURSIVE_LOCK`), so a thread-hop to the correct sequence
    // may be needed.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &VideoCaptureEffectsProcessor::PostProcessDataOnValidSequence,
            weak_ptr_factory_.GetWeakPtr(), std::move(context),
            std::move(in_buffer_handle), std::move(frame_info),
            std::move(out_buffer_handle), out_buffer_format));
    return;
  }

  PostProcessDataOnValidSequence(
      std::move(context), std::move(in_buffer_handle), std::move(frame_info),
      std::move(out_buffer_handle), out_buffer_format);
}

void VideoCaptureEffectsProcessor::PostProcessDataOnValidSequence(
    PostProcessContext context,
    mojom::VideoBufferHandlePtr in_buffer_handle,
    mojom::VideoFrameInfoPtr frame_info,
    mojom::VideoBufferHandlePtr out_buffer_handle,
    const VideoCaptureFormat& out_buffer_format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
      "PostProcessContext::PostProcessContext()", context.trace_id);

  effects_processor_->PostProcess(
      std::move(in_buffer_handle), std::move(frame_info),
      std::move(out_buffer_handle), out_buffer_format.pixel_format,
      base::BindOnce(&VideoCaptureEffectsProcessor::OnPostProcess,
                     weak_ptr_factory_.GetWeakPtr(), std::move(context)));
}

void VideoCaptureEffectsProcessor::PostProcessBuffer(
    VideoCaptureDevice::Client::Buffer in_buffer,
    mojom::VideoFrameInfoPtr frame_info,
    VideoCaptureBufferType in_buffer_type,
    VideoCaptureDevice::Client::Buffer out_buffer,
    const VideoCaptureFormat& out_buffer_format,
    VideoCaptureBufferType out_buffer_type,
    VideoCaptureEffectsProcessor::PostProcessDoneCallback post_process_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<gpu::ClientSharedImage> in_shared_image;
  mojom::VideoBufferHandlePtr in_buffer_handle = CreateBufferHandle(
      in_buffer, *frame_info, in_buffer_type, in_shared_image);

  auto out_frame_info = frame_info->Clone();
  out_frame_info->pixel_format = out_buffer_format.pixel_format;

  scoped_refptr<gpu::ClientSharedImage> out_shared_image;
  mojom::VideoBufferHandlePtr out_buffer_handle = CreateBufferHandle(
      out_buffer, *out_frame_info, out_buffer_type, out_shared_image);

  PostProcessContext context(std::move(in_buffer), std::move(in_shared_image),
                             std::move(out_buffer), std::move(out_shared_image),
                             std::move(post_process_cb));

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
      "PostProcessContext::PostProcessContext()", context.trace_id);

  effects_processor_->PostProcess(
      std::move(in_buffer_handle), std::move(frame_info),
      std::move(out_buffer_handle), out_buffer_format.pixel_format,
      base::BindOnce(&VideoCaptureEffectsProcessor::OnPostProcess,
                     weak_ptr_factory_.GetWeakPtr(), std::move(context)));
}

void VideoCaptureEffectsProcessor::OnPostProcess(
    VideoCaptureEffectsProcessor::PostProcessContext context,
    video_effects::mojom::PostProcessResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
      "PostProcessContext::PostProcessContext()", context.trace_id,
      "is_success", result->is_success());

  switch (result->which()) {
    case video_effects::mojom::PostProcessResult::Tag::kSuccess: {
      std::move(context.post_process_cb)
          .Run(base::ok(PostProcessDoneInfo(
              std::move(context.out_buffer),
              std::move(result->get_success()->frame_info))));
      break;
    }
    case video_effects::mojom::PostProcessResult::Tag::kError: {
      std::move(context.post_process_cb)
          .Run(base::unexpected(result->get_error()));
      break;
    }
  }
}

VideoCaptureEffectsProcessor::PostProcessContext::PostProcessContext(
    std::optional<VideoCaptureDevice::Client::Buffer> in_buffer,
    scoped_refptr<gpu::ClientSharedImage> in_shared_image,
    VideoCaptureDevice::Client::Buffer out_buffer,
    scoped_refptr<gpu::ClientSharedImage> out_shared_image,
    VideoCaptureEffectsProcessor::PostProcessDoneCallback post_process_cb)
    : in_buffer(std::move(in_buffer)),
      in_shared_image(std::move(in_shared_image)),
      out_buffer(std::move(out_buffer)),
      out_shared_image(std::move(out_shared_image)),
      post_process_cb(std::move(post_process_cb)) {}

VideoCaptureEffectsProcessor::PostProcessContext::PostProcessContext(
    VideoCaptureEffectsProcessor::PostProcessContext&& other) = default;

VideoCaptureEffectsProcessor::PostProcessContext::~PostProcessContext() =
    default;

}  // namespace media
