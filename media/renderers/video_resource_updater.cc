// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/renderers/video_resource_updater.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/renderers/resource_sync_token_client.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/khronos/GLES3/gl3.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/trace_util.h"

namespace media {
namespace {

// Generates process-unique IDs to use for tracing video resources.
base::AtomicSequenceNumber g_next_video_resource_updater_id;

gfx::ProtectedVideoType ProtectedVideoTypeFromMetadata(
    const VideoFrameMetadata& metadata) {
  // DisplayCompositor doesn't have access to contents of the VideoFrame in this
  // case,
  if (metadata.dcomp_surface) {
    return gfx::ProtectedVideoType::kHardwareProtected;
  }

  if (!metadata.protected_video) {
    return gfx::ProtectedVideoType::kClear;
  }

  return metadata.hw_protected ? gfx::ProtectedVideoType::kHardwareProtected
                               : gfx::ProtectedVideoType::kSoftwareProtected;
}

VideoFrameResourceType ExternalResourceTypeForHardware(const VideoFrame& frame,
                                                       GLuint target) {
  bool si_prefers_external_sampler =
      frame.shared_image()->format().PrefersExternalSampler();
  if (si_prefers_external_sampler) {
    return VideoFrameResourceType::RGB;
  }

  const VideoPixelFormat format = frame.format();
  switch (format) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_BGRA:
      switch (target) {
        case GL_TEXTURE_EXTERNAL_OES:
#if BUILDFLAG(IS_ANDROID)
          return VideoFrameResourceType::RGB;
#endif
        case GL_TEXTURE_2D:
        case GL_TEXTURE_RECTANGLE_ARB:
          return (format == PIXEL_FORMAT_XRGB)
                     ? VideoFrameResourceType::RGB
                     : VideoFrameResourceType::RGBA_PREMULTIPLIED;
        default:
          NOTREACHED();
      }
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
    case PIXEL_FORMAT_RGBAF16:
      return VideoFrameResourceType::RGB;

    case PIXEL_FORMAT_UYVY:
      NOTREACHED();
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_YUV444AP10:
    case PIXEL_FORMAT_UNKNOWN:
      break;
  }
  return VideoFrameResourceType::NONE;
}

viz::SharedImageFormat GetRGBSharedImageFormat(VideoPixelFormat format) {
#if BUILDFLAG(IS_MAC)
  // macOS IOSurfaces are always BGRA_8888.
  return PaintCanvasVideoRenderer::GetRGBPixelsOutputFormat();
#else
  // viz::SinglePlaneFormat::kRGBX_8888 and viz::SinglePlaneFormat::kBGRX_8888
  // require upload as GL_RGB (3 bytes), while VideoFrame is always four bytes,
  // so we can't upload directly from them.
  switch (format) {
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_ABGR:
      return viz::SinglePlaneFormat::kRGBA_8888;
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ARGB:
      return viz::SinglePlaneFormat::kBGRA_8888;
    default:
      NOTREACHED();
  }
#endif
}

// Returns true if the input VideoFrame format can be stored directly in the
// provided output shared image format.
bool HasCompatibleRGBFormat(VideoPixelFormat input_format,
                            viz::SharedImageFormat output_format) {
  if (input_format == PIXEL_FORMAT_XBGR)
    return output_format == viz::SinglePlaneFormat::kRGBA_8888 ||
           output_format == viz::SinglePlaneFormat::kRGBX_8888;
  if (input_format == PIXEL_FORMAT_ABGR)
    return output_format == viz::SinglePlaneFormat::kRGBA_8888;
  if (input_format == PIXEL_FORMAT_XRGB)
    return output_format == viz::SinglePlaneFormat::kBGRA_8888 ||
           output_format == viz::SinglePlaneFormat::kBGRX_8888;
  if (input_format == PIXEL_FORMAT_ARGB)
    return output_format == viz::SinglePlaneFormat::kBGRA_8888;
  return false;
}

bool IsFrameFormat32BitRGB(VideoPixelFormat frame_format) {
  return frame_format == PIXEL_FORMAT_XBGR ||
         frame_format == PIXEL_FORMAT_XRGB ||
         frame_format == PIXEL_FORMAT_ABGR || frame_format == PIXEL_FORMAT_ARGB;
}

viz::SharedImageFormat::ChannelFormat SupportedMultiPlaneChannelFormat(
    const gpu::Capabilities& caps,
    const gpu::SharedImageCapabilities& shared_image_caps,
    int bits_per_channel) {
  if (bits_per_channel <= 8) {
    // Must support texture_rg or 8-bits luminance.
    DCHECK(shared_image_caps.supports_luminance_shared_images ||
           caps.texture_rg);
    return viz::SharedImageFormat::ChannelFormat::k8;
  }
  // Can support R_16 formats.
  if (caps.texture_norm16 && shared_image_caps.supports_r16_shared_images) {
    return viz::SharedImageFormat::ChannelFormat::k16;
  }
  // Can support R_F16 or LUMINANCE_F16 formats.
  if (shared_image_caps.is_r16f_supported ||
      (caps.texture_half_float_linear &&
       shared_image_caps.supports_luminance_shared_images)) {
    return viz::SharedImageFormat::ChannelFormat::k16F;
  }
  return viz::SharedImageFormat::ChannelFormat::k8;
}

// Return multiplanar shared image format corresponding to the VideoPixelFormat.
viz::SharedImageFormat VideoPixelFormatToMultiPlanarSharedImageFormat(
    VideoPixelFormat input_format) {
  using PlaneConfig = viz::SharedImageFormat::PlaneConfig;
  using Subsampling = viz::SharedImageFormat::Subsampling;
  using ChannelFormat = viz::SharedImageFormat::ChannelFormat;
  // Supports VideoPixelFormats based on data from
  // Media.GpuMemoryBufferVideoFramePool.UnsupportedFormat UMA which ends up
  // going through VideoResourceUpdater for software pixel upload.
  switch (input_format) {
    case PIXEL_FORMAT_I420:
      return viz::MultiPlaneFormat::kI420;
    case PIXEL_FORMAT_YV12:
      return viz::MultiPlaneFormat::kYV12;
    case PIXEL_FORMAT_I422:
      return viz::SharedImageFormat::MultiPlane(
          PlaneConfig::kY_U_V, Subsampling::k422, ChannelFormat::k8);
    case PIXEL_FORMAT_I444:
      return viz::SharedImageFormat::MultiPlane(
          PlaneConfig::kY_U_V, Subsampling::k444, ChannelFormat::k8);
    case PIXEL_FORMAT_NV12:
      return viz::MultiPlaneFormat::kNV12;
    case PIXEL_FORMAT_YUV420P10:
      return viz::SharedImageFormat::MultiPlane(
          PlaneConfig::kY_U_V, Subsampling::k420, ChannelFormat::k10);
    case PIXEL_FORMAT_YUV422P10:
      return viz::SharedImageFormat::MultiPlane(
          PlaneConfig::kY_U_V, Subsampling::k422, ChannelFormat::k10);
    case PIXEL_FORMAT_YUV444P10:
      return viz::SharedImageFormat::MultiPlane(
          PlaneConfig::kY_U_V, Subsampling::k444, ChannelFormat::k10);
    case PIXEL_FORMAT_YUV420P12:
      return viz::SharedImageFormat::MultiPlane(
          PlaneConfig::kY_U_V, Subsampling::k420, ChannelFormat::k16);
    case PIXEL_FORMAT_YUV422P12:
      return viz::SharedImageFormat::MultiPlane(
          PlaneConfig::kY_U_V, Subsampling::k422, ChannelFormat::k16);
    case PIXEL_FORMAT_YUV444P12:
      return viz::SharedImageFormat::MultiPlane(
          PlaneConfig::kY_U_V, Subsampling::k444, ChannelFormat::k16);
    case PIXEL_FORMAT_NV12A:
      return viz::MultiPlaneFormat::kNV12A;
    case PIXEL_FORMAT_I420A:
      return viz::MultiPlaneFormat::kI420A;
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_RGBAF16:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_YUV444AP10:
    case PIXEL_FORMAT_UNKNOWN:
      NOTREACHED();
  }
}

std::vector<VideoFrame::Plane> GetVideoFramePlanes(
    viz::SharedImageFormat format) {
  CHECK(format.is_multi_plane());
  switch (format.plane_config()) {
    case viz::SharedImageFormat::PlaneConfig::kY_U_V:
      return {VideoFrame::Plane::kY, VideoFrame::Plane::kU,
              VideoFrame::Plane::kV};
    case viz::SharedImageFormat::PlaneConfig::kY_V_U:
      return {VideoFrame::Plane::kY, VideoFrame::Plane::kV,
              VideoFrame::Plane::kU};
    case viz::SharedImageFormat::PlaneConfig::kY_UV:
      return {VideoFrame::Plane::kY, VideoFrame::Plane::kUV};
    case viz::SharedImageFormat::PlaneConfig::kY_UV_A:
      return {VideoFrame::Plane::kY, VideoFrame::Plane::kUV,
              VideoFrame::Plane::kATriPlanar};
    case viz::SharedImageFormat::PlaneConfig::kY_U_V_A:
      return {VideoFrame::Plane::kY, VideoFrame::Plane::kU,
              VideoFrame::Plane::kV, VideoFrame::Plane::kA};
  }
  NOTREACHED();
}

class CopyingSyncTokenClient : public VideoFrame::SyncTokenClient {
 public:
  CopyingSyncTokenClient() = default;
  CopyingSyncTokenClient(const CopyingSyncTokenClient&) = delete;
  CopyingSyncTokenClient& operator=(const CopyingSyncTokenClient&) = delete;

