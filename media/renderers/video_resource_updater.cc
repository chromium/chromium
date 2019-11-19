// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_resource_updater.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/bit_cast.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
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
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/video_frame.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/half_float_maker.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/trace_util.h"

namespace media {
namespace {

// Generates process-unique IDs to use for tracing video resources.
base::AtomicSequenceNumber g_next_video_resource_updater_id;

VideoFrameResourceType ExternalResourceTypeForHardwarePlanes(
    VideoPixelFormat format,
    GLuint target,
    int num_textures,
    gfx::BufferFormat buffer_formats[VideoFrame::kMaxPlanes],
    bool use_stream_video_draw_quad) {
  switch (format) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_BGRA:
      DCHECK_EQ(num_textures, 1);
      // This maps VideoPixelFormat back to GMB BufferFormat
      // NOTE: ABGR == RGBA and ARGB == BGRA, they differ only byte order
      // See: VideoFormat function in gpu_memory_buffer_video_frame_pool
      // https://cs.chromium.org/chromium/src/media/video/gpu_memory_buffer_video_frame_pool.cc?type=cs&g=0&l=281
      buffer_formats[0] = (format == PIXEL_FORMAT_ABGR)
                              ? gfx::BufferFormat::RGBA_8888
                              : gfx::BufferFormat::BGRA_8888;
      switch (target) {
        case GL_TEXTURE_EXTERNAL_OES:
          if (use_stream_video_draw_quad)
            return VideoFrameResourceType::STREAM_TEXTURE;
          FALLTHROUGH;
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
                              ? gfx::BufferFormat::BGRX_1010102
                              : gfx::BufferFormat::RGBX_1010102;
      return VideoFrameResourceType::RGB;
    case PIXEL_FORMAT_I420:
      DCHECK_EQ(num_textures, 3);
      buffer_formats[0] = gfx::BufferFormat::R_8;
      buffer_formats[1] = gfx::BufferFormat::R_8;
      buffer_formats[2] = gfx::BufferFormat::R_8;
      return VideoFrameResourceType::YUV;

    case PIXEL_FORMAT_NV12:
      // |target| is set to 0 for Vulkan textures.
      DCHECK(target == 0 || target == GL_TEXTURE_EXTERNAL_OES ||
             target == GL_TEXTURE_2D || target == GL_TEXTURE_RECTANGLE_ARB)
          << "Unsupported target " << gl::GLEnums::GetStringEnum(target);

      if (num_textures == 1) {
        // Single-texture multi-planar frames can be sampled as RGB.
        buffer_formats[0] = gfx::BufferFormat::YUV_420_BIPLANAR;
        return VideoFrameResourceType::RGB;
      }

      buffer_formats[0] = gfx::BufferFormat::R_8;
      buffer_formats[1] = gfx::BufferFormat::RG_88;
      return VideoFrameResourceType::YUV;

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
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_P016LE:
    case PIXEL_FORMAT_UNKNOWN:
      break;
  }
  return VideoFrameResourceType::NONE;
}

class SyncTokenClientImpl : public VideoFrame::SyncTokenClient {
 public:
  SyncTokenClientImpl(gpu::gles2::GLES2Interface* gl, gpu::SyncToken sync_token)
      : gl_(gl), sync_token_(sync_token) {}
  ~SyncTokenClientImpl() override = default;

  void GenerateSyncToken(gpu::SyncToken* sync_token) override {
    if (sync_token_.HasData()) {
      *sync_token = sync_token_;
    } else {
      gl_->GenSyncTokenCHROMIUM(sync_token->GetData());
    }
  }

  void WaitSyncToken(const gpu::SyncToken& sync_token) override {
    if (sync_token.HasData()) {
      gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
      if (sync_token_.HasData() && sync_token_ != sync_token) {
        gl_->WaitSyncTokenCHROMIUM(sync_token_.GetConstData());
        sync_token_.Clear();
      }
    }
  }

 private:
  gpu::gles2::GLES2Interface* gl_;
  gpu::SyncToken sync_token_;
  DISALLOW_COPY_AND_ASSIGN(SyncTokenClientImpl);
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
  gfx::Size coded_size = input_frame->coded_size();
  if (software_compositor)
    return coded_size;

