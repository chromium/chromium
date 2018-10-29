// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/rendering_helper.h"

#include <string.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringize_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif  // defined(USE_OZONE)

namespace media {

namespace {

// Helper for Shader creation.
void CreateShader(GLuint program, GLenum type, const char* source, int size) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, &size);
  glCompileShader(shader);
  int result = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    char log[4096];
    glGetShaderInfoLog(shader, arraysize(log), NULL, log);
    LOG(FATAL) << log;
  }
  glAttachShader(program, shader);
  glDeleteShader(shader);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

void DeleteTexture(uint32_t texture_id) {
  glDeleteTextures(1, &texture_id);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

// Helper function to set GL viewport.
void GLSetViewPort(const gfx::Rect& area) {
  glViewport(area.x(), area.y(), area.width(), area.height());
  glScissor(area.x(), area.y(), area.width(), area.height());
}

}  // namespace

bool RenderingHelper::use_gl_ = false;

VideoFrameTexture::VideoFrameTexture(uint32_t texture_target,
                                     uint32_t texture_id,
                                     const base::Closure& no_longer_needed_cb)
    : texture_target_(texture_target),
      texture_id_(texture_id),
      no_longer_needed_cb_(no_longer_needed_cb) {
  DCHECK(no_longer_needed_cb_);
}

VideoFrameTexture::~VideoFrameTexture() {
  std::move(no_longer_needed_cb_).Run();
}

RenderingHelper::RenderedVideo::RenderedVideo() {}

RenderingHelper::RenderedVideo::RenderedVideo(const RenderedVideo&) = default;

RenderingHelper::RenderedVideo::~RenderedVideo() {}

// static
void RenderingHelper::InitializeOneOff(bool use_gl, base::WaitableEvent* done) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitchASCII(switches::kUseGL, gl::kGLImplementationEGLName);

  use_gl_ = use_gl;

#if defined(USE_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);
  ui::OzonePlatform::GetInstance()->AfterSandboxEntry();
#endif

  if (!use_gl_) {
    done->Signal();
    return;
  }

  if (!gl::init::InitializeGLOneOff())
    LOG(FATAL) << "Could not initialize GL";
  done->Signal();
}

RenderingHelper::RenderingHelper() {
  Clear();
}

RenderingHelper::~RenderingHelper() {
  CHECK_EQ(videos_.size(), 0U) << "Must call UnInitialize before dtor.";
  Clear();
}

void RenderingHelper::Initialize(const RenderingHelperParams& params,
                                 base::WaitableEvent* done) {
  // Use videos_.size() != 0 as a proxy for the class having already been
  // Initialize()'d, and UnInitialize() before continuing.
  if (videos_.size()) {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    UnInitialize(&done);
    done.Wait();
  }

  render_task_.Reset(
      base::Bind(&RenderingHelper::RenderContent, base::Unretained(this)));

  frame_duration_ = params.rendering_fps > 0
                        ? base::TimeDelta::FromSeconds(1) / params.rendering_fps
                        : base::TimeDelta();

  render_as_thumbnails_ = params.render_as_thumbnails;
  task_runner_ = base::ThreadTaskRunnerHandle::Get();

  videos_.resize(params.num_windows);

  // Skip all the GL stuff if we don't use it
  if (!use_gl_) {
    done->Signal();
    return;
  }

  gl_surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
  gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                          gl::GLContextAttribs());
  CHECK(gl_context_->MakeCurrent(gl_surface_.get()));

  if (render_as_thumbnails_) {
    CHECK_EQ(videos_.size(), 1U);

    GLint max_texture_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    CHECK_GE(max_texture_size, params.thumbnails_page_size.width());
    CHECK_GE(max_texture_size, params.thumbnails_page_size.height());

    thumbnails_fbo_size_ = params.thumbnails_page_size;
    thumbnail_size_ = params.thumbnail_size;

    glGenFramebuffersEXT(1, &thumbnails_fbo_id_);
    glGenTextures(1, &thumbnails_texture_id_);
    glBindTexture(GL_TEXTURE_2D, thumbnails_texture_id_);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 thumbnails_fbo_size_.width(), thumbnails_fbo_size_.height(),
                 0,
                 GL_RGB,
                 GL_UNSIGNED_SHORT_5_6_5,
                 NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, thumbnails_texture_id_, 0);

    GLenum fb_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
    CHECK(fb_status == GL_FRAMEBUFFER_COMPLETE) << fb_status;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebufferEXT(GL_FRAMEBUFFER,
                         gl_surface_->GetBackingFramebufferObject());
  }

  // These vertices and texture coords. map (0,0) in the texture to the
  // bottom left of the viewport.  Since we get the video frames with the
  // the top left at (0,0) we need to flip the texture y coordinate
  // in the vertex shader for this to be rendered the right way up.
  // In the case of thumbnail rendering we use the same vertex shader
  // to render the FBO the screen, where we do not want this flipping.
  // Vertices are 2 floats for position and 2 floats for texcoord each.
  static const float kVertices[] = {
      -1, 1,  0, 1,  // Vertex 0
      -1, -1, 0, 0,  // Vertex 1
      1,  1,  1, 1,  // Vertex 2
      1,  -1, 1, 0,  // Vertex 3
  };
  static const GLvoid* kVertexPositionOffset = 0;
  static const GLvoid* kVertexTexcoordOffset =
      reinterpret_cast<GLvoid*>(sizeof(float) * 2);
  static const GLsizei kVertexStride = sizeof(float) * 4;

  glGenBuffersARB(1, &vertex_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);

  static const char kVertexShader[] =
      STRINGIZE(varying vec2 interp_tc; attribute vec4 in_pos;
                attribute vec2 in_tc; uniform bool tex_flip; void main() {
                  if (tex_flip)
                    interp_tc = vec2(in_tc.x, 1.0 - in_tc.y);
                  else
                    interp_tc = in_tc;
                  gl_Position = in_pos;
                });

