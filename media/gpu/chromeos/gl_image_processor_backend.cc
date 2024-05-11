// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/gl_image_processor_backend.h"

#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "media/base/format_utils.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace media {

namespace {

#define ALIGN(x, y) (x + (y - 1)) & (~(y - 1))

ui::GLOzone& GetCurrentGLOzone() {
  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  CHECK(platform);
  ui::SurfaceFactoryOzone* surface_factory = platform->GetSurfaceFactoryOzone();
  CHECK(surface_factory);
  ui::GLOzone* gl_ozone = surface_factory->GetCurrentGLOzone();
  CHECK(gl_ozone);
  return *gl_ozone;
}

template <typename T>
base::CheckedNumeric<T> GetNV12PlaneDimension(int dimension, int plane) {
  base::CheckedNumeric<T> dimension_scaled(base::strict_cast<T>(dimension));
  if (plane == 0) {
    return dimension_scaled;
  }
  dimension_scaled += 1;
  dimension_scaled /= 2;
  return dimension_scaled;
}

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

std::unique_ptr<ui::NativePixmapGLBinding> CreateAndBindImage(
    const FrameResource* frame,
    GLenum target,
    GLuint texture_id,
    bool should_split_planes,
    int plane) {
  CHECK(plane == 0 || plane == 1);

  // Note: if this is changed to accept other formats,
  // GLImageProcessorBackend::InitializeTask() should be updated to ensure
  // GetCurrentGLOzone().CanImportNativePixmap() returns true for those formats.
  if (frame->format() != PIXEL_FORMAT_NV12) {
    LOG(ERROR) << "The frame's format is not NV12";
    return nullptr;
  }

  // Create a native pixmap from the frame's memory buffer handle. Not using
  // CreateNativePixmapDmaBuf() because we should be using the visible size.
  gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle =
      frame->CreateGpuMemoryBufferHandle();
  if (gpu_memory_buffer_handle.is_null() ||
      gpu_memory_buffer_handle.type != gfx::NATIVE_PIXMAP) {
    LOG(ERROR) << "Failed to create native GpuMemoryBufferHandle";
    return nullptr;
  }

  auto buffer_format =
      VideoPixelFormatToGfxBufferFormat(frame->layout().format());
  if (!buffer_format) {
    LOG(ERROR) << "Unexpected video frame format";
    return nullptr;
  }

  if (!should_split_planes) {
    auto native_pixmap = base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
        frame->coded_size(), *buffer_format,
        std::move(gpu_memory_buffer_handle.native_pixmap_handle));
    DCHECK(native_pixmap->AreDmaBufFdsValid());

    // Import the NativePixmap into GL.
    return GetCurrentGLOzone().ImportNativePixmap(
        std::move(native_pixmap), gfx::BufferFormat::YUV_420_BIPLANAR,
        gfx::BufferPlane::DEFAULT, frame->coded_size(), gfx::ColorSpace(),
        target, texture_id);
  }

  base::CheckedNumeric<int> uv_width(0);
  base::CheckedNumeric<int> uv_height(0);

  if (plane == 1) {
    uv_width = GetNV12PlaneDimension<int>(frame->coded_size().width(), plane);
    uv_height = GetNV12PlaneDimension<int>(frame->coded_size().height(), plane);

    if (!uv_width.IsValid() || !uv_height.IsValid()) {
      LOG(ERROR) << "Could not compute the UV plane's dimensions";
      return nullptr;
    }
  }

  const gfx::Size plane_size =
      plane ? gfx::Size(uv_width.ValueOrDie(), uv_height.ValueOrDie())
            : frame->coded_size();

  const gfx::BufferFormat plane_format =
      plane ? gfx::BufferFormat::RG_88 : gfx::BufferFormat::R_8;

  auto native_pixmap = base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
      plane_size, plane_format,
      std::move(gpu_memory_buffer_handle.native_pixmap_handle));
  DCHECK(native_pixmap->AreDmaBufFdsValid());

  // Import the NativePixmap into GL.
  return GetCurrentGLOzone().ImportNativePixmap(
      std::move(native_pixmap), plane_format,
      plane ? gfx::BufferPlane::UV : gfx::BufferPlane::Y, plane_size,
      gfx::ColorSpace(), target, texture_id);
}

}  // namespace

