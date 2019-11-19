// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/frame_renderer_thumbnail.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

namespace media {
namespace test {

namespace {

// Size of the large image to which the thumbnails will be rendered.
constexpr gfx::Size kThumbnailsPageSize(1600, 1200);
// Size of the individual thumbnails that will be rendered.
constexpr gfx::Size kThumbnailSize(160, 120);

// Default filename used to store the thumbnails image.
constexpr const base::FilePath::CharType* kThumbnailFilename =
    FILE_PATH_LITERAL("thumbnail.png");

// Vertex shader used to render thumbnails.
constexpr char kVertexShader[] =
    "varying vec2 interp_tc;\n"
    "attribute vec4 in_pos;\n"
    "attribute vec2 in_tc;\n"
    "uniform bool tex_flip; void main() {\n"
    "  if (tex_flip)\n"
    "    interp_tc = vec2(in_tc.x, 1.0 - in_tc.y);\n"
    "  else\n"
    "   interp_tc = in_tc;\n"
    "  gl_Position = in_pos;\n"
    "}\n";

// Fragment shader used to render thumbnails.
#if !defined(OS_WIN)
constexpr char kFragmentShader[] =
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
constexpr char kFragmentShader[] =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec2 interp_tc;\n"
    "uniform sampler2D tex;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(tex, interp_tc);\n"
    "}\n";
#endif

GLuint CreateTexture(GLenum texture_target, const gfx::Size& size) {
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(texture_target, texture_id);
  if (texture_target == GL_TEXTURE_2D) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  }

  glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // OpenGLES2.0.25 section 3.8.2 requires CLAMP_TO_EDGE for NPOT textures.
  glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
  return texture_id;
}

void DeleteTexture(uint32_t texture_id) {
  glDeleteTextures(1, &texture_id);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

void RenderTexture(uint32_t texture_target, uint32_t texture_id) {
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

void CreateShader(GLuint program, GLenum type, const char* source, int size) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, &size);
  glCompileShader(shader);
  int result = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    char log[4096];
    glGetShaderInfoLog(shader, base::size(log), nullptr, log);
    LOG(FATAL) << log;
  }
  glAttachShader(program, shader);
  glDeleteShader(shader);
  CHECK_EQ(static_cast<int>(glGetError()), GL_NO_ERROR);
}

void GLSetViewPort(const gfx::Rect& area) {
  glViewport(area.x(), area.y(), area.width(), area.height());
  glScissor(area.x(), area.y(), area.width(), area.height());
}

// Helper function to convert from RGBA to RGB. Returns false if any alpha
// channel is not 0xff, otherwise true.
bool ConvertRGBAToRGB(const std::vector<unsigned char>& rgba,
                      std::vector<unsigned char>* rgb) {
  size_t num_pixels = rgba.size() / 4;
  rgb->resize(num_pixels * 3);
  // Drop the alpha channel, but check as we go that it is all 0xff.
  bool solid = true;
  for (size_t i = 0; i < num_pixels; i++) {
    (*rgb)[3 * i] = rgba[4 * i];
    (*rgb)[3 * i + 1] = rgba[4 * i + 1];
    (*rgb)[3 * i + 2] = rgba[4 * i + 2];
    solid = solid && (rgba[4 * i + 3] == 0xff);
  }
  return solid;
}

}  // namespace

bool FrameRendererThumbnail::gl_initialized_ = false;

FrameRendererThumbnail::FrameRendererThumbnail(
    const std::vector<std::string>& thumbnail_checksums,
    const base::FilePath& output_folder)
    : thumbnail_checksums_(thumbnail_checksums), output_folder_(output_folder) {
  DETACH_FROM_SEQUENCE(renderer_sequence_checker_);
}

FrameRendererThumbnail::~FrameRendererThumbnail() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (renderer_task_runner_) {
    base::WaitableEvent done;
    renderer_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FrameRendererThumbnail::DestroyTask,
                                  base::Unretained(this), &done));
    done.Wait();
  }
}

// static
std::unique_ptr<FrameRendererThumbnail> FrameRendererThumbnail::Create(
    const std::vector<std::string> thumbnail_checksums,
    const base::FilePath& output_folder) {
  auto frame_renderer = base::WrapUnique(
      new FrameRendererThumbnail(thumbnail_checksums, output_folder));
  frame_renderer->Initialize();
  return frame_renderer;
}

bool FrameRendererThumbnail::AcquireGLContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  return gl_context_->MakeCurrent(gl_surface_.get());
}

gl::GLContext* FrameRendererThumbnail::GetGLContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  return gl_context_.get();
}

