// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_frame_converter.h"

#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_io_surface.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_shared_image_interface.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "media/base/mac/color_space_util_mac.h"
#include "media/base/mac/video_frame_mac.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_types.h"
#include "media/gpu/mac/video_toolbox_decompression_metadata.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

namespace {

// The SharedImages created by this class to back VideoFrames can be read by the
// raster interface for canvas and by the GLES2 interface for WebGL in addition
// to being sent to the display compositor and/or used as overlays.
constexpr gpu::SharedImageUsageSet kSharedImageUsage =
    gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
    gpu::SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX |
    gpu::SHARED_IMAGE_USAGE_RASTER_READ | gpu::SHARED_IMAGE_USAGE_GLES2_READ;

constexpr char kSharedImageDebugLabel[] = "VideoToolboxVideoDecoder";

std::optional<viz::SharedImageFormat> PixelFormatToImageFormat(
    OSType pixel_format) {
  switch (pixel_format) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
      return viz::MultiPlaneFormat::kNV12;
    case kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange:
      return viz::MultiPlaneFormat::kNV16;
    case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange:
      return viz::MultiPlaneFormat::kNV24;
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
      return viz::MultiPlaneFormat::kP010;
    case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange:
      return viz::MultiPlaneFormat::kP210;
    case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange:
      return viz::MultiPlaneFormat::kP410;
    case kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar:
      return viz::MultiPlaneFormat::kNV12A;
    default:
      return std::nullopt;
  }
}

VideoPixelFormat PixelFormatToVideoPixelFormat(OSType pixel_format) {
  switch (pixel_format) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
      return PIXEL_FORMAT_NV12;
    case kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange:
      return PIXEL_FORMAT_NV16;
    case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange:
      return PIXEL_FORMAT_NV24;
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
      return PIXEL_FORMAT_P010LE;
    case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange:
      return PIXEL_FORMAT_P210LE;
    case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange:
      return PIXEL_FORMAT_P410LE;
    case kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar:
      return PIXEL_FORMAT_NV12A;
    default:
      return PIXEL_FORMAT_UNKNOWN;
  }
}

// If enabled, adds SHARED_IMAGE_USAGE_WEBGPU_READ as a usage when creating
// SharedImages for a WebGpu-compatible IOSurface. Intended as a killswitch
// to guard against performance regressions.
// TODO: crbug.com/349290188 - Clean up if no performance regressions are
// observed.
BASE_FEATURE(kVideoToolboxFrameConverterSpecifyWebGpuUsage,
             "VideoToolboxFrameConverterSpecifyWebGpuUsage",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

VideoToolboxFrameConverter::VideoToolboxFrameConverter(
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    GetCommandBufferStubCB get_stub_cb)
    : base::RefCountedDeleteOnSequence<VideoToolboxFrameConverter>(
          gpu_task_runner),
      gpu_task_runner_(std::move(gpu_task_runner)),
      media_log_(std::move(media_log)),
      get_stub_cb_(std::move(get_stub_cb)) {
  DVLOG(1) << __func__;
  DCHECK(get_stub_cb_);
}

VideoToolboxFrameConverter::~VideoToolboxFrameConverter() {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->RunsTasksInCurrentSequence());
  DestroyStub();
}

void VideoToolboxFrameConverter::OnWillDestroyStub(bool have_context) {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->RunsTasksInCurrentSequence());
  DestroyStub();
}

void VideoToolboxFrameConverter::Initialize() {
  DVLOG(4) << __func__;
  DCHECK(gpu_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!initialized_);

  initialized_ = true;

  stub_ = std::move(get_stub_cb_).Run();
  if (!stub_) {
    MEDIA_LOG(ERROR, media_log_.get()) << "Failed to get command buffer stub";
    return;
  }

  DCHECK(stub_->channel()->task_runner()->BelongsToCurrentThread());

  stub_->AddDestructionObserver(this);

  // kHigh priority to work around crbug.com/1035750.
  wait_sequence_id_ = stub_->channel()->scheduler()->CreateSequence(
      gpu::SchedulingPriority::kHigh, stub_->channel()->task_runner());

  sis_ = stub_->channel()->shared_image_stub();
  if (!sis_) {
    MEDIA_LOG(ERROR, media_log_.get()) << "Failed to get shared image stub";
    DestroyStub();
    return;
  }
}

void VideoToolboxFrameConverter::DestroyStub() {
  DVLOG(4) << __func__;
  DCHECK(gpu_task_runner_->RunsTasksInCurrentSequence());

  sis_ = nullptr;

  if (stub_) {
    stub_->channel()->scheduler()->DestroySequence(wait_sequence_id_);
    stub_->RemoveDestructionObserver(this);
    stub_ = nullptr;
  }
}