GLImageProcessorBackend::GLImageProcessorBackend(
    const PortConfig& input_config,
    const PortConfig& output_config,
    OutputMode output_mode,
    ErrorCB error_cb)
    : ImageProcessorBackend(
          input_config,
          output_config,
          output_mode,
          std::move(error_cb),
          // Note: we use a single thread task runner because the GL context is
          // thread local, so we need to make sure we run the
          // GLImageProcessorBackend on the same thread always.
          base::ThreadPool::CreateSingleThreadTaskRunner(
              {base::TaskPriority::USER_VISIBLE})) {}

std::string GLImageProcessorBackend::type() const {
  return "GLImageProcessor";
}

bool GLImageProcessorBackend::IsSupported(const PortConfig& input_config,
                                          const PortConfig& output_config) {
  // Technically speaking GLIPBackend doesn't need Ozone but it
  // relies on it to initialize GL.
  if (!ui::OzonePlatform::IsInitialized()) {
    VLOGF(2) << "The GLImageProcessorBackend needs Ozone initialized.";
    return false;
  }

  if (input_config.fourcc.ToVideoPixelFormat() != PIXEL_FORMAT_NV12 ||
      output_config.fourcc.ToVideoPixelFormat() != PIXEL_FORMAT_NV12) {
    VLOGF(2) << "The GLImageProcessorBackend only supports NV12.";
    return false;
  }

  if (!((input_config.fourcc == Fourcc(Fourcc::MM21) &&
         output_config.fourcc == Fourcc(Fourcc::NV12)) ||
        (input_config.fourcc == Fourcc(Fourcc::NV12) &&
         output_config.fourcc == Fourcc(Fourcc::NV12)) ||
        (input_config.fourcc == Fourcc(Fourcc::NM12) &&
         output_config.fourcc == Fourcc(Fourcc::NM12)))) {
    VLOGF(2) << "The GLImageProcessor only supports MM21->NV12, NV12->NV12, "
                "and NM12->NM12.";
    return false;
  }

  if (input_config.visible_rect != output_config.visible_rect &&
      input_config.fourcc == Fourcc(Fourcc::MM21)) {
    VLOGF(2)
        << "The GLImageProcessorBackend only supports scaling for NV12->NV12.";
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

  if (input_config.fourcc == Fourcc(Fourcc::MM21) &&
      ((input_config.size.width() & (kTileWidth - 1)) ||
       (input_config.size.height() & (kTileHeight - 1)))) {
    VLOGF(2) << "The input frame coded size (" << input_config.size.ToString()
             << ") is not aligned to the MM21 tile dimensions (" << kTileWidth
             << "x" << kTileHeight << ").";
    return false;
  }

  return true;
}

// static
std::unique_ptr<ImageProcessorBackend> GLImageProcessorBackend::Create(
    const PortConfig& input_config,
    const PortConfig& output_config,
    OutputMode output_mode,
    ErrorCB error_cb) {
  DCHECK_EQ(output_mode, OutputMode::IMPORT);

  if (!IsSupported(input_config, output_config)) {
    return nullptr;
  }

  auto image_processor =
      std::unique_ptr<GLImageProcessorBackend,
                      std::default_delete<ImageProcessorBackend>>(
          new GLImageProcessorBackend(input_config, output_config,
                                      OutputMode::IMPORT, std::move(error_cb)));

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

  // InitializeTask() should only be called on a freshly constructed
  // GLImageProcessorBackend, so there shouldn't be any GL errors yet.
  CHECK(!got_unrecoverable_gl_error_);

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

  // CreateAndBindImage() will need to call
  // GetCurrentGLOzone().ImportNativePixmap() for NV12 frames, so we should
  // ensure that's supported.
  if (!GetCurrentGLOzone().CanImportNativePixmap(
          gfx::BufferFormat::YUV_420_BIPLANAR)) {
    LOG(ERROR) << "Importing NV12 buffers is not supported";
    done->Signal();
    return;
  }

  // Ensure we can use EGLImage objects as texture images and that we can use
  // the GL_TEXTURE_EXTERNAL_OES target.
  if (!gl_context_->HasExtension("GL_OES_EGL_image")) {
    LOG(ERROR) << "The context doesn't support GL_OES_EGL_image";
    done->Signal();
    return;
  }
  if (!gl_context_->HasExtension("GL_OES_EGL_image_external")) {
    LOG(ERROR) << "The context doesn't support GL_OES_EGL_image_external";
    done->Signal();
    return;
  }

  // Ensure the coded size and visible rectangle are reasonable.
  const gfx::Size input_coded_size = input_config_.size;
  const gfx::Size output_coded_size = output_config_.size;
  if (input_coded_size.IsEmpty() || output_coded_size.IsEmpty()) {
    LOG(ERROR) << "Either the input or output coded size is empty";
    done->Signal();
    return;
  }
  if (!VideoFrame::IsValidCodedSize(input_coded_size) ||
      !VideoFrame::IsValidCodedSize(output_coded_size)) {
    LOG(ERROR) << "Either the input or output coded size is invalid";
    done->Signal();
    return;
  }
  if (input_config_.visible_rect.IsEmpty() ||
      output_config_.visible_rect.IsEmpty()) {
    LOG(ERROR) << "Either the input or output visible rectangle is empty";
    done->Signal();
    return;
  }
  if (!gfx::Rect(input_coded_size).Contains(input_config_.visible_rect) ||
      !gfx::Rect(output_coded_size).Contains(output_config_.visible_rect)) {
    LOG(ERROR) << "Either the input or output visible rectangle is invalid";
    done->Signal();
    return;
  }

  // Note: we use the coded size to import the frames later in
  // CreateAndBindImage(), so we need to check that size against
  // GL_MAX_TEXTURE_SIZE.
  GLint max_texture_size = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
  if (max_texture_size < base::strict_cast<GLint>(input_coded_size.width()) ||
      max_texture_size < base::strict_cast<GLint>(input_coded_size.height()) ||
      max_texture_size < base::strict_cast<GLint>(output_coded_size.width()) ||
      max_texture_size < base::strict_cast<GLint>(output_coded_size.height())) {
    LOG(ERROR) << "Either the input or output coded size exceeds the maximum "
                  "texture size";
    done->Signal();
    return;
  }

  // Create an output texture: this will be used as the color attachment for the
  // framebuffer and will be eventually attached to the output dma-buf. Since we
  // won't sample from it, we don't need to set parameters.
  glGenFramebuffersEXT(1, &fb_id_);
  CHECK_GT(fb_id_, 0u);
  glGenTextures(1, &dst_texture_id_);
  CHECK_GT(dst_texture_id_, 0u);

  // These calculations are used to calculate vertices such that
  // regions that were meant to be cropped out would be clipped out
  // by GL naturally. GL's vertex space is from [-1 1] and everything outside
  // will be clipped out.
  //
  // To do this, the absolute coordinate space vertices of the texture:
  //    Example: |input_config_.visible_rect.x()|
  //
  // These absolute coordinate space vertices are converted to relative
  // space texture coordinates:
  //    Example: |input_left / input_width|
  const GLfloat input_width =
      base::checked_cast<GLfloat>(input_config_.visible_rect.width());
  const GLfloat input_height =
      base::checked_cast<GLfloat>(input_config_.visible_rect.height());
  const GLfloat input_coded_width =
      base::checked_cast<GLfloat>(input_coded_size.width());
  const GLfloat input_coded_height =
      base::checked_cast<GLfloat>(input_coded_size.height());

  const GLfloat input_left =
      base::checked_cast<GLfloat>(input_config_.visible_rect.x());
  const GLfloat input_top =
      base::checked_cast<GLfloat>(input_config_.visible_rect.y());

  const GLfloat normalized_left = input_left / input_width;
  const GLfloat normalized_top = input_top / input_height;

  const GLfloat x_start = -1.0f - normalized_left * 2.0f;

  const GLfloat x_end = 1.0f + (input_coded_width - input_width - input_left) /
                                   input_width * 2.0f;

  const GLfloat y_start = -1.0f - normalized_top * 2.0f;

  const GLfloat y_end = 1.0f + (input_coded_height - input_height - input_top) /
                                   input_height * 2.0f;

  const GLfloat vertices[] = {
      // clang-format off
      x_end,   y_start, 0.0f,
      x_end,   y_end,   0.0f,
      x_start, y_start, 0.0f,
      x_start, y_end,   0.0f
      // clang-format on
  };

  glGenBuffersARB(1, &vbo_id_);
  CHECK_GT(vbo_id_, 0u);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_id_);
  glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);

  glGenVertexArraysOES(1, &vao_id_);
  CHECK_GT(vao_id_, 0u);
  glBindVertexArrayOES(vao_id_);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(0);

  // Create a vertex shader program which will be used for both scaling and
  // conversion shader programs.
  GLuint program = glCreateProgram();
  constexpr GLchar kVertexShader[] =
      "#version 300 es\n"
      "layout(location = 0) in vec3 vertex_position;\n"
      "out vec2 texPos;\n"
      "void main() {\n"
      "  gl_Position = vec4(vertex_position, 1.0);\n"
      "  vec2 uvs[4];\n"
      "  uvs[0] = vec2(1.0, 0.0);\n"
      "  uvs[1] = vec2(1.0, 1.0);\n"
      "  uvs[2] = vec2(0.0, 0.0);\n"
      "  uvs[3] = vec2(0.0, 1.0);\n"
      "  texPos = uvs[gl_VertexID];\n"
      "}\n";
  if (!CreateAndAttachShader(program, GL_VERTEX_SHADER, kVertexShader,
                             sizeof(kVertexShader))) {
    LOG(ERROR) << "Could not compile the vertex shader";
    done->Signal();
    return;
  }

  const bool scaling = (input_config_.fourcc == Fourcc(Fourcc::NV12) &&
                        output_config_.fourcc == Fourcc(Fourcc::NV12)) ||
                       (input_config_.fourcc == Fourcc(Fourcc::NM12) &&
                        output_config_.fourcc == Fourcc(Fourcc::NM12));

  if (scaling) {
    // Creates a fragment shader program to do NV12 scaling.
    constexpr GLchar kFragmentShader[] =
        R"(#version 300 es
          precision mediump float;
          precision mediump int;
          uniform sampler2D videoTexture;
          in vec2 texPos;
          layout(location=0) out vec4 fragColour;
          void main() {
            fragColour = texture(videoTexture, texPos);
        })";
    if (!CreateAndAttachShader(program, GL_FRAGMENT_SHADER, kFragmentShader,
                               sizeof(kFragmentShader))) {
      LOG(ERROR) << "Could not compile the fragment shader";
      done->Signal();
      return;
    }
  } else {
    // The GL_EXT_YUV_target extension is needed for using a YUV texture (target
    // = GL_TEXTURE_EXTERNAL_OES) as a rendering target.
    CHECK_EQ(input_config_.fourcc, Fourcc(Fourcc::MM21));
    if (!gl_context_->HasExtension("GL_EXT_YUV_target")) {
      LOG(ERROR) << "The context doesn't support GL_EXT_YUV_target";
      done->Signal();
      return;
    }

    // Creates a shader program to convert an MM21 buffer into an NV12 buffer.
    // Detiling fragment shader. Notice how we have to sample the Y and UV
    // channel separately. This is because the driver calculates UV coordinates
    // by simply dividing the Y coordinates by 2, but this results in subtle UV
    // plane artifacting, since we should really be dividing by 2 before
    // calculating the detiled coordinates. In practice, this second sample pass
    // usually hits the GPU's cache, so this doesn't influence DRAM bandwidth
    // too negatively.
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
  }

  glLinkProgram(program);
  glBindAttribLocation(program, 0, "vertex_position");

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
  CHECK_GT(src_texture_id_, 0u);
  const auto gl_texture_target =
      scaling ? GL_TEXTURE_2D : GL_TEXTURE_EXTERNAL_OES;
  const auto gl_texture_filter = scaling ? GL_LINEAR : GL_NEAREST;
  glBindTexture(gl_texture_target, src_texture_id_);
  glTexParameteri(gl_texture_target, GL_TEXTURE_MIN_FILTER, gl_texture_filter);
  glTexParameteri(gl_texture_target, GL_TEXTURE_MAG_FILTER, gl_texture_filter);
  glTexParameteri(gl_texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(gl_texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (!scaling) {
    glUniform1i(glGetUniformLocation(program, "tex"), 0);
    glUniform1ui(glGetUniformLocation(program, "width"),
                 base::checked_cast<GLuint>(
                     ALIGN(output_config_.visible_rect.width(), kTileWidth)));
    glUniform1ui(glGetUniformLocation(program, "height"),
                 base::checked_cast<GLuint>(
                     ALIGN(output_config_.visible_rect.height(), kTileHeight)));
  }

  // Ensure the GLImageProcessorBackend is fully initialized by blocking until
  // all the commands above have completed. This should be okay because
  // initialization only happens once.
  glFinish();
  GLenum last_gl_error = GL_NO_ERROR;
  bool gl_error_occurred = false;
  while ((last_gl_error = glGetError()) != GL_NO_ERROR) {
    if (last_gl_error == GL_OUT_OF_MEMORY ||
        last_gl_error == GL_CONTEXT_LOST_KHR) {
      got_unrecoverable_gl_error_ = true;
    }
    gl_error_occurred = true;
    VLOGF(2) << "Got a GL error: "
             << gl::GLEnums::GetStringError(last_gl_error);
  }
  if (gl_error_occurred) {
    LOG(ERROR)
        << "Could not initialize the GL image processor due to GL errors";
    done->Signal();
    return;
  }

  VLOGF(1) << "Initialized a GLImageProcessorBackend: input coded size = "
           << input_coded_size.ToString() << ", input visible rectangle = "
           << input_config_.visible_rect.ToString()
           << ", output coded size = " << output_coded_size.ToString()
           << ", output visible rectangle = "
           << output_config_.visible_rect.ToString();
  *success = true;
  done->Signal();
}

// Note that the ImageProcessor deletes the ImageProcessorBackend on the
// |backend_task_runner_| so this should be thread-safe.
//
// TODO(b/339883058): do we need to explicitly call |gl_surface_|->Destroy()?
GLImageProcessorBackend::~GLImageProcessorBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  if (!gl_surface_) {
    // If there's no surface, nothing else was created.
    CHECK(!gl_context_);
    return;
  }
  if (!gl_context_) {
    // If there's no context, nothing else was created.
    return;
  }
  // In case of an unrecoverable GL error, let's assume the GL state machine is
  // in an undefined state such that it's unsafe to issue further commands.
  if (got_unrecoverable_gl_error_) {
    return;
  }
  if (!gl_context_->MakeCurrent(gl_surface_.get())) {
    // If the context can't be made current, we shouldn't do anything else.
    return;
  }
  if (fb_id_) {
    glDeleteFramebuffersEXT(1, &fb_id_);
  }
  if (dst_texture_id_) {
    glDeleteTextures(1, &dst_texture_id_);
  }
  if (src_texture_id_) {
    glDeleteTextures(1, &src_texture_id_);
  }
  if (vao_id_) {
    glDeleteVertexArraysOES(1, &vao_id_);
  }
  if (vbo_id_) {
    glDeleteBuffersARB(1, &vbo_id_);
  }
  gl_context_->ReleaseCurrent(gl_surface_.get());
}

