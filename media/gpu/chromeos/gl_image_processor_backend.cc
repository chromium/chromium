// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/gl_image_processor_backend.h"

#include "base/callback_forward.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace media {

namespace {

#define ALIGN(x, y) (x + (y - 1)) & (~(y - 1))

bool CreateAndAttachShader(GLuint program,
                           GLenum type,
                           const char* source,
                           int size) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, &size);
  glCompileShader(shader);
  int result = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    char log[4096];
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    LOG(ERROR) << log;
    return false;
  }
  glAttachShader(program, shader);
  glDeleteShader(shader);
  return true;
}

scoped_refptr<gl::GLImageNativePixmap> CreateAndBindImage(
    const VideoFrame* video_frame,
    GLuint target) {
  if (video_frame->format() != PIXEL_FORMAT_NV12) {
    LOG(ERROR) << "The frame's format is not NV12";
    return nullptr;
  }
  if (!video_frame->visible_rect().origin().IsOrigin()) {
    LOG(ERROR) << "The frame's visible rectangle's origin is not (0, 0)";
    return nullptr;
  }

  // Create a native pixmap from the frame's memory buffer handle. Not using
  // CreateNativePixmapDmaBuf() because we should be using the visible size.
  gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle =
      CreateGpuMemoryBufferHandle(video_frame);
  if (gpu_memory_buffer_handle.is_null() ||
      gpu_memory_buffer_handle.type != gfx::NATIVE_PIXMAP) {
    LOG(ERROR) << "Failed to create native GpuMemoryBufferHandle";
    return nullptr;
  }
  auto buffer_format =
      VideoPixelFormatToGfxBufferFormat(video_frame->layout().format());
  if (!buffer_format) {
    LOG(ERROR) << "Unexpected video frame format";
    return nullptr;
  }
  auto native_pixmap = base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
      video_frame->coded_size(), *buffer_format,
      std::move(gpu_memory_buffer_handle.native_pixmap_handle));
  DCHECK(native_pixmap->AreDmaBufFdsValid());

  // Import the NativePixmap into GL.
  auto image = gl::GLImageNativePixmap::Create(
      video_frame->coded_size(), gfx::BufferFormat::YUV_420_BIPLANAR,
      std::move(native_pixmap));
  if (!image) {
    LOG(ERROR) << "Could not initialize the GL image";
    return nullptr;
  }
  if (!image->BindTexImage(target)) {
    LOG(ERROR) << "Could not bind the GL image to the texture";
    return nullptr;
  }
  return image;
}

}  // namespace

