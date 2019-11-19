// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_RESOURCE_UPDATER_H_
#define MEDIA_RENDERERS_VIDEO_RESOURCE_UPDATER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/unguessable_token.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "media/base/media_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
class RRectF;
class Transform;
}  // namespace gfx

namespace viz {
class ClientResourceProvider;
class ContextProvider;
class RasterContextProvider;
class RenderPass;
class SharedBitmapReporter;
}  // namespace viz

namespace media {
class PaintCanvasVideoRenderer;
class VideoFrame;

// Specifies what type of data is contained in the mailboxes, as well as how
// many mailboxes will be present.
enum class VideoFrameResourceType {
  NONE,
  YUV,
  RGB,
  RGBA_PREMULTIPLIED,
  RGBA,
  STREAM_TEXTURE,
  // The VideoFrame is merely a hint to compositor that a hole must be made
  // transparent so the video underlay will be visible.
  // Used by Chromecast only.
  VIDEO_HOLE,
};

class MEDIA_EXPORT VideoFrameExternalResources {
 public:
  VideoFrameResourceType type = VideoFrameResourceType::NONE;
  std::vector<viz::TransferableResource> resources;
  std::vector<viz::ReleaseCallback> release_callbacks;

  // Used by hardware textures which do not return values in the 0-1 range.
  // After a lookup, subtract offset and multiply by multiplier.
  float offset = 0.f;
  float multiplier = 1.f;
  uint32_t bits_per_channel = 8;