void FrameRendererThumbnail::RenderFrame(
    scoped_refptr<VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  if (video_frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM))
    return;

  if (!renderer_task_runner_)
    renderer_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  if (thumbnails_texture_id_ == 0u)
    InitializeThumbnailImageTask();

  // Find the texture associated with the video frame's mailbox.
  const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(0);
  const gpu::Mailbox& mailbox = mailbox_holder.mailbox;
  auto it = mailbox_texture_map_.find(mailbox);
  ASSERT_NE(it, mailbox_texture_map_.end());

  RenderThumbnailTask(mailbox_holder.texture_target, it->second);
}

void FrameRendererThumbnail::WaitUntilRenderingDone() {}

scoped_refptr<VideoFrame> FrameRendererThumbnail::CreateVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& texture_size,
    uint32_t texture_target,
    uint32_t* texture_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  // Create a mailbox.
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes];
  mailbox_holders[0] =
      gpu::MailboxHolder(mailbox, gpu::SyncToken(), texture_target);

  // Create a new video frame associated with the mailbox.
  base::OnceCallback<void(const gpu::SyncToken&)> mailbox_holder_release_cb =
      base::BindOnce(&FrameRendererThumbnail::DeleteTextureTask,
                     base::Unretained(this), mailbox);
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
      pixel_format, mailbox_holders, std::move(mailbox_holder_release_cb),
      texture_size, gfx::Rect(texture_size), texture_size, base::TimeDelta());

  // Create a texture and associate it with the mailbox.
  *texture_id = CreateTexture(texture_target, texture_size);

  mailbox_texture_map_.insert(std::make_pair(mailbox, *texture_id));

  return frame;
}

bool FrameRendererThumbnail::ValidateThumbnail() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (!renderer_task_runner_)
    return false;

  bool success = false;
  base::WaitableEvent done;
  renderer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FrameRendererThumbnail::ValidateThumbnailTask,
                                base::Unretained(this), &success, &done));
  done.Wait();

  return success;
}

void FrameRendererThumbnail::SaveThumbnailTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  // Create the directory tree if it doesn't exist yet.
  if (!DirectoryExists(output_folder_))
    base::CreateDirectory(output_folder_);

  const std::vector<uint8_t> rgba = ConvertThumbnailToRGBATask();

  // Convert raw RGBA into PNG for export.
  std::vector<unsigned char> png;
  gfx::PNGCodec::Encode(&rgba[0], gfx::PNGCodec::FORMAT_RGBA,
                        kThumbnailsPageSize, kThumbnailsPageSize.width() * 4,
                        true, std::vector<gfx::PNGCodec::Comment>(), &png);

  base::FilePath filepath =
      base::MakeAbsoluteFilePath(output_folder_).Append(kThumbnailFilename);
  LOG(INFO) << "Saving thumbnails image to " << filepath;

  base::File thumbnail_file(
      filepath, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  int num_bytes =
      thumbnail_file.Write(0u, reinterpret_cast<char*>(&png[0]), png.size());
  ASSERT_NE(-1, num_bytes);
  EXPECT_EQ(static_cast<size_t>(num_bytes), png.size());
}

void FrameRendererThumbnail::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  // Initialize GL rendering and create GL context.
  if (!gl_initialized_) {
    if (!gl::init::InitializeGLOneOff())
      LOG(FATAL) << "Could not initialize GL";
    gl_initialized_ = true;
  }
  gl_surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
  gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                          gl::GLContextAttribs());
}

void FrameRendererThumbnail::DestroyTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);
  DCHECK(mailbox_texture_map_.empty());

  DestroyThumbnailImageTask();

  // Release the |gl_context_| so it can be destroyed on the same thread it was
  // created on. Otherwise random crashes might occur as not all resources are
  // freed correctly.
  gl_context_->ReleaseCurrent(gl_surface_.get());

  done->Signal();
}