  ~CopyingSyncTokenClient() override = default;

  void GenerateSyncToken(gpu::SyncToken* sync_token) override {
    *sync_token = sync_token_;
  }

  void WaitSyncToken(const gpu::SyncToken& sync_token) override {
    sync_token_ = sync_token;
  }

 private:
  gpu::SyncToken sync_token_;
};

}  // namespace

VideoFrameExternalResource::VideoFrameExternalResource() = default;
VideoFrameExternalResource::~VideoFrameExternalResource() = default;

VideoFrameExternalResource::VideoFrameExternalResource(
    VideoFrameExternalResource&& other) = default;
VideoFrameExternalResource& VideoFrameExternalResource::operator=(
    VideoFrameExternalResource&& other) = default;

// Resource for a video frame allocated and owned by VideoResourceUpdater. This
// will be reused when possible.
class VideoResourceUpdater::FrameResource {
 public:
  // For software frame resource that creates a mapping.
  FrameResource(uint32_t frame_resource_id,
                const gfx::Size& size,
                const gfx::ColorSpace& color_space,
                gpu::SharedImageInterface* shared_image_interface)
      : id_(frame_resource_id), is_software_(true) {
    DCHECK(shared_image_interface);
    shared_image_ =
        shared_image_interface->CreateSharedImageForSoftwareCompositor(
            {viz::SinglePlaneFormat::kBGRA_8888, size, color_space,
             gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY, "VideoResourceUpdater"});
    mapping_ = shared_image_->Map();
    sync_token_ = shared_image_interface->GenVerifiedSyncToken();
  }

  // For hardware frame resource.
  FrameResource(uint32_t frame_resource_id,
                const gfx::Size& size,
                viz::SharedImageFormat format,
                const gfx::ColorSpace& color_space,
                bool use_gpu_memory_buffer_resources,
                gpu::SharedImageInterface* shared_image_interface)
      : id_(frame_resource_id), is_software_(false) {
    DCHECK(shared_image_interface);
    // TODO(crbug.com/40239769): Set `overlay_candidate` for multiplanar
    // formats.
    const bool overlay_candidate =
        format.is_single_plane() && use_gpu_memory_buffer_resources &&
        shared_image_interface->GetCapabilities()
            .supports_scanout_shared_images &&
        CanCreateGpuMemoryBufferForSinglePlaneSharedImageFormat(format);

    // These SharedImages will be sent over to the display compositor as
    // TransferableResources. RasterInterface which in turn uses RasterDecoder
    // writes the contents of video frames into SharedImages.
    gpu::SharedImageUsageSet shared_image_usage =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
        gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
    if (overlay_candidate) {
      shared_image_usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    }
    shared_image_ = shared_image_interface->CreateSharedImage(
        {format, size, color_space, shared_image_usage, "VideoResourceUpdater"},
        gpu::kNullSurfaceHandle);
    CHECK(shared_image_);
    sync_token_ = shared_image_->creation_sync_token();
  }

  FrameResource(const FrameResource&) = delete;
  FrameResource& operator=(const FrameResource&) = delete;

  ~FrameResource() {
    DCHECK(shared_image_);
    shared_image_->UpdateDestructionSyncToken(sync_token_);
  }

  bool Equals(const gfx::Size& size,
              viz::SharedImageFormat format,
              const gfx::ColorSpace& color_space) {
    return size == shared_image_->size() && format == shared_image_->format() &&
           color_space == shared_image_->color_space();
  }