#if !defined(OS_WIN)
  static const char kFragmentShader[] =
      "#extension GL_OES_EGL_image_external : enable\n"
      "precision mediump float;\n"
      "varying vec2 interp_tc;\n"
      "uniform sampler2D tex;\n"
      "#ifdef GL_OES_EGL_image_external\n"
      "uniform samplerExternalOES tex_external;\n"
      "#endif\n"
      "void main() {\n"
      "  vec4 color = texture2D(tex, interp_tc);\n"
      "#ifdef GL_OES_EGL_image_external\n"
      "  color += texture2D(tex_external, interp_tc);\n"
      "#endif\n"
      "  gl_FragColor = color;\n"
      "}\n";
#else
  static const char kFragmentShader[] =
      "#ifdef GL_ES\n"
      "precision mediump float;\n"
      "#endif\n"
      "varying vec2 interp_tc;\n"
      "uniform sampler2D tex;\n"
      "void main() {\n"
      "  gl_FragColor = texture2D(tex, interp_tc);\n"
      "}\n";
#endif
  program_ = glCreateProgram();
  CreateShader(program_, GL_VERTEX_SHADER, kVertexShader,
               arraysize(kVertexShader));
  CreateShader(program_, GL_FRAGMENT_SHADER, kFragmentShader,
               arraysize(kFragmentShader));
  glLinkProgram(program_);
  int result = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &result);
  if (!result) {
    char log[4096];
    glGetShaderInfoLog(program_, arraysize(log), NULL, log);
    LOG(FATAL) << log;
  }
  glUseProgram(program_);
  glDeleteProgram(program_);

  glUniform1i(glGetUniformLocation(program_, "tex_flip"), 0);
  glUniform1i(glGetUniformLocation(program_, "tex"), 0);
  GLint tex_external = glGetUniformLocation(program_, "tex_external");
  if (tex_external != -1) {
    glUniform1i(tex_external, 1);
  }
  int pos_location = glGetAttribLocation(program_, "in_pos");
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, kVertexStride,
                        kVertexPositionOffset);
  int tc_location = glGetAttribLocation(program_, "in_tc");
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, kVertexStride,
                        kVertexTexcoordOffset);

  // Unbind the vertex buffer
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  done->Signal();
}