void FrameRendererThumbnail::InitializeThumbnailImageTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  GLint max_texture_size;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
  CHECK_GE(max_texture_size, kThumbnailsPageSize.width());
  CHECK_GE(max_texture_size, kThumbnailsPageSize.height());

  thumbnails_fbo_size_ = kThumbnailsPageSize;
  thumbnail_size_ = kThumbnailSize;

  glGenFramebuffersEXT(1, &thumbnails_fbo_id_);
  glGenTextures(1, &thumbnails_texture_id_);
  glBindTexture(GL_TEXTURE_2D, thumbnails_texture_id_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, thumbnails_fbo_size_.width(),
               thumbnails_fbo_size_.height(), 0, GL_RGB,
               GL_UNSIGNED_SHORT_5_6_5, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            thumbnails_texture_id_, 0);

  GLenum fb_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  CHECK(fb_status == GL_FRAMEBUFFER_COMPLETE) << fb_status;
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glBindFramebufferEXT(GL_FRAMEBUFFER,
                       gl_surface_->GetBackingFramebufferObject());

  // These vertices and texture coords map (0,0) in the texture to the bottom
  // left of the viewport. Since we get the video frames with the the top left
  // at (0,0) we need to flip the texture y coordinate in the vertex shader for
  // this to be rendered the right way up. In the case of thumbnail rendering we
  // use the same vertex shader to render the FBO to the screen, where we do not
  // want this flipping. Vertices are 2 floats for position and 2 floats for
  // texcoord each.
  const float kVertices[] = {
      -1, 1,  0, 1,  // Vertex 0
      -1, -1, 0, 0,  // Vertex 1
      1,  1,  1, 1,  // Vertex 2
      1,  -1, 1, 0,  // Vertex 3
  };
  const GLvoid* kVertexPositionOffset = 0;
  const GLvoid* kVertexTexcoordOffset =
      reinterpret_cast<GLvoid*>(sizeof(float) * 2);
  const GLsizei kVertexStride = sizeof(float) * 4;

  glGenBuffersARB(1, &vertex_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);

  program_ = glCreateProgram();
  CreateShader(program_, GL_VERTEX_SHADER, kVertexShader,
               base::size(kVertexShader));
  CreateShader(program_, GL_FRAGMENT_SHADER, kFragmentShader,
               base::size(kFragmentShader));
  glLinkProgram(program_);
  GLint result = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &result);
  if (!result) {
    constexpr GLsizei kLogBufferSize = 4096;
    char log[kLogBufferSize];
    glGetShaderInfoLog(program_, kLogBufferSize, nullptr, log);
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
  GLint pos_location = glGetAttribLocation(program_, "in_pos");
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, kVertexStride,
                        kVertexPositionOffset);
  GLint tc_location = glGetAttribLocation(program_, "in_tc");
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, kVertexStride,
                        kVertexTexcoordOffset);

  // Unbind the vertex buffer
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void FrameRendererThumbnail::DestroyThumbnailImageTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  glDeleteTextures(1, &thumbnails_texture_id_);
  glDeleteFramebuffersEXT(1, &thumbnails_fbo_id_);
  glDeleteBuffersARB(1, &vertex_buffer_);

  thumbnails_texture_id_ = 0u;
  thumbnails_fbo_id_ = 0u;
  vertex_buffer_ = 0u;
}

void FrameRendererThumbnail::RenderThumbnailTask(uint32_t texture_target,
                                                 uint32_t texture_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

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
  // We need to flush the GL commands before returning the thumbnail texture to
  // the decoder.
  glFlush();

  ++frame_count_;
}

const std::vector<uint8_t>
FrameRendererThumbnail::ConvertThumbnailToRGBATask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  std::vector<uint8_t> rgba;
  const size_t num_pixels = thumbnails_fbo_size_.GetArea();
  rgba.resize(num_pixels * 4);
  glBindFramebufferEXT(GL_FRAMEBUFFER, thumbnails_fbo_id_);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  // We can only count on GL_RGBA/GL_UNSIGNED_BYTE support.
  glReadPixels(0, 0, thumbnails_fbo_size_.width(),
               thumbnails_fbo_size_.height(), GL_RGBA, GL_UNSIGNED_BYTE,
               &(rgba)[0]);
  glBindFramebufferEXT(GL_FRAMEBUFFER,
                       gl_surface_->GetBackingFramebufferObject());

  return rgba;
}

void FrameRendererThumbnail::ValidateThumbnailTask(bool* success,
                                                   base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  const std::vector<uint8_t> rgba = ConvertThumbnailToRGBATask();

  // Convert the thumbnail from RGBA to RGB.
  std::vector<uint8_t> rgb;
  EXPECT_EQ(ConvertRGBAToRGB(rgba, &rgb), true)
      << "RGBA frame has incorrect alpha";

  // Calculate the thumbnail's checksum and compare it to golden values.
  std::string md5_string = base::MD5String(
      base::StringPiece(reinterpret_cast<char*>(&rgb[0]), rgb.size()));
  *success = base::Contains(thumbnail_checksums_, md5_string);

  // If validation failed, write the thumbnail image to disk.
  if (!success)
    SaveThumbnailTask();

  done->Signal();
}

void FrameRendererThumbnail::DeleteTextureTask(const gpu::Mailbox& mailbox,
                                               const gpu::SyncToken&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(renderer_sequence_checker_);

  auto it = mailbox_texture_map_.find(mailbox);
  ASSERT_NE(it, mailbox_texture_map_.end());
  uint32_t texture_id = it->second;
  mailbox_texture_map_.erase(mailbox);

  DeleteTexture(texture_id);
}

}  // namespace test
}  // namespace media