void GLImageProcessorBackend::ProcessFrame(
    scoped_refptr<FrameResource> input_frame,
    scoped_refptr<FrameResource> output_frame,
    FrameResourceReadyCB cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  TRACE_EVENT2("media", "GLImageProcessorBackend::ProcessFrame", "input_frame",
               input_frame->AsHumanReadableString(), "output_frame",
               output_frame->AsHumanReadableString());
  SCOPED_UMA_HISTOGRAM_TIMER("GLImageProcessorBackend::Process");

  // In the case of unrecoverable GL errors, we assume it's unsafe to issue
  // further commands.
  if (got_unrecoverable_gl_error_) {
    VLOGF(2) << "Earlying out because an unrecoverable GL error was detected";
    error_cb_.Run();
    return;
  }

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

  const bool scaling = (input_config_.fourcc == Fourcc(Fourcc::NV12) &&
                        output_config_.fourcc == Fourcc(Fourcc::NV12)) ||
                       (input_config_.fourcc == Fourcc(Fourcc::NM12) &&
                        output_config_.fourcc == Fourcc(Fourcc::NM12));

  const int num_planes = scaling ? 2 : 1;
  const auto gl_texture_target =
      scaling ? GL_TEXTURE_2D : GL_TEXTURE_EXTERNAL_OES;
  for (int plane = 0; plane < num_planes; plane++) {
    base::CheckedNumeric<GLsizei> output_width = GetNV12PlaneDimension<GLsizei>(
        output_frame->visible_rect().width(), plane);
    base::CheckedNumeric<GLsizei> output_height =
        GetNV12PlaneDimension<GLsizei>(output_frame->visible_rect().height(),
                                       plane);
    if (!output_width.IsValid() || !output_height.IsValid()) {
      LOG(ERROR) << "Could not calculate the viewport dimensions";
      error_cb_.Run();
      return;
    }
    glViewport(0, 0, output_width.ValueOrDie(), output_height.ValueOrDie());

    glBindTexture(gl_texture_target, dst_texture_id_);
    auto output_image_binding = CreateAndBindImage(
        output_frame.get(), gl_texture_target, dst_texture_id_,
        /*should_split_planes=*/scaling, plane);
    if (!output_image_binding) {
      LOG(ERROR) << "Could not import the output buffer into GL";
      error_cb_.Run();
      return;
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER, fb_id_);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              gl_texture_target, dst_texture_id_, 0);
    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE) {
      LOG(ERROR) << "The GL framebuffer is incomplete";
      error_cb_.Run();
      return;
    }

    // Import the input buffer into GL. This is done after importing the output
    // buffer so that binding so that the input texture remains as the texture
    // in unit 0 (otherwise, the sampler would be sampling out of the output
    // texture which wouldn't make sense).
    glBindTexture(gl_texture_target, src_texture_id_);

    auto input_image_binding = CreateAndBindImage(
        input_frame.get(), gl_texture_target, src_texture_id_,
        /*should_split_planes=*/scaling, plane);
    if (!input_image_binding) {
      LOG(ERROR) << "Could not import the input buffer into GL";
      error_cb_.Run();
      return;
    }

    GLuint indices[4] = {0, 1, 2, 3};
    glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, indices);
  }

  // glFlush() is not quite sufficient and will result in frames being output
  // out of order, so we use a full glFinish() call.
  //
  // TODO(bchoobineh): add proper synchronization that does not require blocking
  // the CPU.
  glFinish();

  // Check if any errors occurred. Note that we call glGetError() in a loop to
  // clear all error flags.
  GLenum last_gl_error = GL_NO_ERROR;
  bool gl_error_occurred = false;
  while ((last_gl_error = glGetError()) != GL_NO_ERROR) {
    if (last_gl_error == GL_OUT_OF_MEMORY ||
        last_gl_error == GL_CONTEXT_LOST_KHR) {
      got_unrecoverable_gl_error_ = true;
    }
    gl_error_occurred = true;
    VLOGF(2) << "Got a GL error: "
             << gl::GLEnums::GetStringError(last_gl_error);
  }
  if (gl_error_occurred) {
    LOG(ERROR) << "Could not process a frame due to one or more GL errors";
    error_cb_.Run();
    return;
  }

  output_frame->set_timestamp(input_frame->timestamp());
  std::move(cb).Run(std::move(output_frame));
  return;
}

}  // namespace media