GLImageProcessorBackend::GLImageProcessorBackend(
    const PortConfig& input_config,
    const PortConfig& output_config,
    OutputMode output_mode,
    VideoRotation relative_rotation,
    ErrorCB error_cb,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : ImageProcessorBackend(input_config,
                            output_config,
                            output_mode,
                            relative_rotation,
                            std::move(error_cb),
                            std::move(backend_task_runner)) {}

bool GLImageProcessorBackend::IsSupported(const PortConfig& input_config,
                                          const PortConfig& output_config,
                                          VideoRotation relative_rotation) {
  if (input_config.fourcc.ToVideoPixelFormat() != PIXEL_FORMAT_NV12 ||
      output_config.fourcc.ToVideoPixelFormat() != PIXEL_FORMAT_NV12) {
    VLOGF(2)
        << "The GLImageProcessorBackend only supports MM21 to NV12 conversion.";
    return false;
  }

  if (relative_rotation != VIDEO_ROTATION_0) {
    VLOGF(2) << "The GLImageProcessorBackend does not support rotation.";
    return false;
  }

  if (input_config.visible_rect != output_config.visible_rect) {
    VLOGF(2) << "The GLImageProcessorBackend does not support scaling.";
    return false;
  }

  // In general, this check is not a safe assumption. However, it takes care of
  // most cases in real usage and it's a good first version.
  if (!input_config.visible_rect.origin().IsOrigin() ||
      !output_config.visible_rect.origin().IsOrigin()) {
    VLOGF(2) << "The GLImageProcessorBackend does not support transposition.";
    return false;
  }

  if (!gfx::Rect(input_config.size).Contains(input_config.visible_rect)) {
    VLOGF(2) << "The input frame size (" << input_config.size.ToString()
             << ") does not contain the input visible rect ("
             << input_config.visible_rect.ToString() << ")";
    return false;
  }
  if (!gfx::Rect(output_config.size).Contains(output_config.visible_rect)) {
    VLOGF(2) << "The output frame size (" << output_config.size.ToString()
             << ") does not contain the output visible rect ("
             << output_config.visible_rect.ToString() << ")";
    return false;
  }

  if ((input_config.size.width() & (kTileWidth - 1)) ||
      (input_config.size.height() & (kTileHeight - 1))) {
    VLOGF(2) << "The input frame coded size (" << input_config.size.ToString()
             << ") is not aligned to the tile dimensions (" << kTileWidth << "x"
             << kTileHeight << ").";
    return false;
  }

  return true;
}

// static
std::unique_ptr<ImageProcessorBackend> GLImageProcessorBackend::Create(
    const PortConfig& input_config,
    const PortConfig& output_config,
    OutputMode output_mode,
    VideoRotation relative_rotation,
    ErrorCB error_cb,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner) {
  DCHECK_EQ(output_mode, OutputMode::IMPORT);

  if (!IsSupported(input_config, output_config, relative_rotation))
    return nullptr;

  auto image_processor =
      std::unique_ptr<GLImageProcessorBackend,
                      std::default_delete<ImageProcessorBackend>>(
          new GLImageProcessorBackend(input_config, output_config,
                                      OutputMode::IMPORT, relative_rotation,
                                      std::move(error_cb),
                                      std::move(backend_task_runner)));

  // Initialize GLImageProcessorBackend on the |backend_task_runner_| so that
  // the GL context is bound to the right thread and all the shaders are
  // compiled before we start processing frames. base::Unretained is safe in
  // this circumstance because we block the thread on InitializeTask(),
  // preventing our local variables from being deallocated too soon.
  bool success = false;
  base::WaitableEvent done;
  image_processor->backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GLImageProcessorBackend::InitializeTask,
                     base::Unretained(image_processor.get()),
                     base::Unretained(&done), base::Unretained(&success)));
  done.Wait();
  if (!success) {
    return nullptr;
  }
  return std::move(image_processor);
}

