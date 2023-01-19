// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_resource_updater.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/atomic_sequence_num.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
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
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/format_utils.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/half_float_maker.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/khronos/GLES3/gl3.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/size_conversions.h"
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
  if (!metadata.protected_video) {
    return gfx::ProtectedVideoType::kClear;
  }

  return metadata.hw_protected ? gfx::ProtectedVideoType::kHardwareProtected
                               : gfx::ProtectedVideoType::kSoftwareProtected;
}

VideoFrameResourceType ExternalResourceTypeForHardwarePlanes(
    const VideoFrame& frame,
    GLuint target,
    gfx::BufferFormat buffer_formats[VideoFrame::kMaxPlanes],
    bool use_stream_video_draw_quad) {
  const VideoPixelFormat format = frame.format();
  const size_t num_textures = frame.NumTextures();

  if (frame.RequiresExternalSampler()) {
    // The texture |target| can be 0 for Fuchsia.
    DCHECK(target == 0 || target == GL_TEXTURE_EXTERNAL_OES)
        << "Unsupported target " << gl::GLEnums::GetStringEnum(target);
    DCHECK_EQ(num_textures, 1u);
    absl::optional<gfx::BufferFormat> buffer_format =
        VideoPixelFormatToGfxBufferFormat(format);
    DCHECK(buffer_format.has_value());
    buffer_formats[0] = buffer_format.value();
    return VideoFrameResourceType::RGB;
  }

  switch (format) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_BGRA:
      DCHECK_EQ(num_textures, 1u);
      // This maps VideoPixelFormat back to GMB BufferFormat
      // NOTE: ABGR == RGBA and ARGB == BGRA, they differ only byte order
      // See: VideoFormat function in gpu_memory_buffer_video_frame_pool
      // https://cs.chromium.org/chromium/src/media/video/gpu_memory_buffer_video_frame_pool.cc?type=cs&g=0&l=281
      buffer_formats[0] =
          (format == PIXEL_FORMAT_ABGR || format == PIXEL_FORMAT_XBGR)
              ? gfx::BufferFormat::RGBA_8888
              : gfx::BufferFormat::BGRA_8888;

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
          NOTREACHED();
          break;
      }
      break;
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
      buffer_formats[0] = (format == PIXEL_FORMAT_XR30)
                              ? gfx::BufferFormat::BGRA_1010102
                              : gfx::BufferFormat::RGBA_1010102;
      return VideoFrameResourceType::RGB;
    case PIXEL_FORMAT_I420:
      DCHECK_EQ(num_textures, 3u);
      buffer_formats[0] = gfx::BufferFormat::R_8;
      buffer_formats[1] = gfx::BufferFormat::R_8;
      buffer_formats[2] = gfx::BufferFormat::R_8;
      return VideoFrameResourceType::YUV;

    case PIXEL_FORMAT_NV12:
      // |target| is set to 0 for Vulkan textures.
      //
      // TODO(https://crbug.com/1116101): Note that GL_TEXTURE_EXTERNAL_OES is
      // allowed even for two-texture NV12 frames. This is intended to handle a
      // couple of cases: a) when these textures are connected to the
      // corresponding plane of the contents of an EGLStream using
      // EGL_NV_stream_consumer_gltexture_yuv; b) when gl::GLImageD3D is used
      // with GL_TEXTURE_EXTERNAL_OES (note that this case should be able to be
      // migrated to GL_TEXTURE_2D after https://crrev.com/c/3856660).
      DCHECK(target == 0 || target == GL_TEXTURE_EXTERNAL_OES ||
             target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE_ARB)
          << "Unsupported target " << gl::GLEnums::GetStringEnum(target);
      DCHECK_EQ(num_textures, 2u);
      buffer_formats[0] = gfx::BufferFormat::R_8;
      buffer_formats[1] = gfx::BufferFormat::RG_88;
      return VideoFrameResourceType::YUV;

    case PIXEL_FORMAT_NV12A:
      DCHECK_EQ(num_textures, 3u);
      buffer_formats[0] = gfx::BufferFormat::R_8;
      buffer_formats[1] = gfx::BufferFormat::RG_88;
      buffer_formats[2] = gfx::BufferFormat::R_8;
      return VideoFrameResourceType::YUVA;

    case PIXEL_FORMAT_P016LE:
      DCHECK_EQ(num_textures, 2u);
      // TODO(mcasas): Support other formats such as e.g. P012.
      buffer_formats[0] = gfx::BufferFormat::R_16;
      // TODO(https://crbug.com/1233228): This needs to be
      // gfx::BufferFormat::RG_1616.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
      buffer_formats[1] = gfx::BufferFormat::RG_1616;
#else
      buffer_formats[1] = gfx::BufferFormat::RG_88;
#endif
      return VideoFrameResourceType::YUV;

    case PIXEL_FORMAT_RGBAF16:
      DCHECK_EQ(num_textures, 1u);
      buffer_formats[0] = gfx::BufferFormat::RGBA_F16;
      return VideoFrameResourceType::RGBA;

    case PIXEL_FORMAT_UYVY:
      NOTREACHED();
      [[fallthrough]];
    case PIXEL_FORMAT_YV12:
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

class SyncTokenClientImpl : public VideoFrame::SyncTokenClient {
 public:
  SyncTokenClientImpl(gpu::gles2::GLES2Interface* gl,
                      gpu::SharedImageInterface* sii,
                      gpu::SyncToken sync_token)
      : gl_(gl), sii_(sii), sync_token_(sync_token) {
    // Only one interface should be used.
    DCHECK((gl_ && !sii_) || (!gl_ && sii_));
  }

  SyncTokenClientImpl(const SyncTokenClientImpl&) = delete;
  SyncTokenClientImpl& operator=(const SyncTokenClientImpl&) = delete;

  ~SyncTokenClientImpl() override = default;

  void GenerateSyncToken(gpu::SyncToken* sync_token) override {
    if (sync_token_.HasData()) {
      *sync_token = sync_token_;
    } else {
      if (gl_) {
        gl_->GenSyncTokenCHROMIUM(sync_token->GetData());
      } else {
        *sync_token = sii_->GenVerifiedSyncToken();
      }
    }
  }

  void WaitSyncToken(const gpu::SyncToken& sync_token) override {
    if (sync_token.HasData()) {
      if (gl_) {
        gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
      } else {
        sii_->WaitSyncToken(sync_token);
      }
      if (sync_token_.HasData() && sync_token_ != sync_token) {
        if (gl_) {
          gl_->WaitSyncTokenCHROMIUM(sync_token_.GetConstData());
        } else {
          sii_->WaitSyncToken(sync_token);
        }
        sync_token_.Clear();
      }
    }
  }

 private:
  raw_ptr<gpu::gles2::GLES2Interface> gl_;
  raw_ptr<gpu::SharedImageInterface> sii_;
  gpu::SyncToken sync_token_;
};

// Sync tokens passed downstream to the compositor can be unverified.
void GenerateCompositorSyncToken(gpu::gles2::GLES2Interface* gl,
                                 gpu::SyncToken* sync_token) {
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token->GetData());
}