  // Returns true if this resource matches the unique identifiers of another
  // VideoFrame resource.
  bool Matches(VideoFrame::ID unique_frame_id) {
    CHECK(!unique_frame_id.is_null());
    return unique_frame_id_ == unique_frame_id;
  }

  // Sets the unique identifiers for this resource, may only be called when
  // there is a single reference to the resource (i.e. |ref_count_| == 1).
  void SetUniqueId(VideoFrame::ID unique_frame_id) {
    DCHECK_EQ(ref_count_, 1);
    unique_frame_id_ = unique_frame_id;
  }

  void UpdateSyncToken(const gpu::SyncToken& sync_token) {
    sync_token_ = sync_token;
  }

  // Only called for a software resource.
  SkPixmap pixmap() {
    CHECK(is_software());
    return mapping_->GetSkPixmapForPlane(
        /*plane_index=*/0,
        SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(size())));
  }

  // Accessors for resource identifiers provided at construction time.
  uint32_t id() const { return id_; }
  gfx::Size size() const { return shared_image_->size(); }
  viz::SharedImageFormat format() const { return shared_image_->format(); }
  bool is_software() const { return is_software_; }
  const scoped_refptr<gpu::ClientSharedImage>& shared_image() const {
    return shared_image_;
  }
  const gpu::SyncToken& sync_token() { return sync_token_; }

  // Various methods for managing references. See |ref_count_| for details.
  void add_ref() { ++ref_count_; }
  void remove_ref() { --ref_count_; }
  bool has_refs() const { return ref_count_ != 0; }

 private:
  const uint32_t id_;
  const bool is_software_;

  // The number of times this resource has been imported vs number of times this
  // resource has returned.
  int ref_count_ = 0;

  // Identifies the data stored in this resource; uniquely identify a
  // VideoFrame.
  VideoFrame::ID unique_frame_id_;

  // Used for SharedImage.
  gpu::SyncToken sync_token_;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;

  // Used for software frame resource.
  std::unique_ptr<gpu::ClientSharedImage::ScopedMapping> mapping_;
};

VideoResourceUpdater::VideoResourceUpdater(
    viz::RasterContextProvider* context_provider,
    viz::ClientResourceProvider* resource_provider,
    scoped_refptr<gpu::SharedImageInterface> shared_image_interface,
    bool use_gpu_memory_buffer_resources,
    int max_resource_size)
    : context_provider_(context_provider),
      shared_image_interface_(std::move(shared_image_interface)),
      resource_provider_(resource_provider),
      use_gpu_memory_buffer_resources_(use_gpu_memory_buffer_resources),
      max_resource_size_(max_resource_size),
      tracing_id_(g_next_video_resource_updater_id.GetNext()) {
  DCHECK(context_provider_ || shared_image_interface_);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "media::VideoResourceUpdater",
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

VideoResourceUpdater::~VideoResourceUpdater() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void VideoResourceUpdater::ObtainFrameResource(
    scoped_refptr<VideoFrame> video_frame) {
  UMA_HISTOGRAM_ENUMERATION("Media.VideoResourceUpdater.FrameFormat",
                            video_frame->format(), PIXEL_FORMAT_MAX + 1);

  if (video_frame->storage_type() == VideoFrame::STORAGE_OPAQUE &&
      video_frame->format() == VideoPixelFormat::PIXEL_FORMAT_UNKNOWN &&
      video_frame->metadata().tracking_token.has_value()) {
    // This is a hole punching VideoFrame, there is nothing to display.
    overlay_plane_id_ = *video_frame->metadata().tracking_token;
    frame_resource_type_ = VideoFrameResourceType::VIDEO_HOLE;
    return;
  }

  VideoFrameExternalResource external_resource =
      CreateExternalResourceFromVideoFrame(video_frame);
  frame_resource_type_ = external_resource.type;

  if (external_resource.resource.is_empty()) {
    DLOG(ERROR) << "external_resource is empty.";
    frame_resource_id_ = viz::kInvalidResourceId;
    return;
  }

  frame_resource_id_ = resource_provider_->ImportResource(
      external_resource.resource,
      std::move(external_resource.release_callback));
  TRACE_EVENT_INSTANT1("media", "VideoResourceUpdater::ObtainFrameResource",
                       TRACE_EVENT_SCOPE_THREAD, "Timestamp",
                       video_frame->timestamp().InMicroseconds());
}

void VideoResourceUpdater::ReleaseFrameResource() {
  if (!frame_resource_id_.is_null()) {
    resource_provider_->RemoveImportedResource(frame_resource_id_);
  }
  frame_resource_id_ = viz::ResourceId();
}

void VideoResourceUpdater::AppendQuad(
    viz::CompositorRenderPass* render_pass,
    scoped_refptr<VideoFrame> frame,
    gfx::Transform transform,
    gfx::Rect quad_rect,
    gfx::Rect visible_quad_rect,
    const gfx::MaskFilterInfo& mask_filter_info,
    std::optional<gfx::Rect> clip_rect,
    bool contents_opaque,
    float draw_opacity,
    int sorting_context_id) {
  DCHECK(frame.get());

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(
      transform, quad_rect, visible_quad_rect, mask_filter_info, clip_rect,
      contents_opaque, draw_opacity, SkBlendMode::kSrcOver, sorting_context_id,
      /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  bool needs_blending = !contents_opaque;

  gfx::Rect visible_rect = frame->visible_rect();
  gfx::Size coded_size = frame->coded_size();

  const gfx::PointF uv_top_left(
      static_cast<float>(visible_rect.x()) / coded_size.width(),
      static_cast<float>(visible_rect.y()) / coded_size.height());

  const gfx::PointF uv_bottom_right(
      static_cast<float>(visible_rect.right()) / coded_size.width(),
      static_cast<float>(visible_rect.bottom()) / coded_size.height());

  switch (frame_resource_type_) {
    case VideoFrameResourceType::VIDEO_HOLE: {
      auto* video_hole_quad =
          render_pass->CreateAndAppendDrawQuad<viz::VideoHoleDrawQuad>();
      video_hole_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                              overlay_plane_id_);
      break;
    }
    case VideoFrameResourceType::RGBA_PREMULTIPLIED:
    case VideoFrameResourceType::RGB: {
      if (frame_resource_id_.is_null()) {
        break;
      }

      bool nearest_neighbor = false;
      gfx::ProtectedVideoType protected_video_type =
          ProtectedVideoTypeFromMetadata(frame->metadata());
      auto* texture_quad =
          render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
      texture_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                           needs_blending, frame_resource_id_, uv_top_left,
                           uv_bottom_right, SkColors::kTransparent,
                           nearest_neighbor, false, protected_video_type);
#if BUILDFLAG(IS_WIN)
      // Windows uses DComp surfaces to e.g. hold MediaFoundation videos, which
      // must be promoted to overlay to be composited correctly.
      if (frame->metadata().dcomp_surface) {
        texture_quad->overlay_priority_hint = viz::OverlayPriority::kRequired;
      }
#endif
      texture_quad->is_video_frame = true;
      resource_provider_->ValidateResource(texture_quad->resource_id);
      break;
    }
    case VideoFrameResourceType::NONE:
      NOTIMPLEMENTED();
      break;
  }
}