void GLImageProcessorBackend::InitializeTask(base::WaitableEvent* done,
                                             bool* success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  // Create a driver-level GL context just for us. This is questionable because
  // work in this context will be competing with the context(s) used for
  // rasterization and compositing. However, it's a simple starting point.
  gl_surface_ =
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), gfx::Size());
  if (!gl_surface_) {
    LOG(ERROR) << "Could not create the offscreen EGL surface";
    done->Signal();
    return;
  }
  gl::GLContextAttribs attribs{};
  attribs.can_skip_validation = true;
  attribs.context_priority = gl::ContextPriorityMedium;
  attribs.angle_context_virtualization_group_number =
      gl::AngleContextVirtualizationGroup::kGLImageProcessor;
  gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(), attribs);
  if (!gl_context_) {
    LOG(ERROR) << "Could not create the GL context";
    done->Signal();
    return;
  }
  if (!gl_context_->MakeCurrent(gl_surface_.get())) {
    LOG(ERROR) << "Could not make the GL context current";
    done->Signal();
    return;
  }

  // The GL_EXT_YUV_target extension is needed for using a YUV texture (target =
  // GL_TEXTURE_EXTERNAL_OES) as a rendering target.
  if (!gl_context_->HasExtension("GL_EXT_YUV_target")) {
    LOG(ERROR) << "The context doesn't support GL_EXT_YUV_target";
    done->Signal();
    return;
  }

  const gfx::Size input_visible_size = input_config_.visible_rect.size();
  const gfx::Size output_visible_size = output_config_.visible_rect.size();
  GLint max_texture_size;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
  if (max_texture_size < input_visible_size.width() ||
      max_texture_size < input_visible_size.height() ||
      max_texture_size < output_visible_size.width() ||
      max_texture_size < output_visible_size.height()) {
    LOG(ERROR)
        << "Either the input or output size exceeds the maximum texture size";
    done->Signal();
    return;
  }

  // Create an output texture: this will be used as the color attachment for the
  // framebuffer and will be eventually attached to the output dma-buf. Since we
  // won't sample from it, we don't need to set parameters.
  glGenFramebuffersEXT(1, &fb_id_);
  glGenTextures(1, &dst_texture_id_);

  // Create a shader program to convert an MM21 buffer into an NV12 buffer.
  GLuint program = glCreateProgram();
  constexpr GLchar kVertexShader[] =
      "#version 300 es\n"
      "out vec2 texPos;\n"
      "void main() {\n"
      "  vec2 pos[4];\n"
      "  pos[0] = vec2(-1.0, -1.0);\n"
      "  pos[1] = vec2(1.0, -1.0);\n"
      "  pos[2] = vec2(-1.0, 1.0);\n"
      "  pos[3] = vec2(1.0, 1.0);\n"
      "  gl_Position.xy = pos[gl_VertexID];\n"
      "  gl_Position.zw = vec2(0.0, 1.0);\n"
      "  vec2 uvs[4];\n"
      "  uvs[0] = vec2(0.0, 0.0);\n"
      "  uvs[1] = vec2(1.0, 0.0);\n"
      "  uvs[2] = vec2(0.0, 1.0);\n"
      "  uvs[3] = vec2(1.0, 1.0);\n"
      "  texPos = uvs[gl_VertexID];\n"
      "}\n";
  if (!CreateAndAttachShader(program, GL_VERTEX_SHADER, kVertexShader,
                             sizeof(kVertexShader))) {
    LOG(ERROR) << "Could not compile the vertex shader";
    done->Signal();
    return;
  }

  // Detiling fragment shader. Notice how we have to sample the Y and UV channel
  // separately. This is because the driver calculates UV coordinates by simply
  // dividing the Y coordinates by 2, but this results in subtle UV plane
  // artifacting, since we should really be dividing by 2 before calculating the
  // detiled coordinates. In practice, this second sample pass usually hits the
  // GPU's cache, so this doesn't influence DRAM bandwidth too negatively.
  constexpr GLchar kFragmentShader[] =
      R"(#version 300 es
      #extension GL_EXT_YUV_target : require
      #pragma disable_alpha_to_coverage
      precision mediump float;
      precision mediump int;
      uniform __samplerExternal2DY2YEXT tex;
      const uvec2 kYTileDims = uvec2(16, 32);
      const uvec2 kUVTileDims = uvec2(8, 16);
      uniform uint width;
      uniform uint height;
      in vec2 texPos;
      layout(yuv) out vec4 fragColor;
      void main() {
        uvec2 iCoord = uvec2(gl_FragCoord.xy);
        uvec2 tileCoords = iCoord / kYTileDims;
        uint numTilesPerRow = width / kYTileDims.x;
        uint tileIdx = (tileCoords.y * numTilesPerRow) + tileCoords.x;
        uvec2 inTileCoord = iCoord % kYTileDims;
        uint offsetInTile = (inTileCoord.y * kYTileDims.x) + inTileCoord.x;
        highp uint linearIndex = tileIdx;
        linearIndex = linearIndex * kYTileDims.x;
        linearIndex = linearIndex * kYTileDims.y;
        linearIndex = linearIndex + offsetInTile;
        uint detiledY = linearIndex / width;
        uint detiledX = linearIndex % width;
        fragColor = vec4(0, 0, 0, 1);
        fragColor.r = texelFetch(tex, ivec2(detiledX, detiledY), 0).r;
        iCoord = iCoord / uint(2);
        tileCoords = iCoord / kUVTileDims;
        uint uvWidth = width / uint(2);
        numTilesPerRow = uvWidth / kUVTileDims.x;
        tileIdx = (tileCoords.y * numTilesPerRow) + tileCoords.x;
        inTileCoord = iCoord % kUVTileDims;
        offsetInTile = (inTileCoord.y * kUVTileDims.x) + inTileCoord.x;
        linearIndex = tileIdx;
        linearIndex = linearIndex * kUVTileDims.x;
        linearIndex = linearIndex * kUVTileDims.y;
        linearIndex = linearIndex + offsetInTile;
        detiledY = linearIndex / uvWidth;
        detiledX = linearIndex % uvWidth;
        detiledY = detiledY * uint(2);
        detiledX = detiledX * uint(2);
        fragColor.gb = texelFetch(tex, ivec2(detiledX, detiledY), 0).gb;
      })";
  if (!CreateAndAttachShader(program, GL_FRAGMENT_SHADER, kFragmentShader,
                             sizeof(kFragmentShader))) {
    LOG(ERROR) << "Could not compile the fragment shader";
    done->Signal();
    return;
  }

  glLinkProgram(program);
  GLint result = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &result);
  if (!result) {
    constexpr GLsizei kLogBufferSize = 4096;
    char log[kLogBufferSize];
    glGetShaderInfoLog(program, kLogBufferSize, nullptr, log);
    LOG(ERROR) << "Could not link the GL program" << log;
    done->Signal();
    return;
  }
  glUseProgram(program);
  glDeleteProgram(program);

  // Create an input texture. This will be eventually attached to the input
  // dma-buf and we will sample from it, so we need to set some parameters.
  glGenTextures(1, &src_texture_id_);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, src_texture_id_);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glUniform1i(glGetUniformLocation(program, "tex"), 0);
  glUniform1ui(glGetUniformLocation(program, "width"),
               ALIGN(output_visible_size.width(), kTileWidth));
  glUniform1ui(glGetUniformLocation(program, "height"),
               ALIGN(output_visible_size.height(), kTileHeight));
  glViewport(0, 0, output_visible_size.width(), output_visible_size.height());

  // This glGetError() blocks until all the commands above have executed. This
  // should be okay because initialization only happens once.
  const GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    LOG(ERROR) << "Could not initialize the GL image processor: "
               << gl::GLEnums::GetStringError(error);
    done->Signal();
    return;
  }

  LOG(ERROR) << "Initialized a GLImageProcessorBackend: input size = "
             << input_visible_size.ToString()
             << ", output size = " << output_visible_size.ToString();
  *success = true;
  done->Signal();
}