// For frames that we receive in software format, determine the dimensions of
// each plane in the frame.
gfx::Size SoftwarePlaneDimension(VideoFrame* input_frame,
                                 bool software_compositor,
                                 size_t plane_index) {
  if (software_compositor)
    return input_frame->coded_size();

  int plane_width = input_frame->columns(plane_index);
  int plane_height = input_frame->rows(plane_index);
  return gfx::Size(plane_width, plane_height);
}

viz::ResourceFormat GetRGBResourceFormat(VideoPixelFormat format) {
#if BUILDFLAG(IS_MAC)
  // macOS IOSurfaces are always BGRA_8888.
  return PaintCanvasVideoRenderer::GetRGBPixelsOutputFormat();
#else
  switch (format) {
    case PIXEL_FORMAT_XBGR:
      return viz::RGBX_8888;
    case PIXEL_FORMAT_ABGR:
      return viz::RGBA_8888;
    case PIXEL_FORMAT_XRGB:
      return viz::BGRX_8888;
    case PIXEL_FORMAT_ARGB:
      return viz::BGRA_8888;
    default:
      NOTREACHED();
      return viz::RGBA_8888;
  }
#endif
}

// Returns true if the input VideoFrame format can be stored directly in the
// provided output resource format.
bool HasCompatibleFormat(VideoPixelFormat input_format,
                         viz::ResourceFormat output_format) {
  if (input_format == PIXEL_FORMAT_XBGR)
    return output_format == viz::RGBA_8888 || output_format == viz::RGBX_8888;
  if (input_format == PIXEL_FORMAT_ABGR)
    return output_format == viz::RGBA_8888;
  if (input_format == PIXEL_FORMAT_XRGB)
    return output_format == viz::BGRA_8888 || output_format == viz::BGRX_8888;
  if (input_format == PIXEL_FORMAT_ARGB)
    return output_format == viz::BGRA_8888;
  return false;
}

}  // namespace

VideoFrameExternalResources::VideoFrameExternalResources() = default;
VideoFrameExternalResources::~VideoFrameExternalResources() = default;

VideoFrameExternalResources::VideoFrameExternalResources(
    VideoFrameExternalResources&& other) = default;
VideoFrameExternalResources& VideoFrameExternalResources::operator=(
    VideoFrameExternalResources&& other) = default;

// Resource for a video plane allocated and owned by VideoResourceUpdater. There
// can be multiple plane resources for each video frame, depending on the
// format. These will be reused when possible.
class VideoResourceUpdater::PlaneResource {
 public:
  PlaneResource(uint32_t plane_resource_id,
                const gfx::Size& resource_size,
                viz::ResourceFormat resource_format,
                bool is_software)
      : plane_resource_id_(plane_resource_id),
        resource_size_(resource_size),
        resource_format_(resource_format),
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
  bool Matches(VideoFrame::ID unique_frame_id, size_t plane_index) {
    return has_unique_frame_id_and_plane_index_ &&
           unique_frame_id_ == unique_frame_id && plane_index_ == plane_index;
  }

  // Sets the unique identifiers for this resource, may only be called when
  // there is a single reference to the resource (i.e. |ref_count_| == 1).
  void SetUniqueId(VideoFrame::ID unique_frame_id, size_t plane_index) {
    DCHECK_EQ(ref_count_, 1);
    plane_index_ = plane_index;
    unique_frame_id_ = unique_frame_id;
    has_unique_frame_id_and_plane_index_ = true;
  }

  // Accessors for resource identifiers provided at construction time.
  uint32_t plane_resource_id() const { return plane_resource_id_; }
  const gfx::Size& resource_size() const { return resource_size_; }
  viz::ResourceFormat resource_format() const { return resource_format_; }

  // Various methods for managing references. See |ref_count_| for details.
  void add_ref() { ++ref_count_; }
  void remove_ref() { --ref_count_; }
  void clear_refs() { ref_count_ = 0; }
  bool has_refs() const { return ref_count_ != 0; }

 private:
  const uint32_t plane_resource_id_;
  const gfx::Size resource_size_;
  const viz::ResourceFormat resource_format_;
  const bool is_software_;

  // The number of times this resource has been imported vs number of times this
  // resource has returned.
  int ref_count_ = 0;

  // These two members are used for identifying the data stored in this
  // resource; they uniquely identify a VideoFrame plane.
  VideoFrame::ID unique_frame_id_;
  size_t plane_index_ = 0u;
  // Indicates if the above two members have been set or not.
  bool has_unique_frame_id_and_plane_index_ = false;
};

class VideoResourceUpdater::SoftwarePlaneResource
    : public VideoResourceUpdater::PlaneResource {
 public:
  SoftwarePlaneResource(uint32_t plane_resource_id,
                        const gfx::Size& size,
                        viz::SharedBitmapReporter* shared_bitmap_reporter)
      : PlaneResource(plane_resource_id,
                      size,
                      viz::ResourceFormat::RGBA_8888,
                      /*is_software=*/true),
        shared_bitmap_reporter_(shared_bitmap_reporter),
        shared_bitmap_id_(viz::SharedBitmap::GenerateId()) {
    DCHECK(shared_bitmap_reporter_);

    // Allocate SharedMemory and notify display compositor of the allocation.
    base::MappedReadOnlyRegion shm =
        viz::bitmap_allocation::AllocateSharedBitmap(
            resource_size(), viz::ResourceFormat::RGBA_8888);
    shared_mapping_ = std::move(shm.mapping);
    shared_bitmap_reporter_->DidAllocateSharedBitmap(std::move(shm.region),
                                                     shared_bitmap_id_);
  }

  SoftwarePlaneResource(const SoftwarePlaneResource&) = delete;
  SoftwarePlaneResource& operator=(const SoftwarePlaneResource&) = delete;

  ~SoftwarePlaneResource() override {
    shared_bitmap_reporter_->DidDeleteSharedBitmap(shared_bitmap_id_);
  }

  const viz::SharedBitmapId& shared_bitmap_id() const {
    return shared_bitmap_id_;
  }
  void* pixels() { return shared_mapping_.memory(); }

  // Returns a memory dump GUID consistent across processes.
  base::UnguessableToken GetSharedMemoryGuid() const {
    return shared_mapping_.guid();
  }

 private:
  const raw_ptr<viz::SharedBitmapReporter> shared_bitmap_reporter_;
  const viz::SharedBitmapId shared_bitmap_id_;
  base::WritableSharedMemoryMapping shared_mapping_;
};