void VideoResourceUpdater::ClearFrameResources() {
  // Delete recycled resources that are not in use anymore.
  std::erase_if(all_resources_,
                [](const std::unique_ptr<FrameResource>& resource) {
                  // Resources that are still being used can't be deleted.
                  return !resource->has_refs();
                });

  // May have destroyed resources above that contains shared images.
  // ClientSharedImage destructor calls DestroySharedImage which in turn ensures
  // that the deferred destroy request from above is flushed. Thus,
  // SharedImageInterface::Flush in not needed here explicitly.
}

VideoFrameExternalResource
VideoResourceUpdater::CreateExternalResourceFromVideoFrame(
    scoped_refptr<VideoFrame> video_frame) {
  if (video_frame->format() == PIXEL_FORMAT_UNKNOWN)
    return VideoFrameExternalResource();
  DCHECK(video_frame->HasSharedImage() || video_frame->IsMappable());
  if (video_frame->HasSharedImage()) {
    return CreateForHardwareFrame(std::move(video_frame));
  } else {
    return CreateForSoftwareFrame(std::move(video_frame));
  }
}

bool VideoResourceUpdater::ReallocateUploadPixels(size_t needed_size,
                                                  size_t plane) {
  // Free the existing data first so that the memory can be reused, if
  // possible. Note that the new array is purposely not initialized.
  upload_pixels_[plane].reset();
  uint8_t* pixel_mem = nullptr;
  // Fail if we can't support the required memory to upload pixels.
  if (!base::UncheckedMalloc(needed_size,
                             reinterpret_cast<void**>(&pixel_mem))) {
    DLOG(ERROR) << "Unable to allocate enough memory required to "
                   "upload pixels";
    return false;
  }
  upload_pixels_[plane].reset(pixel_mem);
  upload_pixels_size_[plane] = needed_size;
  return true;
}

VideoResourceUpdater::FrameResource*
VideoResourceUpdater::RecycleOrAllocateResource(
    const gfx::Size& resource_size,
    viz::SharedImageFormat si_format,
    const gfx::ColorSpace& color_space,
    VideoFrame::ID unique_id) {
  FrameResource* recyclable_resource = nullptr;
  for (auto& resource : all_resources_) {
    // If the unique id is valid then we are allowed to return a referenced
    // resource that already contains the right frame data. It's safe to reuse
    // it even if resource_provider_ holds some references to it, because those
    // references are read-only.
    if (!unique_id.is_null() && resource->Matches(unique_id)) {
      DCHECK(resource->Equals(resource_size, si_format, color_space));
      return resource.get();
    }

    // Otherwise check whether this is an unreferenced resource of the right
    // format that we can recycle. Remember it, but don't return immediately,
    // because we still want to find any reusable resources.
    const bool in_use = resource->has_refs();

    if (!in_use && resource->Equals(resource_size, si_format, color_space)) {
      recyclable_resource = resource.get();
    }
  }

  if (recyclable_resource) {
    return recyclable_resource;
  }

  // There was nothing available to reuse or recycle. Allocate a new resource.
  return AllocateResource(resource_size, si_format, color_space);
}

VideoResourceUpdater::FrameResource* VideoResourceUpdater::AllocateResource(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    const gfx::ColorSpace& color_space) {
  const uint32_t resource_id = next_plane_resource_id_++;

  if (software_compositor()) {
    DCHECK_EQ(format, viz::SinglePlaneFormat::kBGRA_8888);
    all_resources_.push_back(std::make_unique<FrameResource>(
        resource_id, size, color_space, shared_image_interface()));
  } else {
    all_resources_.push_back(std::make_unique<FrameResource>(
        resource_id, size, format, color_space,
        use_gpu_memory_buffer_resources_,
        context_provider_->SharedImageInterface()));
  }
  return all_resources_.back().get();
}

void VideoResourceUpdater::CopyHardwareResource(
    VideoFrame* video_frame,
    VideoFrameExternalResource* external_resource) {
  const gfx::Size output_resource_size = video_frame->coded_size();
  auto shared_image = video_frame->shared_image();
  // The copy needs to be a direct transfer of pixel data, so we use an RGBA8
  // target to avoid loss of precision or dropping any alpha component.
  constexpr viz::SharedImageFormat copy_si_format =
      viz::SinglePlaneFormat::kRGBA_8888;

  // We copy to RGBA image, so we need only RGBA portion of the color space.
  const auto copy_color_space = video_frame->ColorSpace().GetAsFullRangeRGB();

  const VideoFrame::ID no_unique_id;  // Do not recycle referenced textures.
  FrameResource* hardware_resource = RecycleOrAllocateResource(
      output_resource_size, copy_si_format, copy_color_space, no_unique_id);
  CHECK(!hardware_resource->is_software());
  hardware_resource->add_ref();

  auto* ri = RasterInterface();
  // Wait on sync tokens for both source (video frame) and destination (shared
  // image).
  std::unique_ptr<gpu::RasterScopedAccess> src_ri_access =
      shared_image->BeginRasterAccess(ri, video_frame->acquire_sync_token(),
                                      /*readonly=*/true);
  std::unique_ptr<gpu::RasterScopedAccess> dst_ri_access =
      hardware_resource->shared_image()->BeginRasterAccess(
          ri, hardware_resource->sync_token(),
          /*readonly=*/false);

  ri->CopySharedImage(
      shared_image->mailbox(), hardware_resource->shared_image()->mailbox(),
      /*xoffset=*/0, /*yoffset=*/0, /*x=*/0, /*y=*/0,
      output_resource_size.width(), output_resource_size.height());

  // Wait (if the existing token isn't null) and replace it with a new one.
  WaitAndReplaceSyncTokenClient client(ri, std::move(src_ri_access));
  gpu::SyncToken sync_token = video_frame->UpdateReleaseSyncToken(&client);
  hardware_resource->UpdateSyncToken(sync_token);
  gpu::RasterScopedAccess::EndAccess(std::move(dst_ri_access));

  SkAlphaType alpha_type =
      (external_resource->type == VideoFrameResourceType::RGBA_PREMULTIPLIED)
          ? kPremul_SkAlphaType
          : kUnpremul_SkAlphaType;
  viz::TransferableResource::MetadataOverride overrides = {
      .is_overlay_candidate = false,
      .alpha_type = alpha_type,
  };
  auto transferable_resource = viz::TransferableResource::Make(
      hardware_resource->shared_image(),
      viz::TransferableResource::ResourceSource::kVideo,
      hardware_resource->sync_token(), overrides);

  transferable_resource.hdr_metadata =
      video_frame->hdr_metadata().value_or(gfx::HDRMetadata());
  transferable_resource.needs_detiling = video_frame->metadata().needs_detiling;

  external_resource->resource = std::move(transferable_resource);
  external_resource->release_callback =
      base::BindOnce(&VideoResourceUpdater::RecycleResource,
                     weak_ptr_factory_.GetWeakPtr(), hardware_resource->id());
}

