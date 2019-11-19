// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_FRAME_RENDERER_THUMBNAIL_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_FRAME_RENDERER_THUMBNAIL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "gpu/command_buffer/common/gl2_types.h"
#include "media/gpu/test/video_player/frame_renderer.h"

namespace gl {

class GLContext;
class GLSurface;

}  // namespace gl

namespace media {
namespace test {

// The thumbnail frame renderer draws a thumbnail of each frame into a large
// image containing 10x10 thumbnails. The checksum of this image can then be
// compared to a golden value.
// Rendering introduces small platform-dependant differences, so multiple golden
// values need to be maintained. The thumbnail frame renderer should only be
// used on older platforms that are not supported by the FrameValidator, and
// will be deprecated soon.
class FrameRendererThumbnail : public FrameRenderer {
 public:
  ~FrameRendererThumbnail() override;

  // Create an instance of the thumbnail frame renderer.
  static std::unique_ptr<FrameRendererThumbnail> Create(
      const std::vector<std::string> thumbnail_checksums,
      const base::FilePath& output_folder);

  // FrameRenderer implementation
  // Acquire the GL context on the |renderer_task_runner_|. This needs to be
  // called before executing any GL-related functions. The context will remain
  // current on the |renderer_task_runner_| until the renderer is destroyed.
  bool AcquireGLContext() override;
  // Get the active GL context on the |renderer_task_runner_|.
  gl::GLContext* GetGLContext() override;
  void RenderFrame(scoped_refptr<VideoFrame> video_frame) override;
  void WaitUntilRenderingDone() override;
  scoped_refptr<VideoFrame> CreateVideoFrame(VideoPixelFormat pixel_format,
                                             const gfx::Size& texture_size,
                                             uint32_t texture_target,
                                             uint32_t* texture_id) override;

  // Validate the thumbnail image by comparing it against known golden values.
  bool ValidateThumbnail();

 private:
  explicit FrameRendererThumbnail(
      const std::vector<std::string>& thumbnail_checksums,
      const base::FilePath& output_folder);

  // Initialize the frame renderer, performs all rendering-related setup.
  void Initialize();
  // Perform cleanup on the |renderer_task_runner_|, releases the GL context.
  void DestroyTask(base::WaitableEvent* done);

  // Initialize the thumbnail image the frame thumbnails will be rendered to on
  // the |renderer_task_runner_|.
  void InitializeThumbnailImageTask();
  // Destroy the thumbnail image on the |renderer_task_runner_|.
  void DestroyThumbnailImageTask();
  // Render the texture with specified |texture_id| to the thumbnail image on
  // the |renderer_task_runner_|.
  void RenderThumbnailTask(uint32_t texture_target, uint32_t texture_id);
  // Convert the thumbnail image to RGBA on the |renderer_task_runner_|.
  const std::vector<uint8_t> ConvertThumbnailToRGBATask();
  // Validate the thumbnail image on the |renderer_task_runner_|. The validated
  // thumbnail will be saved to disk on failure.
  void ValidateThumbnailTask(bool* success, base::WaitableEvent* done);
  // Save the thumbnail image to disk on the |renderer_task_runner_|.
  void SaveThumbnailTask();

  // Destroy the texture associated with the |mailbox| on the
  // |renderer_task_runner_|.
  void DeleteTextureTask(const gpu::Mailbox& mailbox, const gpu::SyncToken&);

  // The number of frames rendered so far.
  size_t frame_count_ = 0u;

  // The list of thumbnail checksums for all platforms.
  const std::vector<std::string> thumbnail_checksums_;
  // Map between mailboxes and texture id's.
  std::map<gpu::Mailbox, uint32_t> mailbox_texture_map_;

  // Id of the frame buffer used to render the thumbnails image.
  GLuint thumbnails_fbo_id_ = 0u;
  // Size of the frame buffer used to render the thumbnails image.
  gfx::Size thumbnails_fbo_size_;
  // Texture id of the thumbnails image.
  GLuint thumbnails_texture_id_ = 0u;
  // Size of the individual thumbnails rendered to the thumbnails image.
  gfx::Size thumbnail_size_;
  // Vertex buffer used to render thumbnails to the thumbnails image.
  GLuint vertex_buffer_ = 0u;
  // Shader program used to render thumbnails to the thumbnails image.
  GLuint program_ = 0u;

  // GL context used for rendering.
  scoped_refptr<gl::GLContext> gl_context_;
  // GL surface used for rendering.
  scoped_refptr<gl::GLSurface> gl_surface_;

  // Whether GL was initialized, as it should only happen once.
  static bool gl_initialized_;

  // Output folder the thumbnails image will be written to by SaveThumbnail().
  const base::FilePath output_folder_;

  // Renderer thread task runner.
  scoped_refptr<base::SequencedTaskRunner> renderer_task_runner_;

  SEQUENCE_CHECKER(client_sequence_checker_);
  SEQUENCE_CHECKER(renderer_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(FrameRendererThumbnail);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_FRAME_RENDERER_THUMBNAIL_H_