  int plane_width = VideoFrame::Columns(plane_index, input_frame->format(),
                                        coded_size.width());
  int plane_height =
      VideoFrame::Rows(plane_index, input_frame->format(), coded_size.height());
  return gfx::Size(plane_width, plane_height);
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
  virtual ~PlaneResource() = default;

  // Casts |this| to SoftwarePlaneResource for software compositing.
  SoftwarePlaneResource* AsSoftware();

  // Casts |this| to HardwarePlaneResource for GPU compositing.
  HardwarePlaneResource* AsHardware();

  // Returns true if this resource matches the unique identifiers of another
  // VideoFrame resource.
  bool Matches(int unique_frame_id, size_t plane_index) {
    return has_unique_frame_id_and_plane_index_ &&
           unique_frame_id_ == unique_frame_id && plane_index_ == plane_index;
  }

  // Sets the unique identifiers for this resource, may only be called when
  // there is a single reference to the resource (i.e. |ref_count_| == 1).
  void SetUniqueId(int unique_frame_id, size_t plane_index) {
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
  int unique_frame_id_ = 0;
  size_t plane_index_ = 0u;
  // Indicates if the above two members have been set or not.
  bool has_unique_frame_id_and_plane_index_ = false;

  DISALLOW_COPY_AND_ASSIGN(PlaneResource);
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
  viz::SharedBitmapReporter* const shared_bitmap_reporter_;
  const viz::SharedBitmapId shared_bitmap_id_;
  base::WritableSharedMemoryMapping shared_mapping_;

  DISALLOW_COPY_AND_ASSIGN(SoftwarePlaneResource);
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
    gpu::gles2::GLES2Interface* gl_;
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
                         caps.texture_storage_image &&
                         IsGpuMemoryBufferFormatSupported(format);
    uint32_t shared_image_usage =
        gpu::SHARED_IMAGE_USAGE_GLES2 | gpu::SHARED_IMAGE_USAGE_DISPLAY;
    if (overlay_candidate_) {
      shared_image_usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
      texture_target_ = gpu::GetBufferTextureTarget(gfx::BufferUsage::SCANOUT,
                                                    BufferFormat(format), caps);
    }
    auto* sii = SharedImageInterface();
    mailbox_ =
        sii->CreateSharedImage(format, size, color_space, shared_image_usage);
    ContextGL()->WaitSyncTokenCHROMIUM(
        sii->GenUnverifiedSyncToken().GetConstData());
  }

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

  viz::ContextProvider* const context_provider_;
  viz::RasterContextProvider* const raster_context_provider_;
  gpu::Mailbox mailbox_;
  GLenum texture_target_ = GL_TEXTURE_2D;
  bool overlay_candidate_ = false;