  VideoFrameExternalResources();
  VideoFrameExternalResources(VideoFrameExternalResources&& other);
  VideoFrameExternalResources& operator=(VideoFrameExternalResources&& other);
  ~VideoFrameExternalResources();
};

// VideoResourceUpdater is used by the video system to produce frame content as
// resources consumable by the display compositor.
class MEDIA_EXPORT VideoResourceUpdater
    : public base::trace_event::MemoryDumpProvider {
 public:
  // For GPU compositing |context_provider| should be provided and for software
  // compositing |shared_bitmap_reporter| should be provided. If there is a
  // non-null |context_provider| we assume GPU compositing.
  VideoResourceUpdater(viz::ContextProvider* context_provider,
                       viz::RasterContextProvider* raster_context_provider,
                       viz::SharedBitmapReporter* shared_bitmap_reporter,
                       viz::ClientResourceProvider* resource_provider,
                       bool use_stream_video_draw_quad,
                       bool use_gpu_memory_buffer_resources,
                       bool use_r16_texture,
                       int max_resource_size);

  ~VideoResourceUpdater() override;

  // For each CompositorFrame the following sequence is expected:
  // 1. ObtainFrameResources(): Import resources for the next video frame with
  //    viz::ClientResourceProvider. This will reuse existing GPU or
  //    SharedMemory buffers if possible, otherwise it will allocate new ones.
  // 2. AppendQuads(): Add DrawQuads to CompositorFrame for video.
  // 3. ReleaseFrameResources(): After the CompositorFrame has been submitted,
  //    remove imported resources from viz::ClientResourceProvider.
  void ObtainFrameResources(scoped_refptr<VideoFrame> video_frame);
  void ReleaseFrameResources();
  // Appends a quad representing |frame| to |render_pass|.
  // At most one quad is expected to be appended, this is enforced by the users
  // of this class (e.g: VideoFrameSubmitter). Producing only one quad will
  // allow viz to optimize compositing when the only content changing per-frame
  // is the video.
  void AppendQuads(viz::RenderPass* render_pass,
                   scoped_refptr<VideoFrame> frame,
                   gfx::Transform transform,
                   gfx::Rect quad_rect,
                   gfx::Rect visible_quad_rect,
                   const gfx::RRectF& rounded_corner_bounds,
                   gfx::Rect clip_rect,
                   bool is_clipped,
                   bool context_opaque,
                   float draw_opacity,
                   int sorting_context_id);

  // TODO(kylechar): This is only public for testing, make private.
  VideoFrameExternalResources CreateExternalResourcesFromVideoFrame(
      scoped_refptr<VideoFrame> video_frame);

  viz::ResourceFormat YuvResourceFormat(int bits_per_channel);

 private:
  class PlaneResource;
  class HardwarePlaneResource;
  class SoftwarePlaneResource;

  // A resource that will be embedded in a DrawQuad in the next CompositorFrame.
  // Each video plane will correspond to one FrameResource.
  struct FrameResource {
    viz::ResourceId id;
    gfx::Size size_in_pixels;
  };

  bool software_compositor() const {
    return context_provider_ == nullptr && raster_context_provider_ == nullptr;
  }

  // Obtain a resource of the right format by either recycling an
  // unreferenced but appropriately formatted resource, or by
  // allocating a new resource.
  // Additionally, if the |unique_id| and |plane_index| match, then
  // it is assumed that the resource has the right data already and will only be
  // used for reading, and so is returned even if it is still referenced.
  // Passing -1 for |plane_index| avoids returning referenced
  // resources.
  PlaneResource* RecycleOrAllocateResource(const gfx::Size& resource_size,
                                           viz::ResourceFormat resource_format,
                                           const gfx::ColorSpace& color_space,
                                           int unique_id,
                                           int plane_index);
  PlaneResource* AllocateResource(const gfx::Size& plane_size,
                                  viz::ResourceFormat format,
                                  const gfx::ColorSpace& color_space);

  // Create a copy of a texture-backed source video frame in a new GL_TEXTURE_2D
  // texture. This is used when there are multiple GPU threads (Android WebView)
  // and the source video frame texture can't be used on the output GL context.
  // https://crbug.com/582170
  void CopyHardwarePlane(VideoFrame* video_frame,
                         const gfx::ColorSpace& resource_color_space,
                         const gpu::MailboxHolder& mailbox_holder,
                         VideoFrameExternalResources* external_resources);

  // Get resources ready to be appended into DrawQuads. This is used for GPU
  // compositing most of the time, except for the cases mentioned in
  // CreateForSoftwarePlanes().
  VideoFrameExternalResources CreateForHardwarePlanes(
      scoped_refptr<VideoFrame> video_frame);

  // Get resources ready to be appended into DrawQuads. This is always used for
  // software compositing. This is also used for GPU compositing when the input
  // video frame has no textures.
  VideoFrameExternalResources CreateForSoftwarePlanes(
      scoped_refptr<VideoFrame> video_frame);

  void RecycleResource(uint32_t plane_resource_id,
                       const gpu::SyncToken& sync_token,
                       bool lost_resource);
  void ReturnTexture(scoped_refptr<VideoFrame> video_frame,
                     const gpu::SyncToken& sync_token,
                     bool lost_resource);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  viz::ContextProvider* const context_provider_;
  viz::RasterContextProvider* const raster_context_provider_;
  viz::SharedBitmapReporter* const shared_bitmap_reporter_;
  viz::ClientResourceProvider* const resource_provider_;
  const bool use_stream_video_draw_quad_;
  const bool use_gpu_memory_buffer_resources_;
  // TODO(crbug.com/759456): Remove after r16 is used without the flag.
  const bool use_r16_texture_;
  const int max_resource_size_;
  const int tracing_id_;
  std::unique_ptr<PaintCanvasVideoRenderer> video_renderer_;
  uint32_t next_plane_resource_id_ = 1;

  // Temporary pixel buffer when converting between formats.
  std::unique_ptr<uint8_t[]> upload_pixels_;
  size_t upload_pixels_size_ = 0;

  VideoFrameResourceType frame_resource_type_;

  float frame_resource_offset_;
  float frame_resource_multiplier_;
  uint32_t frame_bits_per_channel_;

  // Resources that will be placed into quads by the next call to
  // AppendDrawQuads().
  std::vector<FrameResource> frame_resources_;
  // If the video resource is a hole punching VideoFrame sent by Chromecast,
  // the VideoFrame carries an |overlay_plane_id_| to activate the video
  // overlay, but there is no video content to display within VideoFrame.
  base::UnguessableToken overlay_plane_id_;

  // Resources allocated by VideoResourceUpdater. Used to recycle resources so
  // we can reduce the number of allocations and data transfers.
  std::vector<std::unique_ptr<PlaneResource>> all_resources_;

  base::WeakPtrFactory<VideoResourceUpdater> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoResourceUpdater);
};

}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_RESOURCE_UPDATER_H_