VideoFrameExternalResource VideoResourceUpdater::CreateForHardwareFrame(
    scoped_refptr<VideoFrame> video_frame) {
  TRACE_EVENT0("media", "VideoResourceUpdater::CreateForHardwareFrame");
  if (!context_provider_) {
    return VideoFrameExternalResource();
  }

  VideoFrameExternalResource external_resource;
  const bool copy_required = video_frame->metadata().copy_required;
  auto shared_image = video_frame->shared_image();
  GLuint target = shared_image->GetTextureTarget();
  // If |copy_required| then we will copy into a GL_TEXTURE_2D target.
  if (copy_required) {
    target = GL_TEXTURE_2D;
  }

  external_resource.type =
      ExternalResourceTypeForHardware(*video_frame, target);
  if (external_resource.type == VideoFrameResourceType::NONE) {
    DLOG(ERROR) << "Unsupported Texture format"
                << VideoPixelFormatToString(video_frame->format());
    return external_resource;
  }

  // Make a copy of the current release SyncToken so we know if it changes.
  CopyingSyncTokenClient client;
  auto original_release_token = video_frame->UpdateReleaseSyncToken(&client);

  if (copy_required) {
    CopyHardwareResource(video_frame.get(), &external_resource);
    return external_resource;
  }

  SkAlphaType alpha_type =
      (external_resource.type == VideoFrameResourceType::RGBA_PREMULTIPLIED)
          ? kPremul_SkAlphaType
          : kUnpremul_SkAlphaType;

  viz::TransferableResource::MetadataOverride overrides = {
      .size = video_frame->coded_size(),
      .is_overlay_candidate = video_frame->metadata().allow_overlay,
      .color_space = video_frame->ColorSpace(),
      .alpha_type = alpha_type,
  };
  auto transfer_resource = viz::TransferableResource::Make(
      shared_image, viz::TransferableResource::ResourceSource::kVideo,
      video_frame->acquire_sync_token(), overrides);
  transfer_resource.hdr_metadata =
      video_frame->hdr_metadata().value_or(gfx::HDRMetadata());
  transfer_resource.needs_detiling = video_frame->metadata().needs_detiling;
  if (video_frame->metadata().read_lock_fences_enabled) {
    transfer_resource.synchronization_type =
        viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted;
  }
  transfer_resource.ycbcr_info = video_frame->ycbcr_info();

#if BUILDFLAG(IS_ANDROID)
  transfer_resource.is_backed_by_surface_view =
      video_frame->metadata().in_surface_view;
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
  transfer_resource.wants_promotion_hint =
      video_frame->metadata().wants_promotion_hint;
#endif

  external_resource.resource = std::move(transfer_resource);
  external_resource.release_callback = base::BindOnce(
      &VideoResourceUpdater::ReturnTexture, weak_ptr_factory_.GetWeakPtr(),
      video_frame, original_release_token);
  return external_resource;
}

viz::SharedImageFormat VideoResourceUpdater::GetSoftwareOutputFormat(
    VideoPixelFormat input_frame_format,
    int bits_per_channel) {
  if (software_compositor()) {
    return viz::SinglePlaneFormat::kBGRA_8888;
  }
  if (IsFrameFormat32BitRGB(input_frame_format)) {
    return GetRGBSharedImageFormat(input_frame_format);
  }

  if (input_frame_format == PIXEL_FORMAT_Y16) {
    // Unable to display directly as yuv planes so convert it to RGB.
    return PaintCanvasVideoRenderer::GetRGBPixelsOutputFormat();
  }
  const auto& caps = context_provider_->ContextCapabilities();
  if (caps.disable_one_component_textures) {
    // If GPU compositing is enabled, we need to convert texture to RGB if one
    // component textures are disabled.
    return PaintCanvasVideoRenderer::GetRGBPixelsOutputFormat();
  }

  const auto& shared_image_caps =
      context_provider_->SharedImageInterface()->GetCapabilities();
  // Get the multiplanar shared image format for `input_frame_format`.
  auto yuv_si_format =
      VideoPixelFormatToMultiPlanarSharedImageFormat(input_frame_format);
  if (yuv_si_format.plane_config() ==
      viz::SharedImageFormat::PlaneConfig::kY_UV) {
    // Only 8-bit formats are supported with UV planes for software decoding.
    CHECK_EQ(yuv_si_format.channel_format(),
             viz::SharedImageFormat::ChannelFormat::k8);
    // Two channel formats are supported only with texture_rg.
    if (!caps.texture_rg || shared_image_caps.disable_r8_shared_images) {
      return PaintCanvasVideoRenderer::GetRGBPixelsOutputFormat();
    }
  }

  // Get the supported channel format for `yuv_si_format`'s first plane.
  auto channel_format = SupportedMultiPlaneChannelFormat(
      caps, shared_image_caps, bits_per_channel);
  if (yuv_si_format.channel_format() != channel_format) {
    // If the requested channel format is not supported, use the supported
    // channel format and downsample later if needed.
    yuv_si_format = viz::SharedImageFormat::MultiPlane(
        yuv_si_format.plane_config(), yuv_si_format.subsampling(),
        channel_format);
  }
  return yuv_si_format;
}