void VideoToolboxFrameConverter::Convert(
    base::apple::ScopedCFTypeRef<CVImageBufferRef> image,
    std::unique_ptr<VideoToolboxDecodeMetadata> metadata,
    OutputCB output_cb) {
  DVLOG(3) << __func__;
  DCHECK(gpu_task_runner_->RunsTasksInCurrentSequence());

  if (!initialized_) {
    Initialize();
  }

  if (!stub_) {
    MEDIA_LOG(ERROR, media_log_.get()) << "Command buffer stub is missing";
    std::move(output_cb).Run(nullptr, std::move(metadata));
    return;
  }

  const gfx::Size coded_size(CVPixelBufferGetWidth(image.get()),
                             CVPixelBufferGetHeight(image.get()));
  const gfx::Rect visible_rect(CVImageBufferGetCleanRect(image.get()));
  const gfx::Size natural_size =
      metadata->aspect_ratio.GetNaturalSize(visible_rect);

  bool allow_overlay = true;
  gfx::ColorSpace color_space = GetImageBufferColorSpace(image.get());
  if (!color_space.IsValid()) {
    // Chrome and macOS do not agree on the color space; force compositing to
    // ensure a consistent result. See crbug.com/343014700.
    allow_overlay = false;
    // Always use limited range since we request a limited range output format.
    color_space = metadata->color_space.GetWithMatrixAndRange(
        metadata->color_space.GetMatrixID(), gfx::ColorSpace::RangeID::LIMITED);
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.id = gfx::GpuMemoryBufferHandle::kInvalidId;
  handle.type = gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER;
  handle.io_surface.reset(CVPixelBufferGetIOSurface(image.get()),
                          base::scoped_policy::RETAIN);

  OSType pixel_format = IOSurfaceGetPixelFormat(handle.io_surface.get());
  std::optional<viz::SharedImageFormat> format =
      PixelFormatToImageFormat(pixel_format);
  if (!format) {
    MEDIA_LOG(ERROR, media_log_.get())
        << "Unknown pixel format " << pixel_format;
    std::move(output_cb).Run(nullptr, std::move(metadata));
    return;
  }

  VideoPixelFormat video_pixel_format =
      PixelFormatToVideoPixelFormat(pixel_format);

  auto shared_image_interface = sis_->shared_image_interface();
  CHECK(shared_image_interface);

  // Extract IOSurface webgpu compatible attribute before image is moved.
  const bool is_webgpu_compatible =
      IOSurfaceIsWebGPUCompatible(CVPixelBufferGetIOSurface(image.get()));
  gpu::SharedImageUsageSet shared_image_usage = kSharedImageUsage;
  if (is_webgpu_compatible &&
      base::FeatureList::IsEnabled(
          kVideoToolboxFrameConverterSpecifyWebGpuUsage)) {
    shared_image_usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
  }

  auto shared_image = shared_image_interface->CreateSharedImage(
      {*format, coded_size, metadata->color_space, kTopLeft_GrSurfaceOrigin,
       kOpaque_SkAlphaType, shared_image_usage, kSharedImageDebugLabel},
      std::move(handle));
  if (!shared_image) {
    MEDIA_LOG(ERROR, media_log_.get()) << "Failed to create shared image";
    std::move(output_cb).Run(nullptr, std::move(metadata));
    return;
  }


  // |image| must be retained until after the release sync token passes.
  VideoFrame::ReleaseMailboxCB release_cb = base::BindPostTask(
      gpu_task_runner_,
      base::BindOnce(&VideoToolboxFrameConverter::OnVideoFrameReleased, this,
                     shared_image, std::move(image)));

  // It should be possible to use VideoFrame::WrapExternalGpuMemoryBuffer(),
  // which would allow the renderer to map the IOSurface, but this is more
  // expensive whenever the renderer is not doing readback.
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapSharedImage(
      video_pixel_format, shared_image, shared_image->creation_sync_token(),
      std::move(release_cb), coded_size, visible_rect, natural_size,
      metadata->timestamp);

  if (!frame) {
    MEDIA_LOG(ERROR, media_log_.get()) << "Failed to create VideoFrame";
    std::move(output_cb).Run(nullptr, std::move(metadata));
    return;
  }

  frame->set_color_space(color_space);
  frame->set_hdr_metadata(metadata->hdr_metadata);
  frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);
  if (metadata->duration != kNoTimestamp && !metadata->duration.is_zero()) {
    frame->metadata().frame_duration = metadata->duration;
  }
  frame->metadata().allow_overlay = allow_overlay;
  // Releasing |image| must happen after command buffer commands are complete
  // (not just submitted).
  frame->metadata().read_lock_fences_enabled = true;
  frame->metadata().is_webgpu_compatible = is_webgpu_compatible;
  // TODO(crbug.com/40227557): VideoToolbox can report software usage, should
  // we plumb that through?
  frame->metadata().power_efficient = true;

  std::move(output_cb).Run(std::move(frame), std::move(metadata));
}

void VideoToolboxFrameConverter::OnVideoFrameReleased(
    scoped_refptr<gpu::ClientSharedImage> client_shared_image,
    base::apple::ScopedCFTypeRef<CVImageBufferRef> image,
    const gpu::SyncToken& sync_token) {
  DVLOG(4) << __func__;
  DCHECK(gpu_task_runner_->RunsTasksInCurrentSequence());

  if (!stub_) {
    // Release |image| immediately.
    return;
  }

  if (client_shared_image) {
    client_shared_image->UpdateDestructionSyncToken(sync_token);
  }

  // Release |image|.
  stub_->channel()->scheduler()->ScheduleTask(gpu::Scheduler::Task(
      wait_sequence_id_, base::DoNothingWithBoundArgs(std::move(image)),
      std::vector<gpu::SyncToken>({sync_token})));
}

}  // namespace media