void RenderingHelper::UnInitialize(base::WaitableEvent* done) {
  // We have never been initialized in the first place...
  if (task_runner_.get() == nullptr) {
    done->Signal();
    return;
  }

  CHECK(task_runner_->BelongsToCurrentThread());

  render_task_.Cancel();

  if (!use_gl_) {
    Clear();
    done->Signal();
    return;
  }

  if (render_as_thumbnails_) {
    glDeleteTextures(1, &thumbnails_texture_id_);
    glDeleteFramebuffersEXT(1, &thumbnails_fbo_id_);
  }

  glDeleteBuffersARB(1, &vertex_buffer_);

  gl_context_->ReleaseCurrent(gl_surface_.get());
  gl_context_ = NULL;
  gl_surface_ = NULL;

  Clear();
  done->Signal();
}

scoped_refptr<media::test::TextureRef> RenderingHelper::CreateTexture(
    uint32_t texture_target,
    bool pre_allocate,
    VideoPixelFormat pixel_format,
    const gfx::Size& size) {
  CHECK(task_runner_->BelongsToCurrentThread());
  uint32_t texture_id = CreateTextureId(texture_target, size);
  base::OnceClosure delete_texture_cb =
      use_gl_ ? base::BindOnce(DeleteTexture, texture_id) : base::DoNothing();
  if (pre_allocate) {
    return media::test::TextureRef::CreatePreallocated(
        texture_id, std::move(delete_texture_cb), pixel_format, size);
  }
  return media::test::TextureRef::Create(texture_id,
                                         std::move(delete_texture_cb));
}

uint32_t RenderingHelper::CreateTextureId(uint32_t texture_target,
                                          const gfx::Size& size) {
  CHECK(task_runner_->BelongsToCurrentThread());
  if (!use_gl_) {
    return 0;
  }

  uint32_t texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(texture_target, texture_id);
  if (texture_target == GL_TEXTURE_2D) {
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 size.width(), size.height(),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 NULL);
  }
  glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // OpenGLES2.0.25 section 3.8.2 requires CLAMP_TO_EDGE for NPOT textures.
  glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  return texture_id;
}

void RenderingHelper::ConsumeVideoFrame(
    size_t window_id,
    scoped_refptr<VideoFrameTexture> video_frame) {
  if (render_as_thumbnails_) {
    RenderThumbnail(video_frame->texture_target(), video_frame->texture_id());
  } else {
    QueueVideoFrame(window_id, std::move(video_frame));
  }
}

void RenderingHelper::RenderThumbnail(uint32_t texture_target,
                                      uint32_t texture_id) {
  CHECK(task_runner_->BelongsToCurrentThread());
  CHECK(use_gl_);

  const int width = thumbnail_size_.width();
  const int height = thumbnail_size_.height();
  const int thumbnails_in_row = thumbnails_fbo_size_.width() / width;
  const int thumbnails_in_column = thumbnails_fbo_size_.height() / height;
  const int row = (frame_count_ / thumbnails_in_row) % thumbnails_in_column;
  const int col = frame_count_ % thumbnails_in_row;

  gfx::Rect area(col * width, row * height, width, height);

  glUniform1i(glGetUniformLocation(program_, "tex_flip"), 0);
  glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
  GLSetViewPort(area);
  RenderTexture(texture_target, texture_id);
  glBindFramebufferEXT(GL_FRAMEBUFFER,
                       gl_surface_->GetBackingFramebufferObject());

  // Need to flush the GL commands before we return the tnumbnail texture to
  // the decoder.
  glFlush();
  ++frame_count_;
}

void RenderingHelper::QueueVideoFrame(
    size_t window_id,
    scoped_refptr<VideoFrameTexture> video_frame) {
  CHECK(task_runner_->BelongsToCurrentThread());
  RenderedVideo* video = &videos_[window_id];
  DCHECK(!video->is_flushing);

  // If running at zero fps, return immediately. This will give the frame
  // back to the client once it drops its reference to video_frame.
  if (frame_duration_.is_zero())
    return;

  video->pending_frames.push(video_frame);

  if (video->frames_to_drop > 0 && video->pending_frames.size() > 1) {
    --video->frames_to_drop;
    video->pending_frames.pop();
  }

  // Schedules the first RenderContent() if need.
  if (scheduled_render_time_.is_null()) {
    scheduled_render_time_ = base::TimeTicks::Now();
    task_runner_->PostTask(FROM_HERE, render_task_.callback());
  }
}