void VideoResourceUpdater::TransferRGBPixelsToPaintCanvas(
    scoped_refptr<VideoFrame> video_frame,
    FrameResource* software_resource) {
  if (!video_renderer_) {
    video_renderer_ = std::make_unique<PaintCanvasVideoRenderer>();
  }

  CHECK(software_resource->is_software());
  DCHECK_EQ(software_resource->format(), viz::SinglePlaneFormat::kBGRA_8888);
  SkBitmap sk_bitmap;
  sk_bitmap.installPixels(software_resource->pixmap());
  // This is software path, so |canvas| and |video_frame| are always
  // backed by software.
  cc::SkiaPaintCanvas canvas(sk_bitmap);
  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setFilterQuality(cc::PaintFlags::FilterQuality::kLow);

  // Note that the default PaintCanvasVideoRenderer::PaintParams would copy to
  // the origin, not `video_frame->visible_rect`.
  // https://crbug.com/1090435
  PaintCanvasVideoRenderer::PaintParams paint_params;
  paint_params.dest_rect = gfx::RectF(video_frame->visible_rect());
  video_renderer_->Paint(video_frame, &canvas, flags, paint_params, nullptr);
}

bool VideoResourceUpdater::WriteRGBPixelsToTexture(
    scoped_refptr<VideoFrame> video_frame,
    FrameResource* hardware_resource) {
  CHECK(!hardware_resource->is_software());
  viz::SharedImageFormat resource_format = hardware_resource->format();
  size_t bytes_per_row = viz::ResourceSizes::CheckedWidthInBytes<size_t>(
      video_frame->coded_size().width(), resource_format);
  // Note: Strides may be negative in case of bottom-up layouts.
  const int stride = video_frame->stride(VideoFrame::Plane::kARGB);
  const bool has_compatible_stride =
      stride > 0 && static_cast<size_t>(stride) == bytes_per_row;

  const uint8_t* source_pixels = nullptr;
  if (HasCompatibleRGBFormat(video_frame->format(), resource_format) &&
      has_compatible_stride) {
    // We can passthrough when the texture format matches. Since we
    // always copy the entire coded area we don't have to worry about
    // origin.
    source_pixels = video_frame->data(VideoFrame::Plane::kARGB);
  } else {
    size_t needed_size = bytes_per_row * video_frame->coded_size().height();
    if (upload_pixels_size_[0] < needed_size) {
      if (!ReallocateUploadPixels(needed_size, /*plane=*/0)) {
        // Fail here if memory reallocation fails.
        return false;
      }
    }

    // PCVR writes to origin, so offset upload pixels by start since
    // we upload frames in coded size and pass on the visible rect to
    // the compositor. Note: It'd save a few bytes not to do this...
    auto* dest_ptr = upload_pixels_[0].get() +
                     video_frame->visible_rect().y() * bytes_per_row +
                     video_frame->visible_rect().x() * sizeof(uint32_t);
    PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
        video_frame.get(), dest_ptr, bytes_per_row,
        /*premultiply_alpha=*/false);
    source_pixels = upload_pixels_[0].get();
  }

  // Copy pixels into texture.
  auto* ri = RasterInterface();
  std::unique_ptr<gpu::RasterScopedAccess> ri_access =
      hardware_resource->shared_image()->BeginRasterAccess(
          ri, hardware_resource->sync_token(),
          /*readonly=*/false);

  auto color_type =
      viz::ToClosestSkColorType(resource_format, /*plane_index=*/0);
  auto info = SkImageInfo::Make(gfx::SizeToSkISize(hardware_resource->size()),
                                color_type, kPremul_SkAlphaType);
  SkPixmap pixmap(info, source_pixels, bytes_per_row);
  ri->WritePixels(
      hardware_resource->shared_image()->mailbox(), /*dst_x_offset=*/0,
      /*dst_y_offset=*/0, hardware_resource->shared_image()->GetTextureTarget(),
      pixmap);

  gpu::SyncToken ri_sync_token =
      gpu::RasterScopedAccess::EndAccess(std::move(ri_access));
  hardware_resource->UpdateSyncToken(ri_sync_token);

  return true;
}