// Note that the ImageProcessor calls the destructor from the
// backend_task_runner, so this should be threadsafe.
GLImageProcessorBackend::~GLImageProcessorBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  if (gl_context_->MakeCurrent(gl_surface_.get())) {
    glDeleteTextures(1, &src_texture_id_);
    glDeleteTextures(1, &dst_texture_id_);
    glDeleteFramebuffersEXT(1, &fb_id_);
    gl_context_->ReleaseCurrent(gl_surface_.get());
    gl_surface_->HasOneRef();
    gl_context_->HasOneRef();
  }
}

void GLImageProcessorBackend::Process(scoped_refptr<VideoFrame> input_frame,
                                      scoped_refptr<VideoFrame> output_frame,
                                      FrameReadyCB cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  TRACE_EVENT0("media", "GLImageProcessorBackend::Process");
  SCOPED_UMA_HISTOGRAM_TIMER("GLImageProcessorBackend::Process");

  if (!gl_context_->MakeCurrent(gl_surface_.get())) {
    LOG(ERROR) << "Could not make the GL context current";
    error_cb_.Run();
    return;
  }

  // Import the output buffer into GL. This involves creating an EGL image,
  // attaching it to |dst_texture_id_|, and making that texture the color
  // attachment of the framebuffer. Attaching the image of a
  // GL_TEXTURE_EXTERNAL_OES texture to the framebuffer is supported by the
  // GL_EXT_YUV_target extension.
  //
  // Note that calling glFramebufferTexture2DEXT() during InitializeTask()
  // didn't work: it generates a GL error. I guess this means the texture must
  // have a valid image prior to attaching it to the framebuffer.
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, dst_texture_id_);
  auto output_image =
      CreateAndBindImage(output_frame.get(), GL_TEXTURE_EXTERNAL_OES);
  if (!output_image) {
    LOG(ERROR) << "Could not import the output buffer into GL";
    error_cb_.Run();
    return;
  }
  glBindFramebufferEXT(GL_FRAMEBUFFER, fb_id_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_EXTERNAL_OES, dst_texture_id_, 0);
  if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    LOG(ERROR) << "The GL framebuffer is incomplete";
    error_cb_.Run();
    return;
  }

  // Import the input buffer into GL. This is done after importing the output
  // buffer so that binding so that the input texture remains as the texture in
  // unit 0 (otherwise, the sampler would be sampling out of the output texture
  // which wouldn't make sense).
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, src_texture_id_);
  auto input_image =
      CreateAndBindImage(input_frame.get(), GL_TEXTURE_EXTERNAL_OES);
  if (!input_image) {
    LOG(ERROR) << "Could not import the input buffer into GL";
    error_cb_.Run();
    return;
  }

  GLuint indices[4] = {0, 1, 2, 3};
  glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, indices);

  // glFlush() is not quite sufficient, and will result in frames being output
  // out of order, so we use a full glFinish() call.
  glFinish();

  output_frame->set_timestamp(input_frame->timestamp());
  std::move(cb).Run(std::move(output_frame));
  return;
}
}  // namespace media