void RenderingHelper::RenderTexture(uint32_t texture_target,
                                    uint32_t texture_id) {
  // The ExternalOES sampler is bound to GL_TEXTURE1 and the Texture2D sampler
  // is bound to GL_TEXTURE0.
  if (texture_target == GL_TEXTURE_2D) {
    glActiveTexture(GL_TEXTURE0 + 0);
  } else if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
    glActiveTexture(GL_TEXTURE0 + 1);
  }
  glBindTexture(texture_target, texture_id);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindTexture(texture_target, 0);

  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

gl::GLContext* RenderingHelper::GetGLContext() {
  return gl_context_.get();
}

void RenderingHelper::Clear() {
  videos_.clear();
  task_runner_ = nullptr;
  gl_context_ = NULL;
  gl_surface_ = NULL;

  render_as_thumbnails_ = false;
  frame_count_ = 0;
  thumbnails_fbo_id_ = 0;
  thumbnails_texture_id_ = 0;
}

void RenderingHelper::GetThumbnailsAsRGBA(std::vector<unsigned char>* rgba,
                                          base::WaitableEvent* done) {
  CHECK(render_as_thumbnails_ && use_gl_);

  const size_t num_pixels = thumbnails_fbo_size_.GetArea();
  rgba->resize(num_pixels * 4);
  glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  // We can only count on GL_RGBA/GL_UNSIGNED_BYTE support.
  glReadPixels(0, 0, thumbnails_fbo_size_.width(),
               thumbnails_fbo_size_.height(), GL_RGBA, GL_UNSIGNED_BYTE,
               &(*rgba)[0]);
  glBindFramebufferEXT(GL_FRAMEBUFFER,
                       gl_surface_->GetBackingFramebufferObject());

  done->Signal();
}

void RenderingHelper::Flush(size_t window_id) {
  videos_[window_id].is_flushing = true;
}

void RenderingHelper::RenderContent() {
  CHECK(task_runner_->BelongsToCurrentThread());

  // Frames that will be returned to the client (via the no_longer_needed_cb)
  // after this vector falls out of scope at the end of this method. We need
  // to keep references to them until after SwapBuffers() call below.
  std::vector<scoped_refptr<VideoFrameTexture>> frames_to_be_returned;
  for (RenderedVideo& video : videos_) {
    if (video.pending_frames.empty())
      continue;
    scoped_refptr<VideoFrameTexture> frame = video.pending_frames.front();
    // TODO(owenlin): Render to FBO.
    // RenderTexture(frame->texture_target(), frame->texture_id());

    if (video.pending_frames.size() > 1 || video.is_flushing) {
      frames_to_be_returned.push_back(video.pending_frames.front());
      video.pending_frames.pop();
    } else {
      ++video.frames_to_drop;
    }
  }

  ScheduleNextRenderContent();
}

void RenderingHelper::DropOneFrameForAllVideos() {
  for (RenderedVideo& video : videos_) {
    if (video.pending_frames.empty())
      continue;

    if (video.pending_frames.size() > 1 || video.is_flushing) {
      video.pending_frames.pop();
    } else {
      ++video.frames_to_drop;
    }
  }
}

void RenderingHelper::ScheduleNextRenderContent() {
  const auto vsync_interval = base::TimeDelta::FromSeconds(1) / 60;

  scheduled_render_time_ += frame_duration_;
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks target;

  if (vsync_timebase_.is_null()) {
    vsync_timebase_ = now;
  }

  if (vsync_interval.is_zero()) {
    target = std::max(now, scheduled_render_time_);
  } else {
    // Schedules the next RenderContent() at latest VSYNC before the
    // |scheduled_render_time_|.
    target = std::max(now + vsync_interval, scheduled_render_time_);

    int64_t intervals = (target - vsync_timebase_) / vsync_interval;
    target = vsync_timebase_ + intervals * vsync_interval;
  }

  // When the rendering falls behind, drops frames.
  while (scheduled_render_time_ < target) {
    scheduled_render_time_ += frame_duration_;
    DropOneFrameForAllVideos();
  }

  task_runner_->PostDelayedTask(FROM_HERE, render_task_.callback(),
                                target - now);
}
}  // namespace media