bool VideoResourceUpdater::WriteYUVPixelsForAllPlanesToTexture(
    scoped_refptr<VideoFrame> video_frame,
    FrameResource* resource,
    size_t bits_per_channel) {
  // Skip the transfer if this |video_frame|'s plane has been processed.
  if (resource->Matches(video_frame->unique_id())) {
    return true;
  }

  CHECK(!resource->is_software());
  auto yuv_si_format = resource->format();
  std::array<SkPixmap, SkYUVAInfo::kMaxPlanes> pixmaps = {};
  for (int plane_index = 0; plane_index < yuv_si_format.NumberOfPlanes();
       ++plane_index) {
    std::vector<VideoFrame::Plane> frame_planes =
        GetVideoFramePlanes(yuv_si_format);
    // |video_stride_bytes| is the width of the |video_frame| we are
    // uploading (including non-frame data to fill in the stride).
    const int video_stride_bytes =
        video_frame->stride(frame_planes[plane_index]);

    // |resource_size_pixels| is the size of the destination resource.
    const gfx::Size resource_size_pixels =
        yuv_si_format.GetPlaneSize(plane_index, resource->size());

    const size_t plane_size_in_bytes =
        yuv_si_format
            .MaybeEstimatedPlaneSizeInBytes(plane_index, resource->size())
            .value();
    const size_t bytes_per_row = static_cast<size_t>(
        plane_size_in_bytes / resource_size_pixels.height());

    // Use 4-byte row alignment (OpenGL default) for upload performance.
    // Assuming that GL_UNPACK_ALIGNMENT has not changed from default.
    constexpr size_t kDefaultUnpackAlignment = 4;
    const size_t upload_image_stride = cc::MathUtil::CheckedRoundUp<size_t>(
        bytes_per_row, kDefaultUnpackAlignment);

    size_t resource_bit_depth = yuv_si_format.MultiplanarBitDepth();
    if (resource_bit_depth == 10) {
      // Consider 10 bit as 16 for downshifting purposes here.
      resource_bit_depth = 16;
    }
    // Data downshifting is needed if the resource bit depth is not enough.
    const bool needs_bit_downshifting = bits_per_channel > resource_bit_depth;
    // Data upshifting is needed if bits_per_channel is more than 8 i.e. 10/12
    // bit but resource bit depth is higher.
    const bool needs_bit_upshifting =
        bits_per_channel > 8 && bits_per_channel < resource_bit_depth;

    const bool is_16bit_float = yuv_si_format.channel_format() ==
                                viz::SharedImageFormat::ChannelFormat::k16F;

    // We need to convert the incoming data if we're transferring to half
    // float, if there is need for bit downshift or if the strides need to
    // be reconciled.
    const bool needs_conversion =
        is_16bit_float || needs_bit_downshifting || needs_bit_upshifting;
    const uint8_t* pixels;
    int pixels_stride_in_bytes;
    if (!needs_conversion) {
      pixels = video_frame->data(frame_planes[plane_index]);
      pixels_stride_in_bytes = video_stride_bytes;
    } else {
      // Avoid malloc for each frame/plane if possible.
      const size_t needed_size =
          upload_image_stride * resource_size_pixels.height();
      if (upload_pixels_size_[plane_index] < needed_size) {
        if (!ReallocateUploadPixels(needed_size, plane_index)) {
          // Fail here if memory reallocation fails.
          return false;
        }
      }

      if (is_16bit_float) {
        int max_value = 1 << bits_per_channel;
        // Use 1.0/max_value to be consistent with multiplanar shared images
        // which create TextureDrawQuads and don't take in a multiplier, offset.
        // This is consistent with GpuMemoryBufferVideoFramePool as well which
        // performs libyuv conversion for converting I420 to buffer. This is
        // sub-optimal but okay as it is only used for 16-bit float formats with
        // slower software pixel upload path here.
        float libyuv_multiplier = 1.f / max_value;
        libyuv::HalfFloatPlane(
            reinterpret_cast<const uint16_t*>(
                video_frame->data(frame_planes[plane_index])),
            video_stride_bytes,
            reinterpret_cast<uint16_t*>(upload_pixels_[plane_index].get()),
            upload_image_stride, libyuv_multiplier,
            resource_size_pixels.width(), resource_size_pixels.height());
      } else if (needs_bit_downshifting) {
        DCHECK(yuv_si_format.channel_format() ==
               viz::SharedImageFormat::ChannelFormat::k8);
        const int scale = 0x10000 >> (bits_per_channel - 8);
        libyuv::Convert16To8Plane(
            reinterpret_cast<const uint16_t*>(
                video_frame->data(frame_planes[plane_index])),
            video_stride_bytes / 2, upload_pixels_[plane_index].get(),
            upload_image_stride, scale, bytes_per_row,
            resource_size_pixels.height());
      } else if (needs_bit_upshifting) {
        CHECK_EQ(resource_bit_depth, 16u);
        libyuv::ConvertToMSBPlane_16(
            reinterpret_cast<const uint16_t*>(
                video_frame->data(frame_planes[plane_index])),
            video_stride_bytes / 2,
            reinterpret_cast<uint16_t*>(upload_pixels_[plane_index].get()),
            upload_image_stride / 2, resource_size_pixels.width(),
            resource_size_pixels.height(), bits_per_channel);
      } else {
        NOTREACHED();
      }

      pixels = upload_pixels_[plane_index].get();
      pixels_stride_in_bytes = upload_image_stride;
    }

    auto color_type = viz::ToClosestSkColorType(yuv_si_format, plane_index);
    SkImageInfo info = SkImageInfo::Make(resource_size_pixels.width(),
                                         resource_size_pixels.height(),
                                         color_type, kPremul_SkAlphaType);
    pixmaps[plane_index] = SkPixmap(info, pixels, pixels_stride_in_bytes);
  }
  resource->SetUniqueId(video_frame->unique_id());

  // TODO(crbug.com/41380578): This should really default to rec709.
  // Use the identity color space for when MatrixID is RGB for multiplanar
  // software video frames.
  // TODO(crbug.com/368870063): Implement RGB matrix support in
  // ToSkYUVColorSpace.
  SkYUVColorSpace color_space = kIdentity_SkYUVColorSpace;
  gfx::ColorSpace video_color_space = video_frame->ColorSpace();
  // There should be no usages of RGB matrix for color space here.
  CHECK(!video_color_space.IsValid() ||
            (video_color_space.GetMatrixID() != gfx::ColorSpace::MatrixID::RGB),
        base::NotFatalUntil::M139);
  if (video_color_space.IsValid() &&
      video_color_space.GetMatrixID() != gfx::ColorSpace::MatrixID::RGB) {
    // The ColorSpace is converted to SkYUVColorSpace but not used by
    // WritePixelsYUV.
    CHECK(video_color_space.ToSkYUVColorSpace(video_frame->BitDepth(),
                                              &color_space));
  }
  auto resource_size = gfx::SizeToSkISize(resource->size());
  SkYUVAInfo::PlaneConfig plane_config = ToSkYUVAPlaneConfig(yuv_si_format);
  SkYUVAInfo::Subsampling subsampling = ToSkYUVASubsampling(yuv_si_format);
  SkYUVAInfo info(resource_size, plane_config, subsampling, color_space);
  auto yuv_pixmap = SkYUVAPixmaps::FromExternalPixmaps(info, pixmaps.data());

  auto* ri = RasterInterface();
  std::unique_ptr<gpu::RasterScopedAccess> ri_access =
      resource->shared_image()->BeginRasterAccess(ri, resource->sync_token(),
                                                  /*readonly=*/false);

  ri->WritePixelsYUV(resource->shared_image()->mailbox(), yuv_pixmap);

  gpu::SyncToken ri_sync_token =
      gpu::RasterScopedAccess::EndAccess(std::move(ri_access));
  resource->UpdateSyncToken(ri_sync_token);

  return true;
}