class VideoResourceUpdater::HardwarePlaneResource
    : public VideoResourceUpdater::PlaneResource {
 public:
  // Provides a RAII scope to access the HardwarePlaneResource as a texture on a
  // GL context. This will wait on the sync token and provide the shared image
  // access scope.
  class ScopedTexture {
   public:
    ScopedTexture(gpu::gles2::GLES2Interface* gl,
                  HardwarePlaneResource* resource)
        : gl_(gl) {
      texture_id_ = gl_->CreateAndTexStorage2DSharedImageCHROMIUM(
          resource->mailbox().name);
      gl_->BeginSharedImageAccessDirectCHROMIUM(
          texture_id_, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
    }

    ~ScopedTexture() {
      gl_->EndSharedImageAccessDirectCHROMIUM(texture_id_);
      gl_->DeleteTextures(1, &texture_id_);
    }

    GLuint texture_id() const { return texture_id_; }

   private:
    raw_ptr<gpu::gles2::GLES2Interface> gl_;
    GLuint texture_id_;
  };

  HardwarePlaneResource(uint32_t plane_resource_id,
                        const gfx::Size& size,
                        viz::ResourceFormat format,
                        const gfx::ColorSpace& color_space,
                        bool use_gpu_memory_buffer_resources,
                        viz::ContextProvider* context_provider,
                        viz::RasterContextProvider* raster_context_provider)
      : PlaneResource(plane_resource_id, size, format, /*is_software=*/false),
        context_provider_(context_provider),
        raster_context_provider_(raster_context_provider) {
    DCHECK(context_provider_ || raster_context_provider_);
    const gpu::Capabilities& caps =
        raster_context_provider_
            ? raster_context_provider_->ContextCapabilities()
            : context_provider_->ContextCapabilities();
    overlay_candidate_ = use_gpu_memory_buffer_resources &&
                         caps.supports_scanout_shared_images &&
                         IsGpuMemoryBufferFormatSupported(format);
    uint32_t shared_image_usage =
        gpu::SHARED_IMAGE_USAGE_GLES2 | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
    if (overlay_candidate_) {
      shared_image_usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
      texture_target_ = gpu::GetBufferTextureTarget(gfx::BufferUsage::SCANOUT,
                                                    BufferFormat(format), caps);
    }
    auto* sii = SharedImageInterface();
    mailbox_ = sii->CreateSharedImage(
        format, size, color_space, kTopLeft_GrSurfaceOrigin,
        kPremul_SkAlphaType, shared_image_usage, gpu::kNullSurfaceHandle);
    ContextGL()->WaitSyncTokenCHROMIUM(
        sii->GenUnverifiedSyncToken().GetConstData());
  }

  HardwarePlaneResource(const HardwarePlaneResource&) = delete;
  HardwarePlaneResource& operator=(const HardwarePlaneResource&) = delete;

  ~HardwarePlaneResource() override {
    gpu::SyncToken sync_token;
    ContextGL()->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
    SharedImageInterface()->DestroySharedImage(sync_token, mailbox_);
  }

  const gpu::Mailbox& mailbox() const { return mailbox_; }

  GLenum texture_target() const { return texture_target_; }
  bool overlay_candidate() const { return overlay_candidate_; }

 private:
  gpu::SharedImageInterface* SharedImageInterface() {
    auto* sii = raster_context_provider_
                    ? raster_context_provider_->SharedImageInterface()
                    : context_provider_->SharedImageInterface();
    DCHECK(sii);
    return sii;
  }

  gpu::gles2::GLES2Interface* ContextGL() {
    auto* gl = raster_context_provider_ ? raster_context_provider_->ContextGL()
                                        : context_provider_->ContextGL();
    DCHECK(gl);
    return gl;
  }

  const raw_ptr<viz::ContextProvider> context_provider_;
  const raw_ptr<viz::RasterContextProvider> raster_context_provider_;
  gpu::Mailbox mailbox_;
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
    viz::ContextProvider* context_provider,
    viz::RasterContextProvider* raster_context_provider,
    viz::SharedBitmapReporter* shared_bitmap_reporter,
    viz::ClientResourceProvider* resource_provider,
    bool use_stream_video_draw_quad,
    bool use_gpu_memory_buffer_resources,
    bool use_r16_texture,
    int max_resource_size)
    : context_provider_(context_provider),
      raster_context_provider_(raster_context_provider),
      shared_bitmap_reporter_(shared_bitmap_reporter),
      resource_provider_(resource_provider),
      use_stream_video_draw_quad_(use_stream_video_draw_quad),
      use_gpu_memory_buffer_resources_(use_gpu_memory_buffer_resources),
      use_r16_texture_(use_r16_texture),
      max_resource_size_(max_resource_size),
      tracing_id_(g_next_video_resource_updater_id.GetNext()) {
  DCHECK(context_provider_ || raster_context_provider_ ||
         shared_bitmap_reporter_);

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "media::VideoResourceUpdater",
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

VideoResourceUpdater::~VideoResourceUpdater() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void VideoResourceUpdater::ObtainFrameResources(
    scoped_refptr<VideoFrame> video_frame) {
  if (video_frame->metadata().overlay_plane_id.has_value()) {
    // This is a hole punching VideoFrame, there is nothing to display.
    overlay_plane_id_ = *video_frame->metadata().overlay_plane_id;
    frame_resource_type_ = VideoFrameResourceType::VIDEO_HOLE;
    return;
  }

  VideoFrameExternalResources external_resources =
      CreateExternalResourcesFromVideoFrame(video_frame);
  frame_resource_type_ = external_resources.type;

  if (external_resources.type == VideoFrameResourceType::YUV ||
      external_resources.type == VideoFrameResourceType::YUVA) {
    frame_resource_offset_ = external_resources.offset;
    frame_resource_multiplier_ = external_resources.multiplier;
    frame_bits_per_channel_ = external_resources.bits_per_channel;
  }

  DCHECK_EQ(external_resources.resources.size(),
            external_resources.release_callbacks.size());
  for (size_t i = 0; i < external_resources.resources.size(); ++i) {
    viz::ResourceId resource_id = resource_provider_->ImportResource(
        external_resources.resources[i],
        std::move(external_resources.release_callbacks[i]));
    frame_resources_.emplace_back(resource_id,
                                  external_resources.resources[i].size);
  }
  TRACE_EVENT_INSTANT1("media", "VideoResourceUpdater::ObtainFrameResources",
                       TRACE_EVENT_SCOPE_THREAD, "Timestamp",
                       video_frame->timestamp().InMicroseconds());
}

void VideoResourceUpdater::ReleaseFrameResources() {
  for (auto& frame_resource : frame_resources_)
    resource_provider_->RemoveImportedResource(frame_resource.id);
  frame_resources_.clear();
}

void VideoResourceUpdater::AppendQuads(
    viz::CompositorRenderPass* render_pass,
    scoped_refptr<VideoFrame> frame,
    gfx::Transform transform,
    gfx::Rect quad_rect,
    gfx::Rect visible_quad_rect,
    const gfx::MaskFilterInfo& mask_filter_info,
    absl::optional<gfx::Rect> clip_rect,
    bool contents_opaque,
    float draw_opacity,
    int sorting_context_id) {
  DCHECK(frame.get());

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(
      transform, quad_rect, visible_quad_rect, mask_filter_info, clip_rect,
      contents_opaque, draw_opacity, SkBlendMode::kSrcOver, sorting_context_id);

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
    case VideoFrameResourceType::YUV: {
      DCHECK_EQ(frame_resources_.size(),
                VideoFrame::NumPlanes(frame->format()));
      if (frame->HasTextures()) {
        DCHECK(frame->format() == PIXEL_FORMAT_NV12 ||
               frame->format() == PIXEL_FORMAT_P016LE ||
               frame->format() == PIXEL_FORMAT_I420);
      }

      // Get the scaling factor of the YA texture relative to the UV texture.
      const gfx::Size uv_sample_size =
          VideoFrame::SampleSize(frame->format(), VideoFrame::kUPlane);

      auto* yuv_video_quad =
          render_pass->CreateAndAppendDrawQuad<viz::YUVVideoDrawQuad>();
      yuv_video_quad->SetNew(
          shared_quad_state, quad_rect, visible_quad_rect, needs_blending,
          coded_size, visible_rect, uv_sample_size, frame_resources_[0].id,
          frame_resources_[1].id,
          frame_resources_.size() > 2 ? frame_resources_[2].id
                                      : frame_resources_[1].id,
          frame_resources_.size() > 3 ? frame_resources_[3].id
                                      : viz::kInvalidResourceId,
          frame->ColorSpace(), frame_resource_offset_,
          frame_resource_multiplier_, frame_bits_per_channel_,
          ProtectedVideoTypeFromMetadata(frame->metadata()),
          frame->hdr_metadata());

      for (viz::ResourceId resource_id : yuv_video_quad->resources) {
        resource_provider_->ValidateResource(resource_id);
      }
      break;
    }
    case VideoFrameResourceType::YUVA: {
      DCHECK_EQ(frame_resources_.size(),
                VideoFrame::NumPlanes(frame->format()));
      if (frame->HasTextures()) {
        DCHECK_EQ(frame->format(), PIXEL_FORMAT_NV12A);
      }

      // Get the scaling factor of the YA texture relative to the UV texture.
      const gfx::Size uv_sample_size =
          VideoFrame::SampleSize(frame->format(), VideoFrame::kUPlane);

      auto* yuv_video_quad =
          render_pass->CreateAndAppendDrawQuad<viz::YUVVideoDrawQuad>();
      yuv_video_quad->SetNew(
          shared_quad_state, quad_rect, visible_quad_rect, needs_blending,
          coded_size, visible_rect, uv_sample_size, frame_resources_[0].id,
          frame_resources_[1].id,
          frame_resources_.size() > 3 ? frame_resources_[2].id
                                      : frame_resources_[1].id,
          frame_resources_.size() > 3 ? frame_resources_[3].id
                                      : frame_resources_[2].id,
          frame->ColorSpace(), frame_resource_offset_,
          frame_resource_multiplier_, frame_bits_per_channel_,
          ProtectedVideoTypeFromMetadata(frame->metadata()),
          frame->hdr_metadata());

      for (viz::ResourceId resource_id : yuv_video_quad->resources) {
        resource_provider_->ValidateResource(resource_id);
      }
      break;
    }
    case VideoFrameResourceType::RGBA:
    case VideoFrameResourceType::RGBA_PREMULTIPLIED:
    case VideoFrameResourceType::RGB:
    case VideoFrameResourceType::STREAM_TEXTURE: {
      DCHECK_EQ(frame_resources_.size(), 1u);
      if (frame_resources_.size() < 1u)
        break;
      bool premultiplied_alpha =
          frame_resource_type_ == VideoFrameResourceType::RGBA_PREMULTIPLIED;

      float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
      bool flipped = !frame->metadata().texture_origin_is_top_left;
      bool nearest_neighbor = false;
      gfx::ProtectedVideoType protected_video_type =
          ProtectedVideoTypeFromMetadata(frame->metadata());
      auto* texture_quad =
          render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
      texture_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                           needs_blending, frame_resources_[0].id,
                           premultiplied_alpha, uv_top_left, uv_bottom_right,
                           SkColors::kTransparent, opacity, flipped,
                           nearest_neighbor, false, protected_video_type);
      texture_quad->set_resource_size_in_pixels(coded_size);
      // Set the is_stream_video flag for STREAM_TEXTURE. Is used downstream
      // (e.g. *_layer_overlay.cc).
      texture_quad->is_stream_video =
          frame_resource_type_ == VideoFrameResourceType::STREAM_TEXTURE;
      texture_quad->is_video_frame = true;
      texture_quad->hdr_metadata = frame->hdr_metadata();
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

VideoFrameExternalResources
VideoResourceUpdater::CreateExternalResourcesFromVideoFrame(
    scoped_refptr<VideoFrame> video_frame) {
  if (video_frame->format() == PIXEL_FORMAT_UNKNOWN)
    return VideoFrameExternalResources();
  DCHECK(video_frame->HasTextures() || video_frame->IsMappable());
  if (video_frame->HasTextures())
    return CreateForHardwarePlanes(std::move(video_frame));
  else
    return CreateForSoftwarePlanes(std::move(video_frame));
}

viz::ResourceFormat VideoResourceUpdater::YuvResourceFormat(
    int bits_per_channel) {
  DCHECK(raster_context_provider_ || context_provider_);
  const auto& caps = raster_context_provider_
                         ? raster_context_provider_->ContextCapabilities()
                         : context_provider_->ContextCapabilities();
  if (caps.disable_one_component_textures)
    return PaintCanvasVideoRenderer::GetRGBPixelsOutputFormat();
  if (bits_per_channel <= 8)
    return caps.texture_rg ? viz::RED_8 : viz::LUMINANCE_8;
  if (use_r16_texture_ && caps.texture_norm16)
    return viz::R16_EXT;
  if (caps.texture_half_float_linear)
    return viz::LUMINANCE_F16;
  return viz::LUMINANCE_8;
}

bool VideoResourceUpdater::ReallocateUploadPixels(size_t needed_size) {
  // Free the existing data first so that the memory can be reused, if
  // possible. Note that the new array is purposely not initialized.
  upload_pixels_.reset();
  uint8_t* pixel_mem = nullptr;
  // Fail if we can't support the required memory to upload pixels.
  if (!base::UncheckedMalloc(needed_size,
                             reinterpret_cast<void**>(&pixel_mem))) {
    DLOG(ERROR) << "Unable to allocate enough memory required to "
                   "upload pixels";
    return false;
  }
  upload_pixels_.reset(pixel_mem);
  upload_pixels_size_ = needed_size;
  return true;
}

VideoResourceUpdater::PlaneResource*
VideoResourceUpdater::RecycleOrAllocateResource(
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    const gfx::ColorSpace& color_space,
    VideoFrame::ID unique_id,
    int plane_index) {
  PlaneResource* recyclable_resource = nullptr;
  for (auto& resource : all_resources_) {
    // If the plane index is valid (positive, or 0, meaning all planes)
    // then we are allowed to return a referenced resource that already
    // contains the right frame data. It's safe to reuse it even if
    // resource_provider_ holds some references to it, because those
    // references are read-only.
    if (plane_index != -1 && resource->Matches(unique_id, plane_index)) {
      DCHECK(resource->resource_size() == resource_size);
      DCHECK(resource->resource_format() == resource_format);
      return resource.get();
    }

    // Otherwise check whether this is an unreferenced resource of the right
    // format that we can recycle. Remember it, but don't return immediately,
    // because we still want to find any reusable resources.
    const bool in_use = resource->has_refs();

    if (!in_use && resource->resource_size() == resource_size &&
        resource->resource_format() == resource_format) {
      recyclable_resource = resource.get();
    }
  }

  if (recyclable_resource)
    return recyclable_resource;

  // There was nothing available to reuse or recycle. Allocate a new resource.
  return AllocateResource(resource_size, resource_format, color_space);
}

VideoResourceUpdater::PlaneResource* VideoResourceUpdater::AllocateResource(
    const gfx::Size& plane_size,
    viz::ResourceFormat format,
    const gfx::ColorSpace& color_space) {
  const uint32_t plane_resource_id = next_plane_resource_id_++;

  if (software_compositor()) {
    DCHECK_EQ(format, viz::ResourceFormat::RGBA_8888);

    all_resources_.push_back(std::make_unique<SoftwarePlaneResource>(
        plane_resource_id, plane_size, shared_bitmap_reporter_));
  } else {
    all_resources_.push_back(std::make_unique<HardwarePlaneResource>(
        plane_resource_id, plane_size, format, color_space,
        use_gpu_memory_buffer_resources_, context_provider_,
        raster_context_provider_));
  }
  return all_resources_.back().get();
}

void VideoResourceUpdater::CopyHardwarePlane(
    VideoFrame* video_frame,
    const gfx::ColorSpace& resource_color_space,
    const gpu::MailboxHolder& mailbox_holder,
    VideoFrameExternalResources* external_resources) {
  const gfx::Size output_plane_resource_size = video_frame->coded_size();
  // The copy needs to be a direct transfer of pixel data, so we use an RGBA8
  // target to avoid loss of precision or dropping any alpha component.
  constexpr viz::ResourceFormat copy_resource_format =
      viz::ResourceFormat::RGBA_8888;

  const VideoFrame::ID no_unique_id;
  const int no_plane_index = -1;  // Do not recycle referenced textures.
  PlaneResource* plane_resource = RecycleOrAllocateResource(
      output_plane_resource_size, copy_resource_format, resource_color_space,
      no_unique_id, no_plane_index);
  HardwarePlaneResource* hardware_resource = plane_resource->AsHardware();
  hardware_resource->add_ref();

  DCHECK_EQ(hardware_resource->texture_target(),
            static_cast<GLenum>(GL_TEXTURE_2D));

  auto* gl = ContextGL();
  gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());

  // This is only used on Android where all video mailboxes already use shared
  // images.
  DCHECK(mailbox_holder.mailbox.IsSharedImage());

  // TODO(vikassoni): Use raster interface instead of gl interface eventually.
  GLuint src_texture_id =
      gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox_holder.mailbox.name);
  gl->BeginSharedImageAccessDirectCHROMIUM(
      src_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  {
    HardwarePlaneResource::ScopedTexture scope(gl, hardware_resource);
    gl->CopySubTextureCHROMIUM(
        src_texture_id, 0, GL_TEXTURE_2D, scope.texture_id(), 0, 0, 0, 0, 0,
        output_plane_resource_size.width(), output_plane_resource_size.height(),
        false, false, false);
  }
  gl->EndSharedImageAccessDirectCHROMIUM(src_texture_id);
  gl->DeleteTextures(1, &src_texture_id);

  // Pass an empty sync token to force generation of a new sync token.
  SyncTokenClientImpl client(gl, nullptr /* gpu::SharedImageInterface* */,
                             gpu::SyncToken());
  gpu::SyncToken sync_token = video_frame->UpdateReleaseSyncToken(&client);

  auto transferable_resource = viz::TransferableResource::MakeGpu(
      hardware_resource->mailbox(), GL_LINEAR, GL_TEXTURE_2D, sync_token,
      output_plane_resource_size, copy_resource_format,
      false /* is_overlay_candidate */);
  transferable_resource.color_space = resource_color_space;
  external_resources->resources.push_back(std::move(transferable_resource));

  external_resources->release_callbacks.push_back(base::BindOnce(
      &VideoResourceUpdater::RecycleResource, weak_ptr_factory_.GetWeakPtr(),
      hardware_resource->plane_resource_id()));
}

VideoFrameExternalResources VideoResourceUpdater::CreateForHardwarePlanes(
    scoped_refptr<VideoFrame> video_frame) {
  TRACE_EVENT0("cc", "VideoResourceUpdater::CreateForHardwarePlanes");
  DCHECK(video_frame->HasTextures());
  if (!context_provider_ && !raster_context_provider_)
    return VideoFrameExternalResources();

  VideoFrameExternalResources external_resources;
  gfx::ColorSpace resource_color_space = video_frame->ColorSpace();

  const bool copy_required = video_frame->metadata().copy_required;

  GLuint target = video_frame->mailbox_holder(0).texture_target;
  // If |copy_required| then we will copy into a GL_TEXTURE_2D target.
  if (copy_required)
    target = GL_TEXTURE_2D;

  gfx::BufferFormat buffer_formats[VideoFrame::kMaxPlanes];
  external_resources.type = ExternalResourceTypeForHardwarePlanes(
      *video_frame, target, buffer_formats, use_stream_video_draw_quad_);
  external_resources.bits_per_channel = video_frame->BitDepth();

  if (external_resources.type == VideoFrameResourceType::NONE) {
    DLOG(ERROR) << "Unsupported Texture format"
                << VideoPixelFormatToString(video_frame->format());
    return external_resources;
  }
  absl::optional<gfx::ColorSpace> resource_color_space_when_sampled;
  if (external_resources.type == VideoFrameResourceType::RGB ||
      external_resources.type == VideoFrameResourceType::RGBA ||
      external_resources.type == VideoFrameResourceType::RGBA_PREMULTIPLIED) {
    resource_color_space_when_sampled =
        resource_color_space.GetAsFullRangeRGB();
  }

  const size_t num_textures = video_frame->NumTextures();
  for (size_t i = 0; i < num_textures; ++i) {
    const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(i);
    if (mailbox_holder.mailbox.IsZero())
      break;

    if (copy_required) {
      CopyHardwarePlane(
          video_frame.get(),
          resource_color_space_when_sampled.value_or(resource_color_space),
          mailbox_holder, &external_resources);
    } else {
      const size_t width = video_frame->columns(i);
      const size_t height = video_frame->rows(i);
      const gfx::Size plane_size(width, height);
      auto transfer_resource = viz::TransferableResource::MakeGpu(
          mailbox_holder.mailbox, GL_LINEAR, mailbox_holder.texture_target,
          mailbox_holder.sync_token, plane_size,
          viz::GetResourceFormat(buffer_formats[i]),
          video_frame->metadata().allow_overlay);
      transfer_resource.color_space = resource_color_space;
      transfer_resource.color_space_when_sampled =
          resource_color_space_when_sampled;
      transfer_resource.hdr_metadata = video_frame->hdr_metadata();
      if (video_frame->metadata().read_lock_fences_enabled) {
        transfer_resource.synchronization_type = viz::TransferableResource::
            SynchronizationType::kGpuCommandsCompleted;
      }
      transfer_resource.ycbcr_info = video_frame->ycbcr_info();

#if BUILDFLAG(ENABLE_VULKAN)
      // Ensure that `ycbcr_info` is provided when necessary.
      // TODO(crbug.com/1399429): Avoid duplicating this logic.
      if ((transfer_resource.format.IsLegacyMultiplanar()) &&
          !transfer_resource.ycbcr_info) {
        VkSamplerYcbcrModelConversion ycbcr_conversion =
            (resource_color_space.GetMatrixID() ==
             gfx::ColorSpace::MatrixID::BT709)
                ? VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709
                : VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601;

        transfer_resource.ycbcr_info = gpu::VulkanYCbCrInfo(
            ToVkFormat(transfer_resource.format.resource_format()),
            /*external_format=*/0, ycbcr_conversion,
            VK_SAMPLER_YCBCR_RANGE_ITU_NARROW, VK_CHROMA_LOCATION_COSITED_EVEN,
            VK_CHROMA_LOCATION_COSITED_EVEN,
            /*format_features=*/0);
      }
#endif

#if BUILDFLAG(IS_ANDROID)
      transfer_resource.is_backed_by_surface_texture =
          video_frame->metadata().texture_owner;
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
      transfer_resource.wants_promotion_hint =
          video_frame->metadata().wants_promotion_hint;
#endif

      external_resources.resources.push_back(std::move(transfer_resource));
      external_resources.release_callbacks.push_back(
          base::BindOnce(&VideoResourceUpdater::ReturnTexture,
                         weak_ptr_factory_.GetWeakPtr(), video_frame));
    }
  }
  return external_resources;
}

VideoFrameExternalResources VideoResourceUpdater::CreateForSoftwarePlanes(
    scoped_refptr<VideoFrame> video_frame) {
  TRACE_EVENT0("cc", "VideoResourceUpdater::CreateForSoftwarePlanes");
  const VideoPixelFormat input_frame_format = video_frame->format();

  size_t bits_per_channel = video_frame->BitDepth();

  const bool is_rgb = input_frame_format == PIXEL_FORMAT_XBGR ||
                      input_frame_format == PIXEL_FORMAT_XRGB ||
                      input_frame_format == PIXEL_FORMAT_ABGR ||
                      input_frame_format == PIXEL_FORMAT_ARGB;

  DCHECK(IsYuvPlanar(input_frame_format) ||
         input_frame_format == PIXEL_FORMAT_Y16 || is_rgb);

  viz::ResourceFormat output_resource_format;
  absl::optional<viz::ResourceFormat> subplane_resource_format;
  gfx::ColorSpace output_color_space = video_frame->ColorSpace();
  bool texture_needs_rgb_conversion = false;
  if (is_rgb) {
    output_resource_format = GetRGBResourceFormat(input_frame_format);
  } else if (input_frame_format == PIXEL_FORMAT_Y16) {
    // Unable to display directly as yuv planes so convert it to RGB.
    texture_needs_rgb_conversion = true;
  } else if (!software_compositor()) {
    // Can be composited directly from yuv planes.
    output_resource_format = YuvResourceFormat(bits_per_channel);

    // If GPU compositing is enabled, but the output resource format returned by
    // the resource provider is viz::RGBA_8888, then a GPU driver bug
    // workaround requires that YUV frames must be converted to RGB before
    // texture upload.
    if (output_resource_format == viz::ResourceFormat::RGBA_8888 ||
        output_resource_format == viz::ResourceFormat::BGRA_8888) {
      texture_needs_rgb_conversion = true;
    }

    // Some YUV resources have different sized planes. If we lack the proper
    // ResourceFormat just convert to RGB. We could do something better like
    // unpacking to I420/I016, but texture_rg and r16 support should be pretty
    // universal and we expect these frames to be rare.
    if (input_frame_format == PIXEL_FORMAT_NV12) {
      if (output_resource_format == viz::RED_8)
        subplane_resource_format = viz::RG_88;
      else
        texture_needs_rgb_conversion = true;
    } else if (input_frame_format == PIXEL_FORMAT_P016LE) {
      if (output_resource_format == viz::R16_EXT)
        subplane_resource_format = viz::RG16_EXT;
      else
        texture_needs_rgb_conversion = true;
    } else {
      DCHECK_EQ(VideoFrame::BytesPerElement(input_frame_format, 0),
                VideoFrame::BytesPerElement(input_frame_format, 1));
    }
  }

  size_t output_plane_count = VideoFrame::NumPlanes(input_frame_format);

  // TODO(skaslev): If we're in software compositing mode, we do the YUV -> RGB
  // conversion here. That involves an extra copy of each frame to a bitmap.
  // Obviously, this is suboptimal and should be addressed once ubercompositor
  // starts shaping up.
  if (software_compositor() || texture_needs_rgb_conversion) {
    output_resource_format =
        software_compositor()
            ? viz::RGBA_8888
            : PaintCanvasVideoRenderer::GetRGBPixelsOutputFormat();
    output_plane_count = 1;
    bits_per_channel = 8;

    // The YUV to RGB conversion will be performed when we convert
    // from single-channel textures to an RGBA texture via
    // ConvertVideoFrameToRGBPixels below.
    output_color_space = output_color_space.GetAsFullRangeRGB();
  }

  std::vector<gfx::Size> outplane_plane_sizes;
  outplane_plane_sizes.reserve(output_plane_count);
  for (size_t i = 0; i < output_plane_count; ++i) {
    outplane_plane_sizes.push_back(
        SoftwarePlaneDimension(video_frame.get(), software_compositor(), i));
    const gfx::Size& output_plane_resource_size = outplane_plane_sizes.back();
    if (output_plane_resource_size.IsEmpty() ||
        output_plane_resource_size.width() > max_resource_size_ ||
        output_plane_resource_size.height() > max_resource_size_) {
      // This output plane has invalid geometry so return an empty external
      // resources.
      DLOG(ERROR)
          << "Video resource is too large to upload. Maximum dimension is "
          << max_resource_size_ << " and resource is "
          << output_plane_resource_size.ToString();
      return VideoFrameExternalResources();
    }
  }

  // Delete recycled resources that are the wrong format or wrong size.
  auto can_delete_resource_fn =
      [output_resource_format, subplane_resource_format,
       &outplane_plane_sizes](const std::unique_ptr<PlaneResource>& resource) {
        // Resources that are still being used can't be deleted.
        if (resource->has_refs())
          return false;

        return (resource->resource_format() != output_resource_format &&
                resource->resource_format() !=
                    subplane_resource_format.value_or(
                        output_resource_format)) ||
               !base::Contains(outplane_plane_sizes, resource->resource_size());
      };
  base::EraseIf(all_resources_, can_delete_resource_fn);

  // Recycle or allocate resources for each video plane.
  std::vector<PlaneResource*> plane_resources;
  plane_resources.reserve(output_plane_count);
  for (size_t i = 0; i < output_plane_count; ++i) {
    plane_resources.push_back(RecycleOrAllocateResource(
        outplane_plane_sizes[i],
        i == 0 ? output_resource_format
               : subplane_resource_format.value_or(output_resource_format),
        output_color_space, video_frame->unique_id(), i));
    plane_resources.back()->add_ref();
  }

  VideoFrameExternalResources external_resources;

  external_resources.bits_per_channel = bits_per_channel;

  if (software_compositor() || texture_needs_rgb_conversion || is_rgb) {
    DCHECK_EQ(plane_resources.size(), 1u);
    PlaneResource* plane_resource = plane_resources[0];

    if (!plane_resource->Matches(video_frame->unique_id(), 0)) {
      // We need to transfer data from |video_frame| to the plane resource.
      if (software_compositor()) {
        DCHECK_EQ(plane_resource->resource_format(), viz::RGBA_8888);

        if (!video_renderer_)
          video_renderer_ = std::make_unique<PaintCanvasVideoRenderer>();

        SoftwarePlaneResource* software_resource = plane_resource->AsSoftware();

        // We know the format is RGBA_8888 from check above.
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

        // Note that PaintCanvasVideoRenderer::Copy would copy to the origin,
        // not |video_frame->visible_rect|, so call Paint instead.
        // https://crbug.com/1090435
        video_renderer_->Paint(video_frame, &canvas,
                               gfx::RectF(video_frame->visible_rect()), flags,
                               media::kNoTransformation, nullptr);
      } else {
        HardwarePlaneResource* hardware_resource = plane_resource->AsHardware();
        size_t bytes_per_row = viz::ResourceSizes::CheckedWidthInBytes<size_t>(
            video_frame->coded_size().width(), output_resource_format);
        const gfx::Size& plane_size = hardware_resource->resource_size();

        // Note: Strides may be negative in case of bottom-up layouts.
        const int stride = video_frame->stride(VideoFrame::kARGBPlane);
        const bool has_compatible_stride =
            stride > 0 && static_cast<size_t>(stride) == bytes_per_row;

        const uint8_t* source_pixels = nullptr;
        if (HasCompatibleFormat(input_frame_format, output_resource_format) &&
            has_compatible_stride) {
          // We can passthrough when the texture format matches. Since we
          // always copy the entire coded area we don't have to worry about
          // origin.
          source_pixels = video_frame->data(VideoFrame::kARGBPlane);
        } else {
          size_t needed_size =
              bytes_per_row * video_frame->coded_size().height();
          if (upload_pixels_size_ < needed_size) {
            if (!ReallocateUploadPixels(needed_size)) {
              // Fail here if memory reallocation fails.
              return VideoFrameExternalResources();
            }
          }

          // PCVR writes to origin, so offset upload pixels by start since
          // we upload frames in coded size and pass on the visible rect to
          // the compositor. Note: It'd save a few bytes not to do this...
          auto* dest_ptr = upload_pixels_.get() +
                           video_frame->visible_rect().y() * bytes_per_row +
                           video_frame->visible_rect().x() * sizeof(uint32_t);
          PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
              video_frame.get(), dest_ptr, bytes_per_row);
          source_pixels = upload_pixels_.get();
        }

        // Copy pixels into texture.
        auto* gl = ContextGL();
        {
          HardwarePlaneResource::ScopedTexture scope(gl, hardware_resource);
          gl->BindTexture(hardware_resource->texture_target(),
                          scope.texture_id());
          gl->TexSubImage2D(hardware_resource->texture_target(), 0, 0, 0,
                            plane_size.width(), plane_size.height(),
                            GLDataFormat(output_resource_format),
                            GLDataType(output_resource_format), source_pixels);
        }
      }
      plane_resource->SetUniqueId(video_frame->unique_id(), 0);
    }

    viz::TransferableResource transferable_resource;
    if (software_compositor()) {
      SoftwarePlaneResource* software_resource = plane_resource->AsSoftware();
      external_resources.type = VideoFrameResourceType::RGBA_PREMULTIPLIED;
      transferable_resource = viz::TransferableResource::MakeSoftware(
          software_resource->shared_bitmap_id(),
          software_resource->resource_size(),
          plane_resource->resource_format());
    } else {
      HardwarePlaneResource* hardware_resource = plane_resource->AsHardware();
      external_resources.type = VideoFrameResourceType::RGBA;
      gpu::SyncToken sync_token;
      auto* gl = ContextGL();
      GenerateCompositorSyncToken(gl, &sync_token);
      transferable_resource = viz::TransferableResource::MakeGpu(
          hardware_resource->mailbox(), GL_LINEAR,
          hardware_resource->texture_target(), sync_token,
          hardware_resource->resource_size(), output_resource_format,
          hardware_resource->overlay_candidate());
    }

    transferable_resource.color_space = output_color_space;
    external_resources.resources.push_back(std::move(transferable_resource));
    external_resources.release_callbacks.push_back(base::BindOnce(
        &VideoResourceUpdater::RecycleResource, weak_ptr_factory_.GetWeakPtr(),
        plane_resource->plane_resource_id()));

    return external_resources;
  }

  const auto yuv_resource_format = output_resource_format;
  DCHECK(yuv_resource_format == viz::LUMINANCE_F16 ||
         yuv_resource_format == viz::R16_EXT ||
         yuv_resource_format == viz::LUMINANCE_8 ||
         yuv_resource_format == viz::RED_8)
      << yuv_resource_format;

  std::unique_ptr<HalfFloatMaker> half_float_maker;
  if (yuv_resource_format == viz::LUMINANCE_F16) {
    half_float_maker = HalfFloatMaker::NewHalfFloatMaker(bits_per_channel);
    external_resources.offset = half_float_maker->Offset();
    external_resources.multiplier = half_float_maker->Multiplier();
  } else if (yuv_resource_format == viz::R16_EXT) {
    external_resources.multiplier = 65535.0f / ((1 << bits_per_channel) - 1);
    external_resources.offset = 0;
  }

  auto* gl = ContextGL();
  // We need to transfer data from |video_frame| to the plane resources.
  for (size_t i = 0; i < plane_resources.size(); ++i) {
    HardwarePlaneResource* plane_resource = plane_resources[i]->AsHardware();

    // Skip the transfer if this |video_frame|'s plane has been processed.
    if (plane_resource->Matches(video_frame->unique_id(), i))
      continue;

    const viz::ResourceFormat plane_resource_format =
        plane_resource->resource_format();
    DCHECK(plane_resource_format == yuv_resource_format ||
           plane_resource_format ==
               subplane_resource_format.value_or(yuv_resource_format));

    // |video_stride_bytes| is the width of the |video_frame| we are uploading
    // (including non-frame data to fill in the stride).
    const int video_stride_bytes = video_frame->stride(i);

    // |resource_size_pixels| is the size of the destination resource.
    const gfx::Size resource_size_pixels = plane_resource->resource_size();

    const size_t bytes_per_row =
        viz::ResourceSizes::CheckedWidthInBytes<size_t>(
            resource_size_pixels.width(), plane_resource_format);

    // Use 4-byte row alignment (OpenGL default) for upload performance.
    // Assuming that GL_UNPACK_ALIGNMENT has not changed from default.
    constexpr size_t kDefaultUnpackAlignment = 4;
    const size_t upload_image_stride = cc::MathUtil::CheckedRoundUp<size_t>(
        bytes_per_row, kDefaultUnpackAlignment);

    const size_t resource_bit_depth =
        static_cast<size_t>(viz::BitsPerPixel(plane_resource_format));

    // Data downshifting is needed if the resource bit depth is not enough.
    const bool needs_bit_downshifting = bits_per_channel > resource_bit_depth;

    // We need to convert the incoming data if we're transferring to half float,
    // if the need a bit downshift or if the strides need to be reconciled.
    const bool needs_conversion =
        plane_resource_format == viz::LUMINANCE_F16 || needs_bit_downshifting;

    constexpr size_t kDefaultUnpackRowLength = 0;
    GLuint unpack_row_length = kDefaultUnpackRowLength;
    GLuint unpack_alignment = kDefaultUnpackAlignment;

    const uint8_t* pixels;

    if (!needs_conversion) {
      // Stride adaptation is needed if source and destination strides are
      // different but they have the same bit depth.
      const bool needs_stride_adaptation =
          (bits_per_channel == resource_bit_depth) &&
          (upload_image_stride != static_cast<size_t>(video_stride_bytes));
      if (needs_stride_adaptation) {
        const int bytes_per_element =
            VideoFrame::BytesPerElement(video_frame->format(), i);
        // Stride is aligned to VideoFrameLayout::kFrameAddressAlignment (32)
        // which should be divisible by pixel size for YUV formats (1, 2 or 4).
        DCHECK_EQ(video_stride_bytes % bytes_per_element, 0);
        // Unpack row length is in pixels not bytes.
        unpack_row_length = video_stride_bytes / bytes_per_element;
        // Use a non-standard alignment only if necessary.
        if (video_stride_bytes % kDefaultUnpackAlignment != 0)
          unpack_alignment = bytes_per_element;
      }
      pixels = video_frame->data(i);
    } else {
      // Avoid malloc for each frame/plane if possible.
      const size_t needed_size =
          upload_image_stride * resource_size_pixels.height();
      if (upload_pixels_size_ < needed_size) {
        if (!ReallocateUploadPixels(needed_size)) {
          // Fail here if memory reallocation fails.
          return VideoFrameExternalResources();
        }
      }

      if (plane_resource_format == viz::LUMINANCE_F16) {
        for (int row = 0; row < resource_size_pixels.height(); ++row) {
          uint16_t* dst = reinterpret_cast<uint16_t*>(
              &upload_pixels_[upload_image_stride * row]);
          const uint16_t* src = reinterpret_cast<uint16_t*>(
              video_frame->writable_data(i) + (video_stride_bytes * row));
          half_float_maker->MakeHalfFloats(src, bytes_per_row / 2, dst);
        }
      } else if (needs_bit_downshifting) {
        DCHECK(plane_resource_format == viz::LUMINANCE_8 ||
               plane_resource_format == viz::RED_8);
        const int scale = 0x10000 >> (bits_per_channel - 8);
        libyuv::Convert16To8Plane(
            reinterpret_cast<uint16_t*>(video_frame->writable_data(i)),
            video_stride_bytes / 2, upload_pixels_.get(), upload_image_stride,
            scale, bytes_per_row, resource_size_pixels.height());
      } else {
        NOTREACHED();
      }

      pixels = upload_pixels_.get();
    }

    // Copy pixels into texture. TexSubImage2D() is applicable because
    // |yuv_resource_format| is LUMINANCE_F16, R16_EXT, LUMINANCE_8 or RED_8.
    DCHECK(GLSupportsFormat(plane_resource_format));
    {
      HardwarePlaneResource::ScopedTexture scope(gl, plane_resource);

      gl->BindTexture(plane_resource->texture_target(), scope.texture_id());

      gl->PixelStorei(GL_UNPACK_ROW_LENGTH, unpack_row_length);
      gl->PixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);
      gl->TexSubImage2D(plane_resource->texture_target(), 0, 0, 0,
                        resource_size_pixels.width(),
                        resource_size_pixels.height(),
                        GLDataFormat(plane_resource_format),
                        GLDataType(plane_resource_format), pixels);
      gl->PixelStorei(GL_UNPACK_ROW_LENGTH, kDefaultUnpackRowLength);
      gl->PixelStorei(GL_UNPACK_ALIGNMENT, kDefaultUnpackAlignment);
    }

    plane_resource->SetUniqueId(video_frame->unique_id(), i);
  }

  // Set the sync token otherwise resource is assumed to be synchronized.
  gpu::SyncToken sync_token;
  GenerateCompositorSyncToken(gl, &sync_token);

  for (size_t i = 0; i < plane_resources.size(); ++i) {
    HardwarePlaneResource* plane_resource = plane_resources[i]->AsHardware();
    auto transferable_resource = viz::TransferableResource::MakeGpu(
        plane_resource->mailbox(), GL_LINEAR, plane_resource->texture_target(),
        sync_token, plane_resource->resource_size(),
        i == 0 ? output_resource_format
               : subplane_resource_format.value_or(output_resource_format),
        plane_resource->overlay_candidate());
    transferable_resource.color_space = output_color_space;
    external_resources.resources.push_back(std::move(transferable_resource));
    external_resources.release_callbacks.push_back(base::BindOnce(
        &VideoResourceUpdater::RecycleResource, weak_ptr_factory_.GetWeakPtr(),
        plane_resource->plane_resource_id()));
  }

  external_resources.type = VideoFrameResourceType::YUV;
  return external_resources;
}

