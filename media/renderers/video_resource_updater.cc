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

#include <string>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/client/shared_bitmap_reporter.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "media/base/media_switches.h"
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

bool MediaSharedBitmapConversionEnabled() {
  return base::FeatureList::IsEnabled(kMediaSharedBitmapToSharedImage);
}

// Generates process-unique IDs to use for tracing video resources.
base::AtomicSequenceNumber g_next_video_resource_updater_id;

gfx::ProtectedVideoType ProtectedVideoTypeFromMetadata(
    const VideoFrameMetadata& metadata) {
  if (!metadata.protected_video) {
    return gfx::ProtectedVideoType::kClear;
  }

  return metadata.hw_protected ? gfx::ProtectedVideoType::kHardwareProtected
                               : gfx::ProtectedVideoType::kSoftwareProtected;
}

VideoFrameResourceType ExternalResourceTypeForHardwarePlanes(
    const VideoFrame& frame,
    GLuint target,
    viz::SharedImageFormat& si_format,
    bool use_stream_video_draw_quad) {
  const VideoPixelFormat format = frame.format();

  if (frame.RequiresExternalSampler()) {
    // The texture |target| can be 0 for Fuchsia.
    DCHECK(target == 0 || target == GL_TEXTURE_EXTERNAL_OES)
        << "Unsupported target " << gl::GLEnums::GetStringEnum(target);
#if BUILDFLAG(IS_OZONE)
    // The format must be one of NV12/YV12/P010LE/NV12A, as these are the only
    // formats for which VideoFrame::RequiresExternalSampler() will return
    // true.
    switch (format) {
      case PIXEL_FORMAT_NV12:
        si_format = viz::MultiPlaneFormat::kNV12;
        break;
      case PIXEL_FORMAT_YV12:
        si_format = viz::MultiPlaneFormat::kYV12;
        break;
      case PIXEL_FORMAT_P010LE:
        si_format = viz::MultiPlaneFormat::kP010;
        break;
      case PIXEL_FORMAT_NV12A:
        si_format = viz::MultiPlaneFormat::kNV12A;
        break;
      default:
        NOTREACHED();
    }
    si_format.SetPrefersExternalSampler();
    return format == PIXEL_FORMAT_NV12A ? VideoFrameResourceType::RGBA
                                        : VideoFrameResourceType::RGB;
#else
    // MultiplanarSharedImage with external sampling is supported only on
    // Ozone, and VideoFrames with external sampler should not be created on
    // other platforms.
    NOTREACHED_NORETURN();
#endif
  }

  CHECK(!frame.RequiresExternalSampler());

  switch (format) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_BGRA:
      // This maps VideoPixelFormat back to SharedImageFormat
      // NOTE: ABGR == RGBA and ARGB == BGRA, they differ only byte order
      // See: VideoFormat function in gpu_memory_buffer_video_frame_pool
      // https://cs.chromium.org/chromium/src/media/video/gpu_memory_buffer_video_frame_pool.cc?type=cs&g=0&l=281
      si_format = (format == PIXEL_FORMAT_ABGR || format == PIXEL_FORMAT_XBGR)
                      ? viz::SinglePlaneFormat::kRGBA_8888
                      : viz::SinglePlaneFormat::kBGRA_8888;

      switch (target) {
        case GL_TEXTURE_EXTERNAL_OES:
          // `use_stream_video_draw_quad` is set on Android and `dcomp_surface`
          // is used on Windows.
          // TODO(sunnyps): It's odd to reuse the Android path on Windows. There
          // could be other unknown assumptions in other parts of the rendering
          // stack about stream video quads. Investigate alternative solutions.
          if (use_stream_video_draw_quad || frame.metadata().dcomp_surface)
            return VideoFrameResourceType::STREAM_TEXTURE;
          [[fallthrough]];
        case GL_TEXTURE_2D:
        case GL_TEXTURE_RECTANGLE_ARB:
          return (format == PIXEL_FORMAT_XRGB)
                     ? VideoFrameResourceType::RGB
                     : VideoFrameResourceType::RGBA_PREMULTIPLIED;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
      break;
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
      si_format = (format == PIXEL_FORMAT_XR30)
                      ? viz::SinglePlaneFormat::kBGRA_1010102
                      : viz::SinglePlaneFormat::kRGBA_1010102;
      return VideoFrameResourceType::RGB;
    case PIXEL_FORMAT_I420:
      si_format = viz::MultiPlaneFormat::kI420;
      return VideoFrameResourceType::RGB;

    case PIXEL_FORMAT_YV12:
      CHECK_EQ(frame.shared_image_format_type(),
               SharedImageFormatType::kSharedImageFormat);
      si_format = viz::MultiPlaneFormat::kYV12;
      return VideoFrameResourceType::RGBA;

    case PIXEL_FORMAT_NV12:
      // |target| is set to 0 for Vulkan textures.
      //
      // TODO(crbug.com/40144615): Note that GL_TEXTURE_EXTERNAL_OES is
      // allowed even for two-texture NV12 frames. This is intended to handle a
      // couple of cases: a) when these textures are connected to the
      // corresponding plane of the contents of an EGLStream using
      // EGL_NV_stream_consumer_gltexture_yuv; b) when D3DImageBacking is used
      // with GL_TEXTURE_EXTERNAL_OES (note that this case should be able to be
      // migrated to GL_TEXTURE_2D after https://crrev.com/c/3856660).
      DCHECK(target == 0 || target == GL_TEXTURE_EXTERNAL_OES ||
             target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE_ARB)
          << "Unsupported target " << gl::GLEnums::GetStringEnum(target);
      si_format = viz::MultiPlaneFormat::kNV12;
      return VideoFrameResourceType::RGB;

    case PIXEL_FORMAT_NV16:
      si_format = viz::MultiPlaneFormat::kNV16;
      return VideoFrameResourceType::RGB;

    case PIXEL_FORMAT_NV24:
      si_format = viz::MultiPlaneFormat::kNV24;
      return VideoFrameResourceType::RGB;

    case PIXEL_FORMAT_NV12A:
      si_format = viz::MultiPlaneFormat::kNV12A;
      return VideoFrameResourceType::RGBA;

    case PIXEL_FORMAT_P010LE:
      si_format = viz::MultiPlaneFormat::kP010;
      return VideoFrameResourceType::RGB;

    case PIXEL_FORMAT_P210LE:
      si_format = viz::MultiPlaneFormat::kP210;
      return VideoFrameResourceType::RGB;

    case PIXEL_FORMAT_P410LE:
      si_format = viz::MultiPlaneFormat::kP410;
      return VideoFrameResourceType::RGB;

    case PIXEL_FORMAT_RGBAF16:
      si_format = viz::SinglePlaneFormat::kRGBA_F16;
      return VideoFrameResourceType::RGBA;

    case PIXEL_FORMAT_UYVY:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
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

// For frames that we receive in software format, determine the dimensions of
// first plane in the frame.
gfx::Size SoftwareFirstPlaneDimension(VideoFrame* input_frame,
                                      bool software_compositor) {
  if (software_compositor)
    return input_frame->coded_size();

  int plane_width = input_frame->columns(/*plane=*/0);
  int plane_height = input_frame->rows(/*plane=*/0);
  return gfx::Size(plane_width, plane_height);
}

SkYUVAInfo::PlaneConfig ToSkYUVAPlaneConfig(viz::SharedImageFormat format) {
  using PlaneConfig = viz::SharedImageFormat::PlaneConfig;
  switch (format.plane_config()) {
    case PlaneConfig::kY_U_V:
      return SkYUVAInfo::PlaneConfig::kY_U_V;
    case PlaneConfig::kY_V_U:
      return SkYUVAInfo::PlaneConfig::kY_V_U;
    case PlaneConfig::kY_UV:
      return SkYUVAInfo::PlaneConfig::kY_UV;
    case PlaneConfig::kY_UV_A:
      return SkYUVAInfo::PlaneConfig::kY_UV_A;
    case PlaneConfig::kY_U_V_A:
      return SkYUVAInfo::PlaneConfig::kY_U_V_A;
  }
}

SkYUVAInfo::Subsampling ToSkYUVASubsampling(viz::SharedImageFormat format) {
  using Subsampling = viz::SharedImageFormat::Subsampling;
  switch (format.subsampling()) {
    case Subsampling::k420:
      return SkYUVAInfo::Subsampling::k420;
    case Subsampling::k422:
      return SkYUVAInfo::Subsampling::k422;
    case Subsampling::k444:
      return SkYUVAInfo::Subsampling::k444;
  }
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

viz::SharedImageFormat GetSingleChannel8BitFormat(
    const gpu::Capabilities& caps,
    const gpu::SharedImageCapabilities& shared_image_caps) {
  if (caps.texture_rg && !shared_image_caps.disable_r8_shared_images) {
    return viz::SinglePlaneFormat::kR_8;
  }

  DCHECK(shared_image_caps.supports_luminance_shared_images);
  return viz::SinglePlaneFormat::kLUMINANCE_8;
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

bool IsFormat16BitFloat(viz::SharedImageFormat format) {
  // Assume multiplanar SharedImageFormat with ChannelFormat::k16F is always
  // used as LUMINANCEF16.
  CHECK(format.is_multi_plane());
  return format.channel_format() == viz::SharedImageFormat::ChannelFormat::k16F;
}

viz::SharedImageFormat::ChannelFormat SupportedMultiPlaneChannelFormat(
    viz::SharedImageFormat format) {
  if (format == viz::SinglePlaneFormat::kR_16) {
    return viz::SharedImageFormat::ChannelFormat::k16;
  }
  if (format == viz::SinglePlaneFormat::kLUMINANCE_F16 ||
      format == viz::SinglePlaneFormat::kR_F16) {
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
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV420P10:
      return viz::SharedImageFormat::MultiPlane(
          PlaneConfig::kY_U_V, Subsampling::k420, ChannelFormat::k10);
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV422P10:
      return viz::SharedImageFormat::MultiPlane(
          PlaneConfig::kY_U_V, Subsampling::k422, ChannelFormat::k10);
    case PIXEL_FORMAT_YUV444P9:
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

// Resource for a video plane allocated and owned by VideoResourceUpdater. There
// can be multiple plane resources for each video frame, depending on the
// format. These will be reused when possible.
class VideoResourceUpdater::PlaneResource {
 public:
  PlaneResource(uint32_t plane_resource_id,
                const gfx::Size& resource_size,
                viz::SharedImageFormat si_format,
                bool is_software)
      : plane_resource_id_(plane_resource_id),
        resource_size_(resource_size),
        si_format_(si_format),
        is_software_(is_software) {}

  PlaneResource(const PlaneResource&) = delete;
  PlaneResource& operator=(const PlaneResource&) = delete;

  virtual ~PlaneResource() = default;

  // Casts |this| to SoftwarePlaneResource for software compositing.
  SoftwarePlaneResource* AsSoftware();

  // Casts |this| to HardwarePlaneResource for GPU compositing.
  HardwarePlaneResource* AsHardware();

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

  // Accessors for resource identifiers provided at construction time.
  uint32_t plane_resource_id() const { return plane_resource_id_; }
  const gfx::Size& resource_size() const { return resource_size_; }
  viz::SharedImageFormat si_format() const { return si_format_; }

  // Various methods for managing references. See |ref_count_| for details.
  void add_ref() { ++ref_count_; }
  void remove_ref() { --ref_count_; }
  void clear_refs() { ref_count_ = 0; }
  bool has_refs() const { return ref_count_ != 0; }

 private:
  const uint32_t plane_resource_id_;
  const gfx::Size resource_size_;
  const viz::SharedImageFormat si_format_;
  const bool is_software_;

  // The number of times this resource has been imported vs number of times this
  // resource has returned.
  int ref_count_ = 0;

  // Identifies the data stored in this resource; uniquely identify a
  // VideoFrame.
  VideoFrame::ID unique_frame_id_;
};

class VideoResourceUpdater::SoftwarePlaneResource
    : public VideoResourceUpdater::PlaneResource {
 public:
  SoftwarePlaneResource(
      uint32_t plane_resource_id,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      viz::SharedBitmapReporter* shared_bitmap_reporter,
      scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface,
      VideoResourceUpdater* video_resource_updater)
      : PlaneResource(plane_resource_id,
                      size,
                      shared_image_interface
                          ? viz::SinglePlaneFormat::kBGRA_8888
                          : viz::SinglePlaneFormat::kRGBA_8888,
                      /*is_software=*/true),
        video_resource_updater_(video_resource_updater),
        shared_bitmap_reporter_(shared_bitmap_reporter),
        shared_bitmap_id_(shared_image_interface
                              ? viz::SharedBitmapId()
                              : viz::SharedBitmap::GenerateId()) {
    if (shared_image_interface) {
      auto shared_image_mapping = shared_image_interface->CreateSharedImage(
          {viz::SinglePlaneFormat::kBGRA_8888, size, color_space,
           gpu::SHARED_IMAGE_USAGE_CPU_WRITE, "VideoResourceUpdater"});
      shared_image_ = std::move(shared_image_mapping.shared_image);
      shared_mapping_ = std::move(shared_image_mapping.mapping);
      CHECK(shared_image_);
    } else {
      DCHECK(shared_bitmap_reporter_);
      // Allocate SharedMemory and notify display compositor of the allocation.
      base::MappedReadOnlyRegion shm =
          viz::bitmap_allocation::AllocateSharedBitmap(
              resource_size(), viz::SinglePlaneFormat::kRGBA_8888);
      shared_mapping_ = std::move(shm.mapping);
      shared_bitmap_reporter_->DidAllocateSharedBitmap(std::move(shm.region),
                                                       shared_bitmap_id_);
    }
  }

  SoftwarePlaneResource(const SoftwarePlaneResource&) = delete;
  SoftwarePlaneResource& operator=(const SoftwarePlaneResource&) = delete;

  ~SoftwarePlaneResource() override {
    if (shared_image_) {
      auto shared_image_interface =
          video_resource_updater_->shared_image_interface();
      if (shared_image_interface) {
        shared_image_interface->DestroySharedImage(
            GetSyncToken(shared_image_interface), std::move(shared_image_));
      }
    } else {
      shared_bitmap_reporter_->DidDeleteSharedBitmap(shared_bitmap_id_);
    }
  }

  const scoped_refptr<gpu::ClientSharedImage>& shared_image() const {
    return shared_image_;
  }

  const viz::SharedBitmapId& shared_bitmap_id() const {
    return shared_bitmap_id_;
  }

  void* pixels() { return shared_mapping_.memory(); }

  gpu::SyncToken GetSyncToken(
      scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface) {
    if (shared_image_ && shared_image_interface) {
      return shared_image_interface->GenVerifiedSyncToken();
    }

    return gpu::SyncToken();
  }

  // Returns a memory dump GUID consistent across processes.
  base::UnguessableToken GetSharedMemoryGuid() const {
    return shared_mapping_.guid();
  }

  viz::SharedImageFormat SupportedFormat() {
    return shared_image_ ? viz::SinglePlaneFormat::kBGRA_8888
                         : viz::SinglePlaneFormat::kRGBA_8888;
  }

 private:
  // Used for SharedImage.
  // SoftwarePlaneResource is called only in VideoResourceUpdater.
  const raw_ptr<VideoResourceUpdater> video_resource_updater_;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;

  // Used for SharedBitmap.
  const raw_ptr<viz::SharedBitmapReporter> shared_bitmap_reporter_;
  const viz::SharedBitmapId shared_bitmap_id_;

  base::WritableSharedMemoryMapping shared_mapping_;
};

class VideoResourceUpdater::HardwarePlaneResource
    : public VideoResourceUpdater::PlaneResource {
 public:
  HardwarePlaneResource(uint32_t plane_resource_id,
                        const gfx::Size& size,
                        viz::SharedImageFormat format,
                        const gfx::ColorSpace& color_space,
                        bool use_gpu_memory_buffer_resources,
                        viz::RasterContextProvider* context_provider)
      : PlaneResource(plane_resource_id, size, format, /*is_software=*/false),
        context_provider_(context_provider) {
    DCHECK(context_provider_);
    auto* sii = SharedImageInterface();
    if (format.is_single_plane()) {
      // TODO(crbug.com/40239769): Set `overlay_candidate_` for multiplanar
      // formats.
      overlay_candidate_ =
          use_gpu_memory_buffer_resources &&
          sii->GetCapabilities().supports_scanout_shared_images &&
          CanCreateGpuMemoryBufferForSinglePlaneSharedImageFormat(format);
    }
    // These SharedImages will be sent over to the display compositor as
    // TransferableResources. RasterInterface which in turn uses RasterDecoder
    // writes the contents of video frames into SharedImages.
    gpu::SharedImageUsageSet shared_image_usage =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
        gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
    if (overlay_candidate_) {
      shared_image_usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    }
    shared_image_ = sii->CreateSharedImage(
        {format, size, color_space, shared_image_usage, "VideoResourceUpdater"},
        gpu::kNullSurfaceHandle);
    CHECK(shared_image_);
    // Determine if a platform-specific target is needed.
    texture_target_ = shared_image_->GetTextureTarget();
    RasterInterface()->WaitSyncTokenCHROMIUM(
        sii->GenUnverifiedSyncToken().GetConstData());
  }

  HardwarePlaneResource(const HardwarePlaneResource&) = delete;
  HardwarePlaneResource& operator=(const HardwarePlaneResource&) = delete;

  ~HardwarePlaneResource() override {
    gpu::SyncToken sync_token;
    RasterInterface()->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
    SharedImageInterface()->DestroySharedImage(sync_token,
                                               std::move(shared_image_));
  }

  const gpu::Mailbox& mailbox() const { return shared_image_->mailbox(); }

  GLenum texture_target() const { return texture_target_; }
  bool overlay_candidate() const { return overlay_candidate_; }

 private:
  gpu::SharedImageInterface* SharedImageInterface() {
    auto* sii = context_provider_->SharedImageInterface();
    DCHECK(sii);
    return sii;
  }

  gpu::raster::RasterInterface* RasterInterface() {
    auto* ri = context_provider_->RasterInterface();
    CHECK(ri);
    return ri;
  }

  const raw_ptr<viz::RasterContextProvider> context_provider_;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  GLenum texture_target_ = GL_TEXTURE_2D;
  bool overlay_candidate_ = false;
};

VideoResourceUpdater::SoftwarePlaneResource*
VideoResourceUpdater::PlaneResource::AsSoftware() {
  DCHECK(is_software_);
  return static_cast<SoftwarePlaneResource*>(this);
}

VideoResourceUpdater::HardwarePlaneResource*
VideoResourceUpdater::PlaneResource::AsHardware() {
  DCHECK(!is_software_);
  return static_cast<HardwarePlaneResource*>(this);
}

VideoResourceUpdater::VideoResourceUpdater(
    viz::RasterContextProvider* context_provider,
    viz::SharedBitmapReporter* shared_bitmap_reporter,
    viz::ClientResourceProvider* resource_provider,
    scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface,
    bool use_stream_video_draw_quad,
    bool use_gpu_memory_buffer_resources,
    int max_resource_size)
    : context_provider_(context_provider),
      shared_bitmap_reporter_(shared_bitmap_reporter),
      shared_image_interface_(MediaSharedBitmapConversionEnabled()
                                  ? std::move(shared_image_interface)
                                  : nullptr),
      resource_provider_(resource_provider),
      use_stream_video_draw_quad_(use_stream_video_draw_quad),
      use_gpu_memory_buffer_resources_(use_gpu_memory_buffer_resources),
      max_resource_size_(max_resource_size),
      tracing_id_(g_next_video_resource_updater_id.GetNext()) {
  DCHECK(context_provider_ || shared_bitmap_reporter_ ||
         shared_image_interface_);

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

  if (video_frame->metadata().overlay_plane_id.has_value()) {
    // This is a hole punching VideoFrame, there is nothing to display.
    overlay_plane_id_ = *video_frame->metadata().overlay_plane_id;
    frame_resource_type_ = VideoFrameResourceType::VIDEO_HOLE;
    return;
  }

  VideoFrameExternalResource external_resource =
      CreateExternalResourceFromVideoFrame(video_frame);
  frame_resource_type_ = external_resource.type;

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
    case VideoFrameResourceType::RGBA:
    case VideoFrameResourceType::RGBA_PREMULTIPLIED:
    case VideoFrameResourceType::RGB:
    case VideoFrameResourceType::STREAM_TEXTURE: {
      if (frame_resource_id_.is_null()) {
        break;
      }
      bool premultiplied_alpha =
          frame_resource_type_ == VideoFrameResourceType::RGBA_PREMULTIPLIED;

      bool flipped = !frame->metadata().texture_origin_is_top_left;
      bool nearest_neighbor = false;
      gfx::ProtectedVideoType protected_video_type =
          ProtectedVideoTypeFromMetadata(frame->metadata());
      auto* texture_quad =
          render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
      texture_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                           needs_blending, frame_resource_id_,
                           premultiplied_alpha, uv_top_left, uv_bottom_right,
                           SkColors::kTransparent, flipped, nearest_neighbor,
                           false, protected_video_type);
      texture_quad->set_resource_size_in_pixels(coded_size);
      // Set the is_stream_video flag for STREAM_TEXTURE. Is used downstream
      // (e.g. *_layer_overlay.cc).
      texture_quad->is_stream_video =
          frame_resource_type_ == VideoFrameResourceType::STREAM_TEXTURE;
#if BUILDFLAG(IS_WIN)
      // Windows uses DComp surfaces to e.g. hold MediaFoundation videos, which
      // must be promoted to overlay to be composited correctly.
      if (frame->metadata().dcomp_surface) {
        texture_quad->overlay_priority_hint = viz::OverlayPriority::kRequired;
      }
#endif
      texture_quad->is_video_frame = true;
      for (viz::ResourceId resource_id : texture_quad->resources) {
        resource_provider_->ValidateResource(resource_id);
      }

      break;
    }
    case VideoFrameResourceType::NONE:
      NOTIMPLEMENTED();
      break;
  }
}

VideoFrameExternalResource
VideoResourceUpdater::CreateExternalResourceFromVideoFrame(
    scoped_refptr<VideoFrame> video_frame) {
  if (video_frame->format() == PIXEL_FORMAT_UNKNOWN)
    return VideoFrameExternalResource();
  DCHECK(video_frame->HasSharedImage() || video_frame->IsMappable());
  if (video_frame->HasSharedImage()) {
    return CreateForHardwarePlanes(std::move(video_frame));
  } else {
    return CreateForSoftwarePlanes(std::move(video_frame));
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

VideoResourceUpdater::PlaneResource*
VideoResourceUpdater::RecycleOrAllocateResource(
    const gfx::Size& resource_size,
    viz::SharedImageFormat si_format,
    const gfx::ColorSpace& color_space,
    VideoFrame::ID unique_id) {
  PlaneResource* recyclable_resource = nullptr;
  for (auto& resource : all_resources_) {
    // If the unique id is valid then we are allowed to return a referenced
    // resource that already contains the right frame data. It's safe to reuse
    // it even if resource_provider_ holds some references to it, because those
    // references are read-only.
    if (!unique_id.is_null() && resource->Matches(unique_id)) {
      DCHECK(resource->resource_size() == resource_size);
      DCHECK(resource->si_format() == si_format);
      return resource.get();
    }

    // Otherwise check whether this is an unreferenced resource of the right
    // format that we can recycle. Remember it, but don't return immediately,
    // because we still want to find any reusable resources.
    const bool in_use = resource->has_refs();

    if (!in_use && resource->resource_size() == resource_size &&
        resource->si_format() == si_format) {
      recyclable_resource = resource.get();
    }
  }

  if (recyclable_resource)
    return recyclable_resource;

  // There was nothing available to reuse or recycle. Allocate a new resource.
  return AllocateResource(resource_size, si_format, color_space);
}

VideoResourceUpdater::PlaneResource* VideoResourceUpdater::AllocateResource(
    const gfx::Size& plane_size,
    viz::SharedImageFormat format,
    const gfx::ColorSpace& color_space) {
  const uint32_t plane_resource_id = next_plane_resource_id_++;

  if (software_compositor()) {
    DCHECK_EQ(format,
              (shared_image_interface() ? viz::SinglePlaneFormat::kBGRA_8888
                                        : viz::SinglePlaneFormat::kRGBA_8888));

    all_resources_.push_back(std::make_unique<SoftwarePlaneResource>(
        plane_resource_id, plane_size, color_space, shared_bitmap_reporter_,
        shared_image_interface(), this));
  } else {
    all_resources_.push_back(std::make_unique<HardwarePlaneResource>(
        plane_resource_id, plane_size, format, color_space,
        use_gpu_memory_buffer_resources_, context_provider_));
  }
  return all_resources_.back().get();
}

void VideoResourceUpdater::CopyHardwarePlane(
    VideoFrame* video_frame,
    VideoFrameExternalResource* external_resource) {
  const gfx::Size output_plane_resource_size = video_frame->coded_size();
  auto shared_image = video_frame->shared_image();
  // The copy needs to be a direct transfer of pixel data, so we use an RGBA8
  // target to avoid loss of precision or dropping any alpha component.
  constexpr viz::SharedImageFormat copy_si_format =
      viz::SinglePlaneFormat::kRGBA_8888;

  // We copy to RGBA image, so we need only RGBA portion of the color space.
  const auto copy_color_space = video_frame->ColorSpace().GetAsFullRangeRGB();

  const VideoFrame::ID no_unique_id;  // Do not recycle referenced textures.
  PlaneResource* plane_resource =
      RecycleOrAllocateResource(output_plane_resource_size, copy_si_format,
                                copy_color_space, no_unique_id);
  HardwarePlaneResource* hardware_resource = plane_resource->AsHardware();
  hardware_resource->add_ref();

  DCHECK_EQ(hardware_resource->texture_target(),
            static_cast<GLenum>(GL_TEXTURE_2D));

  auto* ri = RasterInterface();
  ri->WaitSyncTokenCHROMIUM(video_frame->acquire_sync_token().GetConstData());

  ri->CopySharedImage(
      shared_image->mailbox(), hardware_resource->mailbox(), GL_TEXTURE_2D,
      /*xoffset=*/0, /*yoffset=*/0, /*x=*/0, /*y=*/0,
      output_plane_resource_size.width(), output_plane_resource_size.height(),
      /*unpack_flip_y=*/false, /*unpack_premultiply_alpha=*/false);

  // Wait (if the existing token isn't null) and replace it with a new one.
  //
  // This path is currently only used with single mailbox frames. Assert this
  // here since this code isn't tuned for multiple planes; it should only update
  // the release token once.
  WaitAndReplaceSyncTokenClient client(RasterInterface());
  gpu::SyncToken sync_token = video_frame->UpdateReleaseSyncToken(&client);

  auto transferable_resource = viz::TransferableResource::MakeGpu(
      hardware_resource->mailbox(), GL_TEXTURE_2D, sync_token,
      output_plane_resource_size, copy_si_format,
      false /* is_overlay_candidate */,
      viz::TransferableResource::ResourceSource::kVideo);
  transferable_resource.color_space = copy_color_space;
  transferable_resource.hdr_metadata =
      video_frame->hdr_metadata().value_or(gfx::HDRMetadata());
  transferable_resource.needs_detiling = video_frame->metadata().needs_detiling;

  external_resource->resource = std::move(transferable_resource);
  external_resource->release_callback = base::BindOnce(
      &VideoResourceUpdater::RecycleResource, weak_ptr_factory_.GetWeakPtr(),
      hardware_resource->plane_resource_id());
}

VideoFrameExternalResource VideoResourceUpdater::CreateForHardwarePlanes(
    scoped_refptr<VideoFrame> video_frame) {
  TRACE_EVENT0("media", "VideoResourceUpdater::CreateForHardwarePlanes");
  DCHECK(video_frame->HasSharedImage());
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

  viz::SharedImageFormat si_format;
  external_resource.type = ExternalResourceTypeForHardwarePlanes(
      *video_frame, target, si_format, use_stream_video_draw_quad_);
  external_resource.bits_per_channel = video_frame->BitDepth();

  if (external_resource.type == VideoFrameResourceType::NONE) {
    DLOG(ERROR) << "Unsupported Texture format"
                << VideoPixelFormatToString(video_frame->format());
    return external_resource;
  }

  // Make a copy of the current release SyncToken so we know if it changes.
  CopyingSyncTokenClient client;
  auto original_release_token = video_frame->UpdateReleaseSyncToken(&client);

  if (copy_required) {
    CopyHardwarePlane(video_frame.get(), &external_resource);
    return external_resource;
  }

  const size_t width = video_frame->columns(/*plane=*/0);
  const size_t height = video_frame->rows(/*plane=*/0);
  const gfx::Size size(width, height);
  auto transfer_resource = viz::TransferableResource::MakeGpu(
      shared_image->mailbox(), shared_image->GetTextureTarget(),
      video_frame->acquire_sync_token(), size, si_format,
      video_frame->metadata().allow_overlay,
      viz::TransferableResource::ResourceSource::kVideo);
  transfer_resource.color_space = video_frame->ColorSpace();
  transfer_resource.hdr_metadata =
      video_frame->hdr_metadata().value_or(gfx::HDRMetadata());
  transfer_resource.needs_detiling = video_frame->metadata().needs_detiling;
  if (video_frame->metadata().read_lock_fences_enabled) {
    transfer_resource.synchronization_type =
        viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted;
  }
  transfer_resource.ycbcr_info = video_frame->ycbcr_info();

#if BUILDFLAG(IS_ANDROID)
  transfer_resource.is_backed_by_surface_texture =
      video_frame->metadata().texture_owner;
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

viz::SharedImageFormat VideoResourceUpdater::YuvSharedImageFormat(
    int bits_per_channel) {
  DCHECK(context_provider_);
  const auto& caps = context_provider_->ContextCapabilities();
  const auto& shared_image_caps =
      context_provider_->SharedImageInterface()->GetCapabilities();
  if (caps.disable_one_component_textures) {
    return PaintCanvasVideoRenderer::GetRGBPixelsOutputFormat();
  }
  if (bits_per_channel <= 8) {
    DCHECK(shared_image_caps.supports_luminance_shared_images ||
           caps.texture_rg);
    return GetSingleChannel8BitFormat(caps, shared_image_caps);
  }
  if (caps.texture_norm16 && shared_image_caps.supports_r16_shared_images) {
    return viz::SinglePlaneFormat::kR_16;
  }
  if (shared_image_caps.is_r16f_supported) {
    return viz::SinglePlaneFormat::kR_F16;
  }
  if (caps.texture_half_float_linear &&
      shared_image_caps.supports_luminance_shared_images) {
    return viz::SinglePlaneFormat::kLUMINANCE_F16;
  }
  return GetSingleChannel8BitFormat(caps, shared_image_caps);
}

viz::SharedImageFormat VideoResourceUpdater::GetSoftwareOutputFormat(
    VideoPixelFormat input_frame_format,
    int bits_per_channel,
    const gfx::ColorSpace& input_frame_color_space,
    bool& texture_needs_rgb_conversion_out) {
  viz::SharedImageFormat output_si_format;
  // TODO(crbug.com/332564976, hitawala): Simplify this format conversion
  // process.
  if (IsFrameFormat32BitRGB(input_frame_format)) {
    texture_needs_rgb_conversion_out = false;
    output_si_format = GetRGBSharedImageFormat(input_frame_format);
  } else if (input_frame_format == PIXEL_FORMAT_Y16) {
    // Unable to display directly as yuv planes so convert it to RGB.
    texture_needs_rgb_conversion_out = true;
  } else if (!software_compositor()) {
    // Can be composited directly from yuv planes.
    output_si_format = YuvSharedImageFormat(bits_per_channel);

    // If GPU compositing is enabled, but the output resource format returned by
    // the resource provider is viz::SinglePlaneFormat::kRGBA_8888, then a GPU
    // driver bug workaround requires that YUV frames must be converted to RGB
    // before texture upload.
    if (output_si_format == viz::SinglePlaneFormat::kRGBA_8888 ||
        output_si_format == viz::SinglePlaneFormat::kBGRA_8888) {
      texture_needs_rgb_conversion_out = true;
    }

    // Some YUV resources have different sized planes. If we lack the proper
    // SharedImageFormat just convert to RGB. We could do something better like
    // unpacking to I420/I016, but texture_rg and r16 support should be pretty
    // universal and we expect these frames to be rare.
    if (input_frame_format == PIXEL_FORMAT_NV12) {
      if (output_si_format != viz::SinglePlaneFormat::kR_8) {
        texture_needs_rgb_conversion_out = true;
      }
    } else {
      DCHECK_EQ(VideoFrame::BytesPerElement(input_frame_format, 0),
                VideoFrame::BytesPerElement(input_frame_format, 1));
    }

    // If it is multiplanar and does not need RGB conversion, go through
    // RasterDecoder WritePixelsYUV path.
    if (!texture_needs_rgb_conversion_out) {
      // Get the supported channel format for the `output_si_format`'s first
      // plane.
      auto channel_format = SupportedMultiPlaneChannelFormat(output_si_format);
      // Now get the multiplanar shared image format for `input_frame_format`.
      output_si_format =
          VideoPixelFormatToMultiPlanarSharedImageFormat(input_frame_format);
      if (output_si_format.channel_format() != channel_format) {
        // If the requested channel format is not supported, use the supported
        // channel format and downsample later if needed.
        output_si_format = viz::SharedImageFormat::MultiPlane(
            output_si_format.plane_config(), output_si_format.subsampling(),
            channel_format);
      }
    }
  }

  if (software_compositor() || texture_needs_rgb_conversion_out) {
    output_si_format =
        software_compositor()
            ? (shared_image_interface() ? viz::SinglePlaneFormat::kBGRA_8888
                                        : viz::SinglePlaneFormat::kRGBA_8888)
            : PaintCanvasVideoRenderer::GetRGBPixelsOutputFormat();
  }

  return output_si_format;
}

void VideoResourceUpdater::TransferRGBPixelsToPaintCanvas(
    scoped_refptr<VideoFrame> video_frame,
    PlaneResource* plane_resource) {
  if (!video_renderer_) {
    video_renderer_ = std::make_unique<PaintCanvasVideoRenderer>();
  }

  SoftwarePlaneResource* software_resource = plane_resource->AsSoftware();

  DCHECK_EQ(plane_resource->si_format(), software_resource->SupportedFormat());

  // We know the format is RGBA_8888 or BGRA_8888 from check above.
  SkImageInfo info = SkImageInfo::MakeN32Premul(
      gfx::SizeToSkISize(software_resource->resource_size()));

  SkBitmap sk_bitmap;
  sk_bitmap.installPixels(info, software_resource->pixels(),
                          info.minRowBytes());
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
    PlaneResource* plane_resource,
    viz::SharedImageFormat output_si_format) {
  HardwarePlaneResource* hardware_resource = plane_resource->AsHardware();
  size_t bytes_per_row = viz::ResourceSizes::CheckedWidthInBytes<size_t>(
      video_frame->coded_size().width(), output_si_format);
  const gfx::Size& plane_size = hardware_resource->resource_size();

  const VideoPixelFormat input_frame_format = video_frame->format();
  // Note: Strides may be negative in case of bottom-up layouts.
  const int stride = video_frame->stride(VideoFrame::Plane::kARGB);
  const bool has_compatible_stride =
      stride > 0 && static_cast<size_t>(stride) == bytes_per_row;

  const uint8_t* source_pixels = nullptr;
  if (HasCompatibleRGBFormat(input_frame_format, output_si_format) &&
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
  auto color_type = viz::ToClosestSkColorType(
      /*gpu_compositing=*/true, output_si_format, /*plane_index=*/0);
  SkImageInfo info = SkImageInfo::Make(plane_size.width(), plane_size.height(),
                                       color_type, kPremul_SkAlphaType);
  SkPixmap pixmap = SkPixmap(info, source_pixels, bytes_per_row);
  ri->WritePixels(hardware_resource->mailbox(), /*dst_x_offset=*/0,
                  /*dst_y_offset=*/0, hardware_resource->texture_target(),
                  pixmap);

  return true;
}

bool VideoResourceUpdater::WriteYUVPixelsForAllPlanesToTexture(
    scoped_refptr<VideoFrame> video_frame,
    HardwarePlaneResource* resource,
    size_t bits_per_channel) {
  // Skip the transfer if this |video_frame|'s plane has been processed.
  if (resource->Matches(video_frame->unique_id())) {
    return true;
  }

  auto yuv_si_format = resource->si_format();
  SkPixmap pixmaps[SkYUVAInfo::kMaxPlanes] = {};
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
        yuv_si_format.GetPlaneSize(plane_index, resource->resource_size());

    const size_t plane_size_in_bytes =
        yuv_si_format
            .MaybeEstimatedPlaneSizeInBytes(plane_index,
                                            resource->resource_size())
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

    // We need to convert the incoming data if we're transferring to half
    // float, if there is need for bit downshift or if the strides need to
    // be reconciled.
    const bool needs_conversion = IsFormat16BitFloat(yuv_si_format) ||
                                  needs_bit_downshifting ||
                                  needs_bit_upshifting;
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

      if (IsFormat16BitFloat(yuv_si_format)) {
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
        NOTREACHED_IN_MIGRATION();
      }

      pixels = upload_pixels_[plane_index].get();
      pixels_stride_in_bytes = upload_image_stride;
    }

    auto color_type = viz::ToClosestSkColorType(
        /*gpu_compositing=*/true, yuv_si_format, plane_index);
    SkImageInfo info = SkImageInfo::Make(resource_size_pixels.width(),
                                         resource_size_pixels.height(),
                                         color_type, kPremul_SkAlphaType);
    pixmaps[plane_index] = SkPixmap(info, pixels, pixels_stride_in_bytes);
  }
  resource->SetUniqueId(video_frame->unique_id());

  SkISize video_size{resource->resource_size().width(),
                     resource->resource_size().height()};
  SkYUVAInfo::PlaneConfig plane_config = ToSkYUVAPlaneConfig(yuv_si_format);
  SkYUVAInfo::Subsampling subsampling = ToSkYUVASubsampling(yuv_si_format);

  // TODO(crbug.com/41380578): This should really default to rec709.
  // Use the identity color space for when MatrixID is RGB for multiplanar
  // software video frames.
  // TODO(crbug.com/368870063): Implement RGB matrix support in
  // ToSkYUVColorSpace.
  SkYUVColorSpace color_space = kIdentity_SkYUVColorSpace;
  if (video_frame->ColorSpace().IsValid() &&
      video_frame->ColorSpace().GetMatrixID() !=
          gfx::ColorSpace::MatrixID::RGB) {
    // The ColorSpace is converted to SkYUVColorSpace but not used by
    // WritePixelsYUV.
    CHECK(video_frame->ColorSpace().ToSkYUVColorSpace(video_frame->BitDepth(),
                                                      &color_space));
  }
  SkYUVAInfo info =
      SkYUVAInfo(video_size, plane_config, subsampling, color_space);
  SkYUVAPixmaps yuv_pixmap = SkYUVAPixmaps::FromExternalPixmaps(info, pixmaps);
  RasterInterface()->WritePixelsYUV(resource->mailbox(), yuv_pixmap);

  return true;
}

VideoFrameExternalResource VideoResourceUpdater::CreateForSoftwarePlanes(
    scoped_refptr<VideoFrame> video_frame) {
  TRACE_EVENT0("media", "VideoResourceUpdater::CreateForSoftwarePlanes");
  const VideoPixelFormat input_frame_format = video_frame->format();
  size_t bits_per_channel = video_frame->BitDepth();
  DCHECK(IsYuvPlanar(input_frame_format) ||
         input_frame_format == PIXEL_FORMAT_Y16 ||
         IsFrameFormat32BitRGB(input_frame_format));

  bool texture_needs_rgb_conversion = false;
  viz::SharedImageFormat output_si_format = GetSoftwareOutputFormat(
      input_frame_format, bits_per_channel, video_frame->ColorSpace(),
      texture_needs_rgb_conversion);
  gfx::ColorSpace output_color_space = video_frame->ColorSpace();

  // TODO(skaslev): If we're in software compositing mode, we do the YUV -> RGB
  // conversion here. That involves an extra copy of each frame to a bitmap.
  // Obviously, this is suboptimal and should be addressed once ubercompositor
  // starts shaping up.
  if (software_compositor() || texture_needs_rgb_conversion) {
    bits_per_channel = 8;

    // The YUV to RGB conversion will be performed when we convert
    // from single-channel textures to an RGBA texture via
    // ConvertVideoFrameToRGBPixels below.
    output_color_space = output_color_space.GetAsFullRangeRGB();
  }

  // For multiplanar shared images, we only need to store size for first plane
  // (subplane sizes are handled automatically within shared images) and
  // create a single multiplanar resource.
  gfx::Size output_resource_size =
      SoftwareFirstPlaneDimension(video_frame.get(), software_compositor());
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
      [output_si_format,
       output_resource_size](const std::unique_ptr<PlaneResource>& resource) {
        // Resources that are still being used can't be deleted.
        if (resource->has_refs())
          return false;

        return resource->si_format() != output_si_format ||
               resource->resource_size() != output_resource_size;
      };
  std::erase_if(all_resources_, can_delete_resource_fn);

  // Recycle or allocate resource. For multiplanar shared images, we only need
  // to create a single multiplanar resource.
  PlaneResource* plane_resource =
      RecycleOrAllocateResource(output_resource_size, output_si_format,
                                output_color_space, video_frame->unique_id());
  plane_resource->add_ref();

  VideoFrameExternalResource external_resource;
  external_resource.bits_per_channel = bits_per_channel;

  if (software_compositor() || texture_needs_rgb_conversion ||
      IsFrameFormat32BitRGB(input_frame_format)) {
    CHECK(output_si_format.is_single_plane());

    if (!plane_resource->Matches(video_frame->unique_id())) {
      // We need to transfer data from |video_frame| to the plane resource.
      if (software_compositor()) {
        TransferRGBPixelsToPaintCanvas(video_frame, plane_resource);
      } else {
        if (!WriteRGBPixelsToTexture(video_frame, plane_resource,
                                     output_si_format)) {
          // Return empty resources if this fails.
          return VideoFrameExternalResource();
        }
      }
      plane_resource->SetUniqueId(video_frame->unique_id());
    }

    viz::TransferableResource transferable_resource;
    if (software_compositor()) {
      SoftwarePlaneResource* software_resource = plane_resource->AsSoftware();
      external_resource.type = VideoFrameResourceType::RGBA_PREMULTIPLIED;
      const auto& shared_image = software_resource->shared_image();
      transferable_resource =
          shared_image
              ? viz::TransferableResource::MakeSoftwareSharedImage(
                    shared_image,
                    software_resource->GetSyncToken(shared_image_interface()),
                    software_resource->resource_size(),
                    plane_resource->si_format(),
                    viz::TransferableResource::ResourceSource::kVideo)
              : viz::TransferableResource::MakeSoftwareSharedBitmap(
                    software_resource->shared_bitmap_id(),
                    software_resource->GetSyncToken(shared_image_interface()),
                    software_resource->resource_size(),
                    plane_resource->si_format(),
                    viz::TransferableResource::ResourceSource::kVideo);
    } else {
      HardwarePlaneResource* hardware_resource = plane_resource->AsHardware();
      external_resource.type = VideoFrameResourceType::RGBA;
      gpu::SyncToken sync_token;
      RasterInterface()->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
      transferable_resource = viz::TransferableResource::MakeGpu(
          hardware_resource->mailbox(), hardware_resource->texture_target(),
          sync_token, hardware_resource->resource_size(), output_si_format,
          hardware_resource->overlay_candidate(),
          viz::TransferableResource::ResourceSource::kVideo);
    }

    transferable_resource.color_space = output_color_space;
    transferable_resource.hdr_metadata =
        video_frame->hdr_metadata().value_or(gfx::HDRMetadata());
    transferable_resource.needs_detiling =
        video_frame->metadata().needs_detiling;
    external_resource.resource = std::move(transferable_resource);
    external_resource.release_callback = base::BindOnce(
        &VideoResourceUpdater::RecycleResource, weak_ptr_factory_.GetWeakPtr(),
        plane_resource->plane_resource_id());

    return external_resource;
  }

  CHECK(output_si_format.is_multi_plane());
  HardwarePlaneResource* resource = plane_resource->AsHardware();
  CHECK_EQ(resource->si_format(), output_si_format);
  if (!WriteYUVPixelsForAllPlanesToTexture(video_frame, resource,
                                           bits_per_channel)) {
    // Return empty resource if this fails.
    return VideoFrameExternalResource();
  }

  // Set the sync token otherwise resource is assumed to be synchronized.
  gpu::SyncToken sync_token;
  RasterInterface()->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  auto transferable_resource = viz::TransferableResource::MakeGpu(
      resource->mailbox(), resource->texture_target(), sync_token,
      resource->resource_size(), output_si_format,
      resource->overlay_candidate(),
      viz::TransferableResource::ResourceSource::kVideo);
  transferable_resource.color_space = output_color_space;
  transferable_resource.hdr_metadata =
      video_frame->hdr_metadata().value_or(gfx::HDRMetadata());

  external_resource.resource = std::move(transferable_resource);
  external_resource.release_callback = base::BindOnce(
      &VideoResourceUpdater::RecycleResource, weak_ptr_factory_.GetWeakPtr(),
      resource->plane_resource_id());
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

void VideoResourceUpdater::RecycleResource(uint32_t plane_resource_id,
                                           const gpu::SyncToken& sync_token,
                                           bool lost_resource) {
  auto resource_it = base::ranges::find(all_resources_, plane_resource_id,
                                        &PlaneResource::plane_resource_id);
  if (resource_it == all_resources_.end())
    return;

  if (context_provider_ && sync_token.HasData()) {
    RasterInterface()->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
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
    std::string dump_name =
        base::StringPrintf("cc/video_memory/updater_%d/resource_%d",
                           tracing_id_, resource->plane_resource_id());
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_name);

    const uint64_t total_bytes =
        resource->si_format().EstimatedSizeInBytes(resource->resource_size());
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_bytes);

    // The importance value assigned to the GUID here must be greater than the
    // importance value assigned elsewhere so that resource ownership is
    // attributed to VideoResourceUpdater.
    constexpr int kImportance = 2;

    // Resources are shared across processes and require a shared GUID to
    // prevent double counting the memory.
    if (software_compositor()) {
      base::UnguessableToken shm_guid =
          resource->AsSoftware()->GetSharedMemoryGuid();
      pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shm_guid, kImportance);
    } else {
      base::trace_event::MemoryAllocatorDumpGuid guid =
          gpu::GetSharedImageGUIDForTracing(resource->AsHardware()->mailbox());
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }

  return true;
}

scoped_refptr<gpu::ClientSharedImageInterface>
VideoResourceUpdater::shared_image_interface() const {
  return shared_image_interface_;
}

}  // namespace media
