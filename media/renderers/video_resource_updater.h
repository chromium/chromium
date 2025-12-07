// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_RESOURCE_UPDATER_H_
#define MEDIA_RENDERERS_VIDEO_RESOURCE_UPDATER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/unguessable_token.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
class Transform;
class MaskFilterInfo;
}  // namespace gfx

namespace viz {
class ClientResourceProvider;
class RasterContextProvider;
class CompositorRenderPass;
}  // namespace viz

namespace gpu {
class SharedImageInterface;
}  // namespace gpu

namespace media {
class PaintCanvasVideoRenderer;

// Specifies what type of data is contained in the mailbox.
enum class VideoFrameResourceType {
  NONE,
  RGB,
  RGBA_PREMULTIPLIED,
  // The VideoFrame is merely a hint to compositor that a hole must be made
  // transparent so the video underlay will be visible.
  // Used by Chromecast only.
  VIDEO_HOLE,
};

class MEDIA_EXPORT VideoFrameExternalResource {
 public:
  VideoFrameResourceType type = VideoFrameResourceType::NONE;
  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;

  VideoFrameExternalResource();
  VideoFrameExternalResource(VideoFrameExternalResource&& other);
  VideoFrameExternalResource& operator=(VideoFrameExternalResource&& other);
  ~VideoFrameExternalResource();
};

// VideoResourceUpdater is used by the video system to produce frame content as
// resources consumable by the display compositor.
class MEDIA_EXPORT VideoResourceUpdater
    : public base::trace_event::MemoryDumpProvider {
 public:
  // For GPU compositing |context_provider| should be provided and for software
  // compositing |shared_image_interface| should be provided. If there is a
  // non-null |context_provider| we assume GPU compositing.
  VideoResourceUpdater(
      viz::RasterContextProvider* context_provider,
      viz::ClientResourceProvider* resource_provider,
      scoped_refptr<gpu::SharedImageInterface> shared_image_interface,
      bool use_gpu_memory_buffer_resources,
      int max_resource_size);

  VideoResourceUpdater(const VideoResourceUpdater&) = delete;
  VideoResourceUpdater& operator=(const VideoResourceUpdater&) = delete;

  ~VideoResourceUpdater() override;

  // For each CompositorFrame the following sequence is expected:
  // 1. ObtainFrameResource(): Import resource for the next video frame with
  //    viz::ClientResourceProvider. This will reuse existing GPU or
  //    SharedMemory buffers if possible, otherwise it will allocate new ones.
  // 2. AppendQuad(): Add DrawQuad to CompositorFrame for video.
  // 3. ReleaseFrameResource(): After the CompositorFrame has been submitted,
  //    remove imported resource from viz::ClientResourceProvider.
  void ObtainFrameResource(scoped_refptr<VideoFrame> video_frame);
  void ReleaseFrameResource();
  // Appends a quad representing |frame| to |render_pass|.
  // At most one quad is expected to be appended, this is enforced by the users
  // of this class (e.g: VideoFrameSubmitter). Producing only one quad will
  // allow viz to optimize compositing when the only content changing per-frame
  // is the video.
  void AppendQuad(viz::CompositorRenderPass* render_pass,
                  scoped_refptr<VideoFrame> frame,
                  gfx::Transform transform,
                  gfx::Rect quad_rect,
                  gfx::Rect visible_quad_rect,
                  const gfx::MaskFilterInfo& mask_filter_info,
                  std::optional<gfx::Rect> clip_rect,
                  bool context_opaque,
                  float draw_opacity,
                  int sorting_context_id);

  void ClearFrameResources();

  // TODO(kylechar): This is only public for testing, make private.
  VideoFrameExternalResource CreateExternalResourceFromVideoFrame(
      scoped_refptr<VideoFrame> video_frame);

  viz::SharedImageFormat YuvSharedImageFormat(int bits_per_channel);
  gpu::SharedImageInterface* shared_image_interface() const;

  viz::ResourceId GetFrameResourceIdForTesting() const;

 private:
  class FrameResource;

  bool software_compositor() const { return context_provider_ == nullptr; }

  // Reallocate |upload_pixels_| with the requested size.
  bool ReallocateUploadPixels(size_t needed_size, size_t plane);

  // Obtain a resource of the right format by either recycling an
  // unreferenced but appropriately formatted resource, or by
  // allocating a new resource.
  // Additionally, if the |unique_id| matches, then it is assumed that the
  // resource has the right data already and will only be used for reading, and
  // so is returned even if it is still referenced.
  FrameResource* RecycleOrAllocateResource(const gfx::Size& resource_size,
                                           viz::SharedImageFormat si_format,
                                           const gfx::ColorSpace& color_space,
                                           SkAlphaType alpha_type,
                                           VideoFrame::ID unique_id);
  FrameResource* AllocateResource(const gfx::Size& size,
                                  viz::SharedImageFormat format,
                                  const gfx::ColorSpace& color_space,
                                  SkAlphaType alpha_type);

  // Create a copy of a texture-backed source video frame in a new GL_TEXTURE_2D
  // texture. This is used when there are multiple GPU threads (Android WebView)
  // and the source video frame texture can't be used on the output GL context.
  // https://crbug.com/582170
  VideoFrameExternalResource CopyHardwareResource(VideoFrame* video_frame);

  // Get resource ready to be appended into DrawQuad. This is used for GPU
  // compositing most of the time, except for the cases mentioned in
  // CreateForSoftwareFrame().
  VideoFrameExternalResource CreateForHardwareFrame(
      scoped_refptr<VideoFrame> video_frame);

  // Get the shared image format for creating resource which is used for
  // software compositing or GPU compositing with video frames without textures
  // (pixel upload).
  viz::SharedImageFormat GetSoftwareOutputFormat(
      VideoPixelFormat input_frame_format,
      int bits_per_channel);

  // Transfer RGB pixels from the video frame to software resource through
  // canvas via PaintCanvasVideoRenderer.
  void TransferRGBPixelsToPaintCanvas(scoped_refptr<VideoFrame> video_frame,
                                      FrameResource* frame_resource);

  // Write/copy RGB pixels from video frame to hardware resource through
  // WritePixels or TexSubImage2D.
  bool WriteRGBPixelsToTexture(scoped_refptr<VideoFrame> video_frame,
                               FrameResource* frame_resource);

  // Write/copy YUV pixels for all planes from video frame to hardware resource
  // through WritePixelsYUV. Also perform bit downshifting for
  // channel format mismatch between input frame and supported shared image
  // format.
  bool WriteYUVPixelsForAllPlanesToTexture(
      scoped_refptr<VideoFrame> video_frame,
      FrameResource* resource,
      size_t bits_per_channel);

  // Get resource ready to be appended into DrawQuad. This is always used for
  // software compositing. This is also used for GPU compositing when the input
  // video frame has no textures.
  VideoFrameExternalResource CreateForSoftwareFrame(
      scoped_refptr<VideoFrame> video_frame);

  gpu::raster::RasterInterface* RasterInterface();

  void RecycleResource(uint32_t resource_id,
                       const gpu::SyncToken& sync_token,
                       bool lost_resource);
  void ReturnTexture(scoped_refptr<VideoFrame> video_frame,
                     const gpu::SyncToken& original_release_token,
                     const gpu::SyncToken& new_release_token,
                     bool lost_resource);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  const raw_ptr<viz::RasterContextProvider> context_provider_;
  scoped_refptr<gpu::SharedImageInterface> shared_image_interface_;
  const raw_ptr<viz::ClientResourceProvider, DanglingUntriaged>
      resource_provider_;
  const bool use_gpu_memory_buffer_resources_;
  const int max_resource_size_;
  const int tracing_id_;
  std::unique_ptr<PaintCanvasVideoRenderer> video_renderer_;
  uint32_t next_plane_resource_id_ = 1;

  // Temporary pixel buffers when converting between formats.
  using PlaneData = base::HeapArray<uint8_t, base::UncheckedFreeDeleter>;
  std::array<PlaneData, SkYUVAInfo::kMaxPlanes> upload_pixels_ = {};

  VideoFrameResourceType frame_resource_type_;

  // Id of resource that will be placed into quad by the next call to
  // AppendDrawQuads().
  viz::ResourceId frame_resource_id_;
  // If the video resource is a hole punching VideoFrame sent by Chromecast,
  // the VideoFrame carries an |overlay_plane_id_| to activate the video
  // overlay, but there is no video content to display within VideoFrame.
  base::UnguessableToken overlay_plane_id_;

  // Resources allocated by VideoResourceUpdater. Used to recycle resources so
  // we can reduce the number of allocations and data transfers.
  std::vector<std::unique_ptr<FrameResource>> all_resources_;

  base::WeakPtrFactory<VideoResourceUpdater> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_RESOURCE_UPDATER_H_