  DISALLOW_COPY_AND_ASSIGN(HardwarePlaneResource);
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
      this, "media::VideoResourceUpdater", base::ThreadTaskRunnerHandle::Get());
}

VideoResourceUpdater::~VideoResourceUpdater() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void VideoResourceUpdater::ObtainFrameResources(
    scoped_refptr<VideoFrame> video_frame) {
  if (video_frame->metadata()->GetUnguessableToken(
          VideoFrameMetadata::OVERLAY_PLANE_ID, &overlay_plane_id_)) {
    // This is a hole punching VideoFrame, there is nothing to display.
    frame_resource_type_ = VideoFrameResourceType::VIDEO_HOLE;
    return;
  }

  VideoFrameExternalResources external_resources =
      CreateExternalResourcesFromVideoFrame(video_frame);
  frame_resource_type_ = external_resources.type;

  if (external_resources.type == VideoFrameResourceType::YUV) {
    frame_resource_offset_ = external_resources.offset;
    frame_resource_multiplier_ = external_resources.multiplier;
    frame_bits_per_channel_ = external_resources.bits_per_channel;
  }

  DCHECK_EQ(external_resources.resources.size(),
            external_resources.release_callbacks.size());
  for (size_t i = 0; i < external_resources.resources.size(); ++i) {
    viz::ResourceId resource_id = resource_provider_->ImportResource(
        external_resources.resources[i],
        viz::SingleReleaseCallback::Create(
            std::move(external_resources.release_callbacks[i])));
    frame_resources_.push_back(
        {resource_id, external_resources.resources[i].size});
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

void VideoResourceUpdater::AppendQuads(viz::RenderPass* render_pass,
                                       scoped_refptr<VideoFrame> frame,
                                       gfx::Transform transform,
                                       gfx::Rect quad_rect,
                                       gfx::Rect visible_quad_rect,
                                       const gfx::RRectF& rounded_corner_bounds,
                                       gfx::Rect clip_rect,
                                       bool is_clipped,
                                       bool contents_opaque,
                                       float draw_opacity,
                                       int sorting_context_id) {
  DCHECK(frame.get());

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(transform, quad_rect, visible_quad_rect,
                            rounded_corner_bounds, clip_rect, is_clipped,
                            contents_opaque, draw_opacity,
                            SkBlendMode::kSrcOver, sorting_context_id);

  bool needs_blending = !contents_opaque;

  gfx::Rect visible_rect = frame->visible_rect();
  gfx::Size coded_size = frame->coded_size();

  const float tex_width_scale =
      static_cast<float>(visible_rect.width()) / coded_size.width();
  const float tex_height_scale =
      static_cast<float>(visible_rect.height()) / coded_size.height();

  const gfx::PointF uv_top_left(0.f, 0.f);
  const gfx::PointF uv_bottom_right(tex_width_scale, tex_height_scale);

  switch (frame_resource_type_) {
    case VideoFrameResourceType::VIDEO_HOLE: {
      auto* video_hole_quad =
          render_pass->CreateAndAppendDrawQuad<viz::VideoHoleDrawQuad>();
      video_hole_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                              overlay_plane_id_);
      break;
    }
    case VideoFrameResourceType::YUV: {
      const gfx::Size ya_tex_size = coded_size;

      int u_width = VideoFrame::Columns(VideoFrame::kUPlane, frame->format(),
                                        coded_size.width());
      int u_height = VideoFrame::Rows(VideoFrame::kUPlane, frame->format(),
                                      coded_size.height());
      gfx::Size uv_tex_size(u_width, u_height);

      DCHECK_EQ(frame_resources_.size(),
                VideoFrame::NumPlanes(frame->format()));
      if (frame->HasTextures()) {
        DCHECK(frame->format() == PIXEL_FORMAT_NV12 ||
               frame->format() == PIXEL_FORMAT_I420);
      }

      // Compute the UV sub-sampling factor based on the ratio between
      // |ya_tex_size| and |uv_tex_size|.
      float uv_subsampling_factor_x =
          static_cast<float>(ya_tex_size.width()) / uv_tex_size.width();
      float uv_subsampling_factor_y =
          static_cast<float>(ya_tex_size.height()) / uv_tex_size.height();
      gfx::RectF ya_tex_coord_rect(visible_rect);
      gfx::RectF uv_tex_coord_rect(
          visible_rect.x() / uv_subsampling_factor_x,
          visible_rect.y() / uv_subsampling_factor_y,
          visible_rect.width() / uv_subsampling_factor_x,
          visible_rect.height() / uv_subsampling_factor_y);

      auto* yuv_video_quad =
          render_pass->CreateAndAppendDrawQuad<viz::YUVVideoDrawQuad>();
      yuv_video_quad->SetNew(
          shared_quad_state, quad_rect, visible_quad_rect, needs_blending,
          ya_tex_coord_rect, uv_tex_coord_rect, ya_tex_size, uv_tex_size,
          frame_resources_[0].id, frame_resources_[1].id,
          frame_resources_.size() > 2 ? frame_resources_[2].id
                                      : frame_resources_[1].id,
          frame_resources_.size() > 3 ? frame_resources_[3].id : 0,
          frame->ColorSpace(), frame_resource_offset_,
          frame_resource_multiplier_, frame_bits_per_channel_);
      if (frame->metadata()->IsTrue(VideoFrameMetadata::PROTECTED_VIDEO)) {
        if (frame->metadata()->IsTrue(VideoFrameMetadata::HW_PROTECTED)) {
          yuv_video_quad->protected_video_type =
              gfx::ProtectedVideoType::kHardwareProtected;
        } else {
          yuv_video_quad->protected_video_type =
              gfx::ProtectedVideoType::kSoftwareProtected;
        }
      }

      for (viz::ResourceId resource_id : yuv_video_quad->resources) {
        resource_provider_->ValidateResource(resource_id);
      }
      break;
    }
    case VideoFrameResourceType::RGBA:
    case VideoFrameResourceType::RGBA_PREMULTIPLIED:
    case VideoFrameResourceType::RGB: {
      DCHECK_EQ(frame_resources_.size(), 1u);
      if (frame_resources_.size() < 1u)
        break;
      bool premultiplied_alpha =
          frame_resource_type_ == VideoFrameResourceType::RGBA_PREMULTIPLIED;

      float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
      bool flipped = false;
      bool nearest_neighbor = false;
      gfx::ProtectedVideoType protected_video_type =
          gfx::ProtectedVideoType::kClear;
      if (frame->metadata()->IsTrue(VideoFrameMetadata::PROTECTED_VIDEO)) {
        if (frame->metadata()->IsTrue(VideoFrameMetadata::HW_PROTECTED))
          protected_video_type = gfx::ProtectedVideoType::kHardwareProtected;
        else
          protected_video_type = gfx::ProtectedVideoType::kSoftwareProtected;
      }

      auto* texture_quad =
          render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
      texture_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                           needs_blending, frame_resources_[0].id,
                           premultiplied_alpha, uv_top_left, uv_bottom_right,
                           SK_ColorTRANSPARENT, opacity, flipped,
                           nearest_neighbor, false, protected_video_type);
      texture_quad->set_resource_size_in_pixels(coded_size);
      for (viz::ResourceId resource_id : texture_quad->resources) {
        resource_provider_->ValidateResource(resource_id);
      }

      break;
    }
    case VideoFrameResourceType::STREAM_TEXTURE: {
      DCHECK_EQ(frame_resources_.size(), 1u);
      if (frame_resources_.size() < 1u)
        break;
      auto* stream_video_quad =
          render_pass->CreateAndAppendDrawQuad<viz::StreamVideoDrawQuad>();
      stream_video_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                                needs_blending, frame_resources_[0].id,
                                frame_resources_[0].size_in_pixels, uv_top_left,
                                uv_bottom_right);
      for (viz::ResourceId resource_id : stream_video_quad->resources) {
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
    return viz::RGBA_8888;
  if (bits_per_channel <= 8)
    return caps.texture_rg ? viz::RED_8 : viz::LUMINANCE_8;
  if (use_r16_texture_ && caps.texture_norm16)
    return viz::R16_EXT;
  if (caps.texture_half_float_linear)
    return viz::LUMINANCE_F16;
  return viz::LUMINANCE_8;
}

VideoResourceUpdater::PlaneResource*
VideoResourceUpdater::RecycleOrAllocateResource(
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    const gfx::ColorSpace& color_space,
    int unique_id,
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

  const int no_unique_id = 0;
  const int no_plane_index = -1;  // Do not recycle referenced textures.
  PlaneResource* plane_resource = RecycleOrAllocateResource(
      output_plane_resource_size, copy_resource_format, resource_color_space,
      no_unique_id, no_plane_index);
  HardwarePlaneResource* hardware_resource = plane_resource->AsHardware();
  hardware_resource->add_ref();

  DCHECK_EQ(hardware_resource->texture_target(),
            static_cast<GLenum>(GL_TEXTURE_2D));

  auto* gl = raster_context_provider_ ? raster_context_provider_->ContextGL()
                                      : context_provider_->ContextGL();

  gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
  // TODO(piman): convert to CreateAndTexStorage2DSharedImageCHROMIUM once
  // VideoFrame is all converted to SharedImage.
  GLuint src_texture_id =
      gl->CreateAndConsumeTextureCHROMIUM(mailbox_holder.mailbox.name);
  {
    HardwarePlaneResource::ScopedTexture scope(gl, hardware_resource);
    gl->CopySubTextureCHROMIUM(
        src_texture_id, 0, GL_TEXTURE_2D, scope.texture_id(), 0, 0, 0, 0, 0,
        output_plane_resource_size.width(), output_plane_resource_size.height(),
        false, false, false);
  }
  gl->DeleteTextures(1, &src_texture_id);

  // Pass an empty sync token to force generation of a new sync token.
  SyncTokenClientImpl client(gl, gpu::SyncToken());
  gpu::SyncToken sync_token = video_frame->UpdateReleaseSyncToken(&client);

  auto transferable_resource = viz::TransferableResource::MakeGL(
      hardware_resource->mailbox(), GL_LINEAR, GL_TEXTURE_2D, sync_token,
      output_plane_resource_size, false /* is_overlay_candidate */);
  transferable_resource.color_space = resource_color_space;
  transferable_resource.format = copy_resource_format;
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

  bool copy_required =
      video_frame->metadata()->IsTrue(VideoFrameMetadata::COPY_REQUIRED);

  GLuint target = video_frame->mailbox_holder(0).texture_target;
  // If |copy_required| then we will copy into a GL_TEXTURE_2D target.
  if (copy_required)
    target = GL_TEXTURE_2D;

  gfx::BufferFormat buffer_formats[VideoFrame::kMaxPlanes];
  external_resources.type = ExternalResourceTypeForHardwarePlanes(
      video_frame->format(), target, video_frame->NumTextures(), buffer_formats,
      use_stream_video_draw_quad_);

  if (external_resources.type == VideoFrameResourceType::NONE) {
    DLOG(ERROR) << "Unsupported Texture format"
                << VideoPixelFormatToString(video_frame->format());
    return external_resources;
  }
  if (external_resources.type == VideoFrameResourceType::RGB ||
      external_resources.type == VideoFrameResourceType::RGBA ||
      external_resources.type == VideoFrameResourceType::RGBA_PREMULTIPLIED) {
    resource_color_space = resource_color_space.GetAsFullRangeRGB();
  }

  const size_t num_textures = video_frame->NumTextures();
  for (size_t i = 0; i < num_textures; ++i) {
    const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(i);
    if (mailbox_holder.mailbox.IsZero())
      break;

    if (copy_required) {
      CopyHardwarePlane(video_frame.get(), resource_color_space, mailbox_holder,
                        &external_resources);
    } else {
      const gfx::Size& coded_size = video_frame->coded_size();
      const size_t width =
          VideoFrame::Columns(i, video_frame->format(), coded_size.width());
      const size_t height =
          VideoFrame::Rows(i, video_frame->format(), coded_size.height());
      const gfx::Size plane_size(width, height);
      auto transfer_resource = viz::TransferableResource::MakeGL(
          mailbox_holder.mailbox, GL_LINEAR, mailbox_holder.texture_target,
          mailbox_holder.sync_token, plane_size,
          video_frame->metadata()->IsTrue(VideoFrameMetadata::ALLOW_OVERLAY));
      transfer_resource.color_space = resource_color_space;
      transfer_resource.read_lock_fences_enabled =
          video_frame->metadata()->IsTrue(
              VideoFrameMetadata::READ_LOCK_FENCES_ENABLED);
      transfer_resource.format = viz::GetResourceFormat(buffer_formats[i]);
      transfer_resource.ycbcr_info = video_frame->ycbcr_info();

#if defined(OS_ANDROID)
      transfer_resource.is_backed_by_surface_texture =
          video_frame->metadata()->IsTrue(VideoFrameMetadata::TEXTURE_OWNER);
      transfer_resource.wants_promotion_hint = video_frame->metadata()->IsTrue(
          VideoFrameMetadata::WANTS_PROMOTION_HINT);
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

  // Only YUV and Y16 software video frames are supported.
  DCHECK(IsYuvPlanar(input_frame_format) ||
         input_frame_format == PIXEL_FORMAT_Y16);

  viz::ResourceFormat output_resource_format;
  gfx::ColorSpace output_color_space = video_frame->ColorSpace();
  if (input_frame_format == PIXEL_FORMAT_Y16) {
    // Unable to display directly as yuv planes so convert it to RGBA for
    // compositing.
    output_resource_format = viz::RGBA_8888;
    output_color_space = output_color_space.GetAsFullRangeRGB();
  } else if (!software_compositor()) {
    // Can be composited directly from yuv planes.
    output_resource_format = YuvResourceFormat(bits_per_channel);
  }

  // If GPU compositing is enabled, but the output resource format
  // returned by the resource provider is viz::RGBA_8888, then a GPU driver
  // bug workaround requires that YUV frames must be converted to RGB
  // before texture upload.
  bool texture_needs_rgb_conversion =
      !software_compositor() &&
      output_resource_format == viz::ResourceFormat::RGBA_8888;

  size_t output_plane_count = VideoFrame::NumPlanes(input_frame_format);

  // TODO(skaslev): If we're in software compositing mode, we do the YUV -> RGB
  // conversion here. That involves an extra copy of each frame to a bitmap.
  // Obviously, this is suboptimal and should be addressed once ubercompositor
  // starts shaping up.
  if (software_compositor() || texture_needs_rgb_conversion) {
    output_resource_format = viz::RGBA_8888;
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
      return VideoFrameExternalResources();
    }
  }

  // Delete recycled resources that are the wrong format or wrong size.
  auto can_delete_resource_fn =
      [output_resource_format,
       &outplane_plane_sizes](const std::unique_ptr<PlaneResource>& resource) {
        // Resources that are still being used can't be deleted.
        if (resource->has_refs())
          return false;

        return resource->resource_format() != output_resource_format ||
               !base::Contains(outplane_plane_sizes, resource->resource_size());
      };
  base::EraseIf(all_resources_, can_delete_resource_fn);

  // Recycle or allocate resources for each video plane.
  std::vector<PlaneResource*> plane_resources;
  plane_resources.reserve(output_plane_count);
  for (size_t i = 0; i < output_plane_count; ++i) {
    plane_resources.push_back(RecycleOrAllocateResource(
        outplane_plane_sizes[i], output_resource_format, output_color_space,
        video_frame->unique_id(), i));
    plane_resources.back()->add_ref();
  }

  VideoFrameExternalResources external_resources;

  external_resources.bits_per_channel = bits_per_channel;

  if (software_compositor() || texture_needs_rgb_conversion) {
    DCHECK_EQ(plane_resources.size(), 1u);
    PlaneResource* plane_resource = plane_resources[0];
    DCHECK_EQ(plane_resource->resource_format(), viz::RGBA_8888);

    if (!plane_resource->Matches(video_frame->unique_id(), 0)) {
      // We need to transfer data from |video_frame| to the plane resource.
      if (software_compositor()) {
        if (!video_renderer_)
          video_renderer_ = std::make_unique<PaintCanvasVideoRenderer>();

        SoftwarePlaneResource* software_resource = plane_resource->AsSoftware();

        // We know the format is RGBA_8888 from check above.
        SkImageInfo info = SkImageInfo::MakeN32Premul(
            gfx::SizeToSkISize(software_resource->resource_size()));

        SkBitmap sk_bitmap;
        sk_bitmap.installPixels(info, software_resource->pixels(),
                                info.minRowBytes());
        cc::SkiaPaintCanvas canvas(sk_bitmap);

        // This is software path, so canvas and video_frame are always backed
        // by software.
        video_renderer_->Copy(video_frame, &canvas, nullptr);
      } else {
        HardwarePlaneResource* hardware_resource = plane_resource->AsHardware();
        size_t bytes_per_row = viz::ResourceSizes::CheckedWidthInBytes<size_t>(
            video_frame->coded_size().width(), viz::ResourceFormat::RGBA_8888);
        size_t needed_size = bytes_per_row * video_frame->coded_size().height();
        if (upload_pixels_size_ < needed_size) {
          // Free the existing data first so that the memory can be reused,
          // if possible. Note that the new array is purposely not initialized.
          upload_pixels_.reset();
          upload_pixels_.reset(new uint8_t[needed_size]);
          upload_pixels_size_ = needed_size;
        }

        PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
            video_frame.get(), upload_pixels_.get(), bytes_per_row);

        // Copy pixels into texture.
        auto* gl = raster_context_provider_
                       ? raster_context_provider_->ContextGL()
                       : context_provider_->ContextGL();

        const gfx::Size& plane_size = hardware_resource->resource_size();
        {
          HardwarePlaneResource::ScopedTexture scope(gl, hardware_resource);
          gl->BindTexture(hardware_resource->texture_target(),
                          scope.texture_id());
          gl->TexSubImage2D(
              hardware_resource->texture_target(), 0, 0, 0, plane_size.width(),
              plane_size.height(), GLDataFormat(viz::ResourceFormat::RGBA_8888),
              GLDataType(viz::ResourceFormat::RGBA_8888), upload_pixels_.get());
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
      auto* gl = raster_context_provider_
                     ? raster_context_provider_->ContextGL()
                     : context_provider_->ContextGL();
      GenerateCompositorSyncToken(gl, &sync_token);
      transferable_resource = viz::TransferableResource::MakeGL(
          hardware_resource->mailbox(), GL_LINEAR,
          hardware_resource->texture_target(), sync_token,
          hardware_resource->resource_size(),
          hardware_resource->overlay_candidate());
    }

    transferable_resource.color_space = output_color_space;
    transferable_resource.format = viz::ResourceFormat::RGBA_8888;
    external_resources.resources.push_back(std::move(transferable_resource));
    external_resources.release_callbacks.push_back(base::BindOnce(
        &VideoResourceUpdater::RecycleResource, weak_ptr_factory_.GetWeakPtr(),
        plane_resource->plane_resource_id()));

    return external_resources;
  }

  const viz::ResourceFormat yuv_resource_format =
      YuvResourceFormat(bits_per_channel);
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

  // We need to transfer data from |video_frame| to the plane resources.
  for (size_t i = 0; i < plane_resources.size(); ++i) {
    HardwarePlaneResource* plane_resource = plane_resources[i]->AsHardware();

    // Skip the transfer if this |video_frame|'s plane has been processed.
    if (plane_resource->Matches(video_frame->unique_id(), i))
      continue;

    const viz::ResourceFormat plane_resource_format =
        plane_resource->resource_format();
    DCHECK_EQ(plane_resource_format, yuv_resource_format);

    // TODO(hubbe): Move upload code to media/.
    // TODO(reveman): Can use GpuMemoryBuffers here to improve performance.

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
    const size_t upload_image_stride =
        cc::MathUtil::CheckedRoundUp<size_t>(bytes_per_row, 4u);

    const size_t resource_bit_depth =
        static_cast<size_t>(viz::BitsPerPixel(plane_resource_format));

    // Data downshifting is needed if the resource bit depth is not enough.
    const bool needs_bit_downshifting = bits_per_channel > resource_bit_depth;

    // A copy to adjust strides is needed if those are different and both source
    // and destination have the same bit depth.
    const bool needs_stride_adaptation =
        (bits_per_channel == resource_bit_depth) &&
        (upload_image_stride != static_cast<size_t>(video_stride_bytes));

    // We need to convert the incoming data if we're transferring to half float,
    // if the need a bit downshift or if the strides need to be reconciled.
    const bool needs_conversion = plane_resource_format == viz::LUMINANCE_F16 ||
                                  needs_bit_downshifting ||
                                  needs_stride_adaptation;

    const uint8_t* pixels;
    if (!needs_conversion) {
      pixels = video_frame->data(i);
    } else {
      // Avoid malloc for each frame/plane if possible.
      const size_t needed_size =
          upload_image_stride * resource_size_pixels.height();
      if (upload_pixels_size_ < needed_size) {
        // Free the existing data first so that the memory can be reused,
        // if possible. Note that the new array is purposely not initialized.
        upload_pixels_.reset();
        upload_pixels_.reset(new uint8_t[needed_size]);
        upload_pixels_size_ = needed_size;
      }

      if (plane_resource_format == viz::LUMINANCE_F16) {
        for (int row = 0; row < resource_size_pixels.height(); ++row) {
          uint16_t* dst = reinterpret_cast<uint16_t*>(
              &upload_pixels_[upload_image_stride * row]);
          const uint16_t* src = reinterpret_cast<uint16_t*>(
              video_frame->data(i) + (video_stride_bytes * row));
          half_float_maker->MakeHalfFloats(src, bytes_per_row / 2, dst);
        }
      } else if (needs_bit_downshifting) {
        DCHECK(plane_resource_format == viz::LUMINANCE_8 ||
               plane_resource_format == viz::RED_8);
        const int scale = 0x10000 >> (bits_per_channel - 8);
        libyuv::Convert16To8Plane(
            reinterpret_cast<uint16_t*>(video_frame->data(i)),
            video_stride_bytes / 2, upload_pixels_.get(), upload_image_stride,
            scale, bytes_per_row, resource_size_pixels.height());
      } else {
        // Make a copy to reconcile stride, size and format being equal.
        DCHECK(needs_stride_adaptation);
        DCHECK(plane_resource_format == viz::LUMINANCE_8 ||
               plane_resource_format == viz::RED_8);
        libyuv::CopyPlane(video_frame->data(i), video_stride_bytes,
                          upload_pixels_.get(), upload_image_stride,
                          resource_size_pixels.width(),
                          resource_size_pixels.height());
      }

      pixels = upload_pixels_.get();
    }

    // Copy pixels into texture. TexSubImage2D() is applicable because
    // |yuv_resource_format| is LUMINANCE_F16, R16_EXT, LUMINANCE_8 or RED_8.
    auto* gl = raster_context_provider_ ? raster_context_provider_->ContextGL()
                                        : context_provider_->ContextGL();
    DCHECK(GLSupportsFormat(plane_resource_format));
    {
      HardwarePlaneResource::ScopedTexture scope(gl, plane_resource);
      gl->BindTexture(plane_resource->texture_target(), scope.texture_id());
      gl->TexSubImage2D(plane_resource->texture_target(), 0, 0, 0,
                        resource_size_pixels.width(),
                        resource_size_pixels.height(),
                        GLDataFormat(plane_resource_format),
                        GLDataType(plane_resource_format), pixels);
    }

    plane_resource->SetUniqueId(video_frame->unique_id(), i);
  }

  // Set the sync token otherwise resource is assumed to be synchronized.
  gpu::SyncToken sync_token;
  auto* gl = raster_context_provider_ ? raster_context_provider_->ContextGL()
                                      : context_provider_->ContextGL();
  GenerateCompositorSyncToken(gl, &sync_token);

  for (size_t i = 0; i < plane_resources.size(); ++i) {
    HardwarePlaneResource* plane_resource = plane_resources[i]->AsHardware();
    auto transferable_resource = viz::TransferableResource::MakeGL(
        plane_resource->mailbox(), GL_LINEAR, plane_resource->texture_target(),
        sync_token, plane_resource->resource_size(),
        plane_resource->overlay_candidate());
    transferable_resource.color_space = output_color_space;
    transferable_resource.format = output_resource_format;
    external_resources.resources.push_back(std::move(transferable_resource));
    external_resources.release_callbacks.push_back(base::BindOnce(
        &VideoResourceUpdater::RecycleResource, weak_ptr_factory_.GetWeakPtr(),
        plane_resource->plane_resource_id()));
  }

  external_resources.type = VideoFrameResourceType::YUV;
  return external_resources;
}

void VideoResourceUpdater::ReturnTexture(scoped_refptr<VideoFrame> video_frame,
                                         const gpu::SyncToken& sync_token,
                                         bool lost_resource) {
  // TODO(dshwang): Forward to the decoder as a lost resource.
  if (lost_resource)
    return;

  // The video frame will insert a wait on the previous release sync token.
  auto* gl = raster_context_provider_ ? raster_context_provider_->ContextGL()
                                      : context_provider_->ContextGL();
  SyncTokenClientImpl client(gl, sync_token);
  video_frame->UpdateReleaseSyncToken(&client);
}

void VideoResourceUpdater::RecycleResource(uint32_t plane_resource_id,
                                           const gpu::SyncToken& sync_token,
                                           bool lost_resource) {
  auto matches_id_fn =
      [plane_resource_id](const std::unique_ptr<PlaneResource>& resource) {
        return resource->plane_resource_id() == plane_resource_id;
      };
  auto resource_it =
      std::find_if(all_resources_.begin(), all_resources_.end(), matches_id_fn);
  if (resource_it == all_resources_.end())
    return;

  if (context_provider_ && sync_token.HasData()) {
    auto* gl = raster_context_provider_ ? raster_context_provider_->ContextGL()
                                        : context_provider_->ContextGL();
    gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
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

}  // namespace media