VideoFrameExternalResource VideoResourceUpdater::CreateForSoftwareFrame(
    scoped_refptr<VideoFrame> video_frame) {
  TRACE_EVENT0("media", "VideoResourceUpdater::CreateForSoftwareFrame");
  const VideoPixelFormat input_frame_format = video_frame->format();
  size_t bits_per_channel = video_frame->BitDepth();
  DCHECK(IsYuvPlanar(input_frame_format) ||
         input_frame_format == PIXEL_FORMAT_Y16 ||
         IsFrameFormat32BitRGB(input_frame_format));

  viz::SharedImageFormat output_si_format =
      GetSoftwareOutputFormat(input_frame_format, bits_per_channel);
  gfx::ColorSpace output_color_space = video_frame->ColorSpace();

  // `output_si_format` can be single plane if we're using software compositor
  // or frame format is 32 bit RGB or we are unable to display frame format as
  // YUV planes directly and needs RGB conversion.
  if (output_si_format.is_single_plane()) {
    DCHECK(output_si_format == viz::SinglePlaneFormat::kBGRA_8888 ||
           output_si_format == viz::SinglePlaneFormat::kRGBA_8888);
    // The YUV to RGB conversion will be performed when we convert
    // from single-channel textures to an RGBA texture via
    // ConvertVideoFrameToRGBPixels below.
    output_color_space = output_color_space.GetAsFullRangeRGB();
  }

  // For multiplanar shared images, we only need to store size for first plane
  // (subplane sizes are handled automatically within shared images) and
  // create a single multiplanar resource.
  gfx::Size output_resource_size = video_frame->coded_size();
  if (output_resource_size.IsEmpty() ||
      output_resource_size.width() > max_resource_size_ ||
      output_resource_size.height() > max_resource_size_) {
    // This output resource has invalid geometry so return an empty external
    // resources.
    DLOG(ERROR)
        << "Video resource is too large to upload. Maximum dimension is "
        << max_resource_size_ << " and resource is "
        << output_resource_size.ToString();
    return VideoFrameExternalResource();
  }

  // Delete recycled resources that are the wrong format or wrong size.
  auto can_delete_resource_fn =
      [output_si_format, output_resource_size,
       output_color_space](const std::unique_ptr<FrameResource>& resource) {
        // Resources that are still being used can't be deleted.
        if (resource->has_refs()) {
          return false;
        }
        return !resource->Equals(output_resource_size, output_si_format,
                                 output_color_space);
      };
  std::erase_if(all_resources_, can_delete_resource_fn);

  // Recycle or allocate resource. For multiplanar shared images, we only need
  // to create a single multiplanar resource.
  FrameResource* frame_resource =
      RecycleOrAllocateResource(output_resource_size, output_si_format,
                                output_color_space, video_frame->unique_id());
  frame_resource->add_ref();

  // The formats must match.
  CHECK_EQ(output_si_format, frame_resource->format());
  CHECK_EQ(output_color_space, frame_resource->shared_image()->color_space());
  VideoFrameExternalResource external_resource;
  // `output_si_format` is single plane if we're using software compositor or
  // frame format is 32 bit RGB or we are unable to display frame format as YUV
  // planes directly and needs RGB conversion.
  if (output_si_format.is_single_plane()) {
    if (!frame_resource->Matches(video_frame->unique_id())) {
      // We need to transfer data from |video_frame| to the frame_resource.
      if (software_compositor()) {
        TransferRGBPixelsToPaintCanvas(video_frame, frame_resource);
      } else {
        if (!WriteRGBPixelsToTexture(video_frame, frame_resource)) {
          // Return empty resources if this fails.
          return VideoFrameExternalResource();
        }
      }
      frame_resource->SetUniqueId(video_frame->unique_id());
    }

    viz::TransferableResource::MetadataOverride overrides;
    overrides.alpha_type =
        software_compositor() ? kPremul_SkAlphaType : kUnpremul_SkAlphaType;
    auto transferable_resource = viz::TransferableResource::Make(
        frame_resource->shared_image(),
        viz::TransferableResource::ResourceSource::kVideo,
        frame_resource->sync_token(), overrides);
    transferable_resource.hdr_metadata =
        video_frame->hdr_metadata().value_or(gfx::HDRMetadata());
    transferable_resource.needs_detiling =
        video_frame->metadata().needs_detiling;
    external_resource.resource = std::move(transferable_resource);
    external_resource.type = software_compositor()
                                 ? VideoFrameResourceType::RGBA_PREMULTIPLIED
                                 : VideoFrameResourceType::RGB;
    external_resource.release_callback =
        base::BindOnce(&VideoResourceUpdater::RecycleResource,
                       weak_ptr_factory_.GetWeakPtr(), frame_resource->id());

    return external_resource;
  }

  CHECK(output_si_format.is_multi_plane());
  if (!WriteYUVPixelsForAllPlanesToTexture(video_frame, frame_resource,
                                           bits_per_channel)) {
    // Return empty resource if this fails.
    return VideoFrameExternalResource();
  }

  viz::TransferableResource::MetadataOverride overrides;
  overrides.alpha_type = kUnpremul_SkAlphaType;
  auto transferable_resource = viz::TransferableResource::Make(
      frame_resource->shared_image(),
      viz::TransferableResource::ResourceSource::kVideo,
      frame_resource->sync_token(), overrides);
  transferable_resource.hdr_metadata =
      video_frame->hdr_metadata().value_or(gfx::HDRMetadata());

  external_resource.resource = std::move(transferable_resource);
  external_resource.release_callback =
      base::BindOnce(&VideoResourceUpdater::RecycleResource,
                     weak_ptr_factory_.GetWeakPtr(), frame_resource->id());
  // With multiplanar shared images, a TextureDrawQuad is created.
  external_resource.type = VideoFrameResourceType::RGB;

  return external_resource;
}

gpu::raster::RasterInterface* VideoResourceUpdater::RasterInterface() {
  auto* ri = context_provider_->RasterInterface();
  CHECK(ri);
  return ri;
}

void VideoResourceUpdater::ReturnTexture(
    scoped_refptr<VideoFrame> video_frame,
    const gpu::SyncToken& original_release_token,
    const gpu::SyncToken& new_release_token,
    bool lost_resource) {
  // Note: This method is called for each plane texture in the frame! Which
  // means it may end up receiving the same `new_release_token` multiple times.

  if (lost_resource) {
    return;
  }

  if (!new_release_token.HasData()) {
    return;
  }

  ResourceSyncTokenClient client(RasterInterface(), original_release_token,
                                 new_release_token);
  video_frame->UpdateReleaseSyncToken(&client);
}

void VideoResourceUpdater::RecycleResource(uint32_t resource_id,
                                           const gpu::SyncToken& sync_token,
                                           bool lost_resource) {
  auto resource_it =
      std::ranges::find(all_resources_, resource_id, &FrameResource::id);
  if (resource_it == all_resources_.end())
    return;

  if (context_provider_ && sync_token.HasData()) {
    (*resource_it)->UpdateSyncToken(sync_token);
  }

  if (lost_resource) {
    all_resources_.erase(resource_it);
  } else {
    (*resource_it)->remove_ref();
  }
}

bool VideoResourceUpdater::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  for (auto& resource : all_resources_) {
    std::string dump_name = base::StringPrintf(
        "cc/video_memory/updater_%d/resource_%d", tracing_id_, resource->id());
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_name);

    const uint64_t total_bytes =
        resource->format().EstimatedSizeInBytes(resource->size());
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_bytes);

    // The importance value assigned to the GUID here must be greater than the
    // importance value assigned elsewhere so that resource ownership is
    // attributed to VideoResourceUpdater.
    constexpr int kImportance = 2;

    // Resources are shared across processes and require a shared GUID to
    // prevent double counting the memory.
    resource->shared_image()->OnMemoryDump(pmd, dump->guid(), kImportance);
  }

  return true;
}

gpu::SharedImageInterface* VideoResourceUpdater::shared_image_interface()
    const {
  return shared_image_interface_.get();
}

viz::ResourceId VideoResourceUpdater::GetFrameResourceIdForTesting() const {
  return frame_resource_id_;
}

}  // namespace media