gpu::gles2::GLES2Interface* VideoResourceUpdater::ContextGL() {
  auto* gl = raster_context_provider_ ? raster_context_provider_->ContextGL()
                                      : context_provider_->ContextGL();
  DCHECK(gl);
  return gl;
}

void VideoResourceUpdater::ReturnTexture(scoped_refptr<VideoFrame> video_frame,
                                         const gpu::SyncToken& sync_token,
                                         bool lost_resource) {
  // TODO(dshwang): Forward to the decoder as a lost resource.
  if (lost_resource)
    return;

  if (!sync_token.HasData())
    return;

  // The video frame will insert a wait on the previous release sync token.
  SyncTokenClientImpl client(
      ContextGL(), nullptr /* gpu::SharedImageInterface* */, sync_token);
  video_frame->UpdateReleaseSyncToken(&client);
}

void VideoResourceUpdater::RecycleResource(uint32_t plane_resource_id,
                                           const gpu::SyncToken& sync_token,
                                           bool lost_resource) {
  auto resource_it = base::ranges::find(all_resources_, plane_resource_id,
                                        &PlaneResource::plane_resource_id);
  if (resource_it == all_resources_.end())
    return;

  if ((raster_context_provider_ || context_provider_) && sync_token.HasData()) {
    ContextGL()->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
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
        viz::ResourceSizes::UncheckedSizeInBytesAligned<uint64_t>(
            resource->resource_size(), resource->resource_format());
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

VideoResourceUpdater::FrameResource::FrameResource() = default;

VideoResourceUpdater::FrameResource::FrameResource(viz::ResourceId id,
                                                   const gfx::Size& size)
    : id(id), size_in_pixels(size) {}

}  // namespace media
