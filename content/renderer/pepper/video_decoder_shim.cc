// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/video_decoder_shim.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/queue.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/pepper/pepper_video_decoder_host.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/vpx_video_decoder.h"
#include "media/media_buildflags.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/pp_errors.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/skia/include/gpu/GrTypes.h"

namespace content {

namespace {

bool IsCodecSupported(media::VideoCodec codec) {
#if BUILDFLAG(ENABLE_LIBVPX)
  if (codec == media::kCodecVP9)
    return true;
#endif

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  return media::FFmpegVideoDecoder::IsCodecSupported(codec);
#else
  return false;
#endif
}

}  // namespace

// YUV->RGB converter class using a shader and FBO.
class VideoDecoderShim::YUVConverter {
 public:
  YUVConverter(scoped_refptr<viz::ContextProviderCommandBuffer>);
  ~YUVConverter();
  bool Initialize();
  void Convert(const media::VideoFrame* frame, GLuint tex_out);

 private:
  GLuint CreateShader();
  GLuint CompileShader(const char* name, GLuint type, const char* code);
  GLuint CreateProgram(const char* name, GLuint vshader, GLuint fshader);
  GLuint CreateTexture();

  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  gpu::gles2::GLES2Interface* gl_;
  GLuint frame_buffer_;
  GLuint vertex_buffer_;
  GLuint program_;

  GLuint y_texture_;
  GLuint u_texture_;
  GLuint v_texture_;
  GLuint a_texture_;

  GLuint internal_format_;
  GLuint format_;
  media::VideoPixelFormat video_format_;

  GLuint y_width_;
  GLuint y_height_;

  GLuint uv_width_;
  GLuint uv_height_;
  uint32_t uv_height_divisor_;
  uint32_t uv_width_divisor_;

  GLint yuv_matrix_loc_;
  GLint yuv_adjust_loc_;

  DISALLOW_COPY_AND_ASSIGN(YUVConverter);
};

VideoDecoderShim::YUVConverter::YUVConverter(
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider)
    : context_provider_(std::move(context_provider)),
      gl_(context_provider_->ContextGL()),
      frame_buffer_(0),
      vertex_buffer_(0),
      program_(0),
      y_texture_(0),
      u_texture_(0),
      v_texture_(0),
      a_texture_(0),
      internal_format_(0),
      format_(0),
      video_format_(media::PIXEL_FORMAT_UNKNOWN),
      y_width_(2),
      y_height_(2),
      uv_width_(2),
      uv_height_(2),
      uv_height_divisor_(1),
      uv_width_divisor_(1),
      yuv_matrix_loc_(0),
      yuv_adjust_loc_(0) {
  DCHECK(gl_);
}

VideoDecoderShim::YUVConverter::~YUVConverter() {
  if (y_texture_)
    gl_->DeleteTextures(1, &y_texture_);

  if (u_texture_)
    gl_->DeleteTextures(1, &u_texture_);

  if (v_texture_)
    gl_->DeleteTextures(1, &v_texture_);

  if (a_texture_)
    gl_->DeleteTextures(1, &a_texture_);

  if (frame_buffer_)
    gl_->DeleteFramebuffers(1, &frame_buffer_);

  if (vertex_buffer_)
    gl_->DeleteBuffers(1, &vertex_buffer_);

  if (program_)
    gl_->DeleteProgram(program_);
}

GLuint VideoDecoderShim::YUVConverter::CreateTexture() {
  GLuint tex = 0;

  gl_->GenTextures(1, &tex);
  gl_->BindTexture(GL_TEXTURE_2D, tex);

  // Create texture with default size - will be resized upon first frame.
  gl_->TexImage2D(GL_TEXTURE_2D, 0, internal_format_, 2, 2, 0, format_,
                  GL_UNSIGNED_BYTE, nullptr);

  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gl_->BindTexture(GL_TEXTURE_2D, 0);

  return tex;
}

GLuint VideoDecoderShim::YUVConverter::CompileShader(const char* name,
                                                     GLuint type,
                                                     const char* code) {
  GLuint shader = gl_->CreateShader(type);

  gl_->ShaderSource(shader, 1, (const GLchar**)&code, nullptr);
  gl_->CompileShader(shader);

#ifndef NDEBUG
  GLint status = 0;

  gl_->GetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    GLint max_length = 0;
    GLint actual_length = 0;
    gl_->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_length);

    // The max_length includes the NULL character.
    std::string error_log(max_length, 0);
    gl_->GetShaderInfoLog(shader, max_length, &actual_length, &error_log[0]);

    LOG(ERROR) << name << " shader compilation failed: " << error_log.c_str();
    gl_->DeleteShader(shader);
    return 0;
  }
#endif

  return shader;
}

GLuint VideoDecoderShim::YUVConverter::CreateProgram(const char* name,
                                                     GLuint vshader,
                                                     GLuint fshader) {
  GLuint program = gl_->CreateProgram();
  gl_->AttachShader(program, vshader);
  gl_->AttachShader(program, fshader);

  gl_->BindAttribLocation(program, 0, "position");

  gl_->LinkProgram(program);

#ifndef NDEBUG
  GLint status = 0;

  gl_->GetProgramiv(program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    GLint max_length = 0;
    GLint actual_length = 0;
    gl_->GetProgramiv(program, GL_INFO_LOG_LENGTH, &max_length);

    // The max_length includes the NULL character.
    std::string error_log(max_length, 0);
    gl_->GetProgramInfoLog(program, max_length, &actual_length, &error_log[0]);

    LOG(ERROR) << name << " program linking failed: " << error_log.c_str();
    return 0;
  }
#endif

  return program;
}

GLuint VideoDecoderShim::YUVConverter::CreateShader() {
  const char* vert_shader =
      "precision mediump float;\n"
      "attribute vec2 position;\n"
      "varying vec2 texcoord;\n"
      "void main()\n"
      "{\n"
      "    gl_Position = vec4( position.xy, 0, 1 );\n"
      "    texcoord = position*0.5+0.5;\n"
      "}";

  const char* frag_shader =
      "precision mediump float;\n"
      "varying vec2 texcoord;\n"
      "uniform sampler2D y_sampler;\n"
      "uniform sampler2D u_sampler;\n"
      "uniform sampler2D v_sampler;\n"
      "uniform sampler2D a_sampler;\n"
      "uniform mat3 yuv_matrix;\n"
      "uniform vec3 yuv_adjust;\n"
      "void main()\n"
      "{\n"
      "  vec3 yuv = vec3(texture2D(y_sampler, texcoord).x,\n"
      "                  texture2D(u_sampler, texcoord).x,\n"
      "                  texture2D(v_sampler, texcoord).x) +\n"
      "                  yuv_adjust;\n"
      "  gl_FragColor = vec4(yuv_matrix * yuv, texture2D(a_sampler, "
      "texcoord).x);\n"
      "}";

  GLuint vertex_shader =
      CompileShader("Vertex Shader", GL_VERTEX_SHADER, vert_shader);
  if (!vertex_shader) {
    return 0;
  }

  GLuint fragment_shader =
      CompileShader("Fragment Shader", GL_FRAGMENT_SHADER, frag_shader);
  if (!fragment_shader) {
    gl_->DeleteShader(vertex_shader);
    return 0;
  }

  GLuint program =
      CreateProgram("YUVConverter Program", vertex_shader, fragment_shader);

  gl_->DeleteShader(vertex_shader);
  gl_->DeleteShader(fragment_shader);

  if (!program) {
    return 0;
  }

  gl_->UseProgram(program);

  GLint uniform_location;
  uniform_location = gl_->GetUniformLocation(program, "y_sampler");
  DCHECK(uniform_location != -1);
  gl_->Uniform1i(uniform_location, 0);

  uniform_location = gl_->GetUniformLocation(program, "u_sampler");
  DCHECK(uniform_location != -1);
  gl_->Uniform1i(uniform_location, 1);

  uniform_location = gl_->GetUniformLocation(program, "v_sampler");
  DCHECK(uniform_location != -1);
  gl_->Uniform1i(uniform_location, 2);

  uniform_location = gl_->GetUniformLocation(program, "a_sampler");
  DCHECK(uniform_location != -1);
  gl_->Uniform1i(uniform_location, 3);

  gl_->UseProgram(0);

  yuv_matrix_loc_ = gl_->GetUniformLocation(program, "yuv_matrix");
  DCHECK(yuv_matrix_loc_ != -1);

  yuv_adjust_loc_ = gl_->GetUniformLocation(program, "yuv_adjust");
  DCHECK(yuv_adjust_loc_ != -1);

  return program;
}

bool VideoDecoderShim::YUVConverter::Initialize() {
  // If texture_rg extension is not available, use slower GL_LUMINANCE.
  if (context_provider_->ContextCapabilities().texture_rg) {
    internal_format_ = GL_RED_EXT;
    format_ = GL_RED_EXT;
  } else {
    internal_format_ = GL_LUMINANCE;
    format_ = GL_LUMINANCE;
  }

  if (context_provider_->ContextCapabilities().max_texture_image_units < 4) {
    // We support YUVA textures and require 4 texture units in the fragment
    // stage.
    return false;
  }

  gl_->TraceBeginCHROMIUM("YUVConverter", "YUVConverterContext");
  gl_->GenFramebuffers(1, &frame_buffer_);

  y_texture_ = CreateTexture();
  u_texture_ = CreateTexture();
  v_texture_ = CreateTexture();
  a_texture_ = CreateTexture();

  // Vertex positions.  Also converted to texcoords in vertex shader.
  GLfloat vertex_positions[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f};

  gl_->GenBuffers(1, &vertex_buffer_);
  gl_->BindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  gl_->BufferData(GL_ARRAY_BUFFER, 2 * sizeof(GLfloat) * 4, vertex_positions,
                  GL_STATIC_DRAW);
  gl_->BindBuffer(GL_ARRAY_BUFFER, 0);

  program_ = CreateShader();

  gl_->TraceEndCHROMIUM();

  return (program_ != 0);
}

void VideoDecoderShim::YUVConverter::Convert(const media::VideoFrame* frame,
                                             GLuint tex_out) {
  const float* yuv_matrix = nullptr;
  const float* yuv_adjust = nullptr;

  if (video_format_ != frame->format()) {
    // The constants below were taken from
    // components/viz/service/display/gl_renderer.cc. These values are magic
    // numbers that are used in the transformation from YUV to RGB color values.
    // They are taken from the following webpage:
    // http://www.fourcc.org/fccyvrgb.php
    static const float yuv_to_rgb_rec601[9] = {
        1.164f, 1.164f, 1.164f, 0.0f, -.391f, 2.018f, 1.596f, -.813f, 0.0f,
    };
    static const float yuv_to_rgb_jpeg[9] = {
        1.f, 1.f, 1.f, 0.0f, -.34414f, 1.772f, 1.402f, -.71414f, 0.0f,
    };
    static const float yuv_to_rgb_rec709[9] = {
        1.164f, 1.164f, 1.164f, 0.0f, -0.213f, 2.112f, 1.793f, -0.533f, 0.0f,
    };

    // These values map to 16, 128, and 128 respectively, and are computed
    // as a fraction over 256 (e.g. 16 / 256 = 0.0625).
    // They are used in the YUV to RGBA conversion formula:
    //   Y - 16   : Gives 16 values of head and footroom for overshooting
    //   U - 128  : Turns unsigned U into signed U [-128,127]
    //   V - 128  : Turns unsigned V into signed V [-128,127]
    static const float yuv_adjust_constrained[3] = {
        -0.0625f, -0.5f, -0.5f,
    };
    // Same as above, but without the head and footroom.
    static const float yuv_adjust_full[3] = {
        0.0f, -0.5f, -0.5f,
    };

    yuv_adjust = yuv_adjust_constrained;
    // TODO(hubbe): Should default to 709
    yuv_matrix = yuv_to_rgb_rec601;

    SkYUVColorSpace sk_yuv_color_space;
    if (frame->ColorSpace().ToSkYUVColorSpace(&sk_yuv_color_space)) {
      switch (sk_yuv_color_space) {
        case kJPEG_SkYUVColorSpace:
          yuv_matrix = yuv_to_rgb_jpeg;
          yuv_adjust = yuv_adjust_full;
          break;
        case kRec709_SkYUVColorSpace:
          yuv_matrix = yuv_to_rgb_rec709;
          break;
        case kRec601_SkYUVColorSpace:
          // Current default.
          break;
        default:
          NOTREACHED();
      }
    }

    switch (frame->format()) {
      case media::PIXEL_FORMAT_I420A:
      case media::PIXEL_FORMAT_I420:
        uv_height_divisor_ = 2;
        uv_width_divisor_ = 2;
        break;
      case media::PIXEL_FORMAT_I422:
        uv_width_divisor_ = 2;
        uv_height_divisor_ = 1;
        break;
      case media::PIXEL_FORMAT_I444:
        uv_width_divisor_ = 1;
        uv_height_divisor_ = 1;
        break;

      default:
        NOTREACHED();
    }

    video_format_ = frame->format();

    // Zero these so everything is reset below.
    y_width_ = y_height_ = 0;
  }

  gl_->TraceBeginCHROMIUM("YUVConverter", "YUVConverterContext");

  uint32_t ywidth = frame->coded_size().width();
  uint32_t yheight = frame->coded_size().height();

  DCHECK_EQ(frame->stride(media::VideoFrame::kUPlane),
            frame->stride(media::VideoFrame::kVPlane));

  uint32_t ystride = frame->stride(media::VideoFrame::kYPlane);
  uint32_t uvstride = frame->stride(media::VideoFrame::kUPlane);

  // The following code assumes that extended GLES 2.0 state like
  // UNPACK_SKIP* (if available) are set to defaults.
  gl_->PixelStorei(GL_UNPACK_ALIGNMENT, 1);

  if (ywidth != y_width_ || yheight != y_height_) {
    y_width_ = ywidth;
    y_height_ = yheight;

    uv_width_ = y_width_ / uv_width_divisor_;
    uv_height_ = y_height_ / uv_height_divisor_;

    // Re-create to resize the textures and upload data.
    gl_->PixelStorei(GL_UNPACK_ROW_LENGTH_EXT, ystride);
    gl_->ActiveTexture(GL_TEXTURE0);
    gl_->BindTexture(GL_TEXTURE_2D, y_texture_);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, internal_format_, y_width_, y_height_, 0,
                    format_, GL_UNSIGNED_BYTE,
                    frame->data(media::VideoFrame::kYPlane));

    if (video_format_ == media::PIXEL_FORMAT_I420A) {
      DCHECK_EQ(frame->stride(media::VideoFrame::kYPlane),
                frame->stride(media::VideoFrame::kAPlane));
      gl_->ActiveTexture(GL_TEXTURE3);
      gl_->BindTexture(GL_TEXTURE_2D, a_texture_);
      gl_->TexImage2D(GL_TEXTURE_2D, 0, internal_format_, y_width_, y_height_,
                      0, format_, GL_UNSIGNED_BYTE,
                      frame->data(media::VideoFrame::kAPlane));
    } else {
      // if there is no alpha channel, then create a 2x2 texture with full
      // alpha.
      gl_->PixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
      const uint8_t alpha[4] = {0xff, 0xff, 0xff, 0xff};
      gl_->ActiveTexture(GL_TEXTURE3);
      gl_->BindTexture(GL_TEXTURE_2D, a_texture_);
      gl_->TexImage2D(GL_TEXTURE_2D, 0, internal_format_, 2, 2, 0, format_,
                      GL_UNSIGNED_BYTE, alpha);
    }

    gl_->PixelStorei(GL_UNPACK_ROW_LENGTH_EXT, uvstride);
    gl_->ActiveTexture(GL_TEXTURE1);
    gl_->BindTexture(GL_TEXTURE_2D, u_texture_);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, internal_format_, uv_width_, uv_height_,
                    0, format_, GL_UNSIGNED_BYTE,
                    frame->data(media::VideoFrame::kUPlane));

    gl_->ActiveTexture(GL_TEXTURE2);
    gl_->BindTexture(GL_TEXTURE_2D, v_texture_);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, internal_format_, uv_width_, uv_height_,
                    0, format_, GL_UNSIGNED_BYTE,
                    frame->data(media::VideoFrame::kVPlane));
  } else {
    // Bind textures and upload texture data
    gl_->PixelStorei(GL_UNPACK_ROW_LENGTH_EXT, ystride);
    gl_->ActiveTexture(GL_TEXTURE0);
    gl_->BindTexture(GL_TEXTURE_2D, y_texture_);
    gl_->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, y_width_, y_height_, format_,
                       GL_UNSIGNED_BYTE,
                       frame->data(media::VideoFrame::kYPlane));

    if (video_format_ == media::PIXEL_FORMAT_I420A) {
      DCHECK_EQ(frame->stride(media::VideoFrame::kYPlane),
                frame->stride(media::VideoFrame::kAPlane));
      gl_->ActiveTexture(GL_TEXTURE3);
      gl_->BindTexture(GL_TEXTURE_2D, a_texture_);
      gl_->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, y_width_, y_height_, format_,
                         GL_UNSIGNED_BYTE,
                         frame->data(media::VideoFrame::kAPlane));
    } else {
      gl_->ActiveTexture(GL_TEXTURE3);
      gl_->BindTexture(GL_TEXTURE_2D, a_texture_);
    }

    gl_->PixelStorei(GL_UNPACK_ROW_LENGTH_EXT, uvstride);
    gl_->ActiveTexture(GL_TEXTURE1);
    gl_->BindTexture(GL_TEXTURE_2D, u_texture_);
    gl_->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, uv_width_, uv_height_, format_,
                       GL_UNSIGNED_BYTE,
                       frame->data(media::VideoFrame::kUPlane));

    gl_->ActiveTexture(GL_TEXTURE2);
    gl_->BindTexture(GL_TEXTURE_2D, v_texture_);
    gl_->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, uv_width_, uv_height_, format_,
                       GL_UNSIGNED_BYTE,
                       frame->data(media::VideoFrame::kVPlane));
  }

  gl_->BindFramebuffer(GL_FRAMEBUFFER, frame_buffer_);
  gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            tex_out, 0);

#ifndef NDEBUG
  // We should probably check for framebuffer complete here, but that
  // will slow this method down so check only in debug mode.
  GLint status = gl_->CheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    return;
  }
#endif

  gl_->Viewport(0, 0, ywidth, yheight);

  gl_->UseProgram(program_);

  if (yuv_matrix) {
    gl_->UniformMatrix3fv(yuv_matrix_loc_, 1, 0, yuv_matrix);
    gl_->Uniform3fv(yuv_adjust_loc_, 1, yuv_adjust);
  }

  gl_->BindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  gl_->EnableVertexAttribArray(0);
  gl_->VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                           static_cast<const void*>(nullptr));

  gl_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // The YUVConverter shares the context with Skia and possibly other modules
  // that may make OpenGL calls.  To be a "good OpenGL citizen" for other
  // (non-Skia) modules that may share this context we restore
  // buffer/texture/state bindings to OpenGL defaults here.  If we were only
  // sharing the context with Skia this may not be necessary as we also
  // Invalidate the GrContext below so that Skia is aware that its state
  // caches need to be reset.

  gl_->BindBuffer(GL_ARRAY_BUFFER, 0);
  gl_->DisableVertexAttribArray(0);
  gl_->UseProgram(0);
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);

  gl_->BindTexture(GL_TEXTURE_2D, 0);

  gl_->ActiveTexture(GL_TEXTURE2);
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  gl_->ActiveTexture(GL_TEXTURE1);
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  gl_->ActiveTexture(GL_TEXTURE0);
  gl_->BindTexture(GL_TEXTURE_2D, 0);
  gl_->PixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);

  gl_->TraceEndCHROMIUM();
}

struct VideoDecoderShim::PendingDecode {
  PendingDecode(uint32_t decode_id,
                const scoped_refptr<media::DecoderBuffer>& buffer);
  ~PendingDecode();

  const uint32_t decode_id;
  const scoped_refptr<media::DecoderBuffer> buffer;
};

VideoDecoderShim::PendingDecode::PendingDecode(
    uint32_t decode_id,
    const scoped_refptr<media::DecoderBuffer>& buffer)
    : decode_id(decode_id), buffer(buffer) {
}

VideoDecoderShim::PendingDecode::~PendingDecode() {
}

struct VideoDecoderShim::PendingFrame {
  explicit PendingFrame(uint32_t decode_id);
  PendingFrame(uint32_t decode_id, scoped_refptr<media::VideoFrame> frame);
  ~PendingFrame();

  const uint32_t decode_id;
  scoped_refptr<media::VideoFrame> video_frame;

 private:
  // This could be expensive to copy, so guard against that.
  DISALLOW_COPY_AND_ASSIGN(PendingFrame);
};

VideoDecoderShim::PendingFrame::PendingFrame(uint32_t decode_id)
    : decode_id(decode_id) {
}

VideoDecoderShim::PendingFrame::PendingFrame(
    uint32_t decode_id,
    scoped_refptr<media::VideoFrame> frame)
    : decode_id(decode_id), video_frame(std::move(frame)) {}

VideoDecoderShim::PendingFrame::~PendingFrame() {
}

// DecoderImpl runs the underlying VideoDecoder on the media thread, receiving
// calls from the VideoDecodeShim on the main thread and sending results back.
// This class is constructed on the main thread, but used and destructed on the
// media thread.
class VideoDecoderShim::DecoderImpl {
 public:
  explicit DecoderImpl(const base::WeakPtr<VideoDecoderShim>& proxy);
  ~DecoderImpl();

  void Initialize(media::VideoDecoderConfig config);
  void Decode(uint32_t decode_id, scoped_refptr<media::DecoderBuffer> buffer);
  void Reset();
  void Stop();

 private:
  void OnInitDone(bool success);
  void DoDecode();
  void OnDecodeComplete(media::DecodeStatus status);
  void OnOutputComplete(scoped_refptr<media::VideoFrame> frame);
  void OnResetComplete();

  // WeakPtr is bound to main_message_loop_. Use only in shim callbacks.
  base::WeakPtr<VideoDecoderShim> shim_;
  media::NullMediaLog media_log_;
  std::unique_ptr<media::VideoDecoder> decoder_;
  bool initialized_ = false;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  // Queue of decodes waiting for the decoder.
  using PendingDecodeQueue = base::queue<PendingDecode>;
  PendingDecodeQueue pending_decodes_;
  bool awaiting_decoder_ = false;
  // VideoDecoder returns pictures without information about the decode buffer
  // that generated it, but VideoDecoder implementations used in this class
  // (media::FFmpegVideoDecoder and media::VpxVideoDecoder) always generate
  // corresponding frames before decode is finished. |decode_id_| is used to
  // store id of the current buffer while Decode() call is pending.
  uint32_t decode_id_ = 0;

  base::WeakPtrFactory<DecoderImpl> weak_ptr_factory_{this};
};

VideoDecoderShim::DecoderImpl::DecoderImpl(
    const base::WeakPtr<VideoDecoderShim>& proxy)
    : shim_(proxy), main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

VideoDecoderShim::DecoderImpl::~DecoderImpl() {
  DCHECK(pending_decodes_.empty());
}

void VideoDecoderShim::DecoderImpl::Initialize(
    media::VideoDecoderConfig config) {
  DCHECK(!decoder_);
#if BUILDFLAG(ENABLE_LIBVPX) || BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#if BUILDFLAG(ENABLE_LIBVPX)
  if (config.codec() == media::kCodecVP9) {
    decoder_.reset(new media::VpxVideoDecoder());
  } else
#endif  // BUILDFLAG(ENABLE_LIBVPX)
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  {
    std::unique_ptr<media::FFmpegVideoDecoder> ffmpeg_video_decoder(
        new media::FFmpegVideoDecoder(&media_log_));
    ffmpeg_video_decoder->set_decode_nalus(true);
    decoder_ = std::move(ffmpeg_video_decoder);
  }
#endif  //  BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  // VpxVideoDecoder and FFmpegVideoDecoder support only one pending Decode()
  // request.
  DCHECK_EQ(decoder_->GetMaxDecodeRequests(), 1);

  decoder_->Initialize(
      config, true /* low_delay */, nullptr,
      base::BindOnce(&VideoDecoderShim::DecoderImpl::OnInitDone,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&VideoDecoderShim::DecoderImpl::OnOutputComplete,
                          weak_ptr_factory_.GetWeakPtr()),
      base::NullCallback());
#else
  OnInitDone(false);
#endif  // BUILDFLAG(ENABLE_LIBVPX) || BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
}

void VideoDecoderShim::DecoderImpl::Decode(
    uint32_t decode_id,
    scoped_refptr<media::DecoderBuffer> buffer) {
  DCHECK(decoder_);
  pending_decodes_.push(PendingDecode(decode_id, buffer));
  DoDecode();
}

void VideoDecoderShim::DecoderImpl::Reset() {
  DCHECK(decoder_);
  // Abort all pending decodes.
  while (!pending_decodes_.empty()) {
    const PendingDecode& decode = pending_decodes_.front();
    std::unique_ptr<PendingFrame> pending_frame(
        new PendingFrame(decode.decode_id));
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoDecoderShim::OnDecodeComplete, shim_,
                                  PP_OK, decode.decode_id));
    pending_decodes_.pop();
  }
  // Don't need to call Reset() if the |decoder_| hasn't been initialized.
  if (!initialized_) {
    OnResetComplete();
    return;
  }

  decoder_->Reset(
      base::BindOnce(&VideoDecoderShim::DecoderImpl::OnResetComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VideoDecoderShim::DecoderImpl::Stop() {
  DCHECK(decoder_);
  // Clear pending decodes now. We don't want OnDecodeComplete to call DoDecode
  // again.
  while (!pending_decodes_.empty())
    pending_decodes_.pop();
  decoder_.reset();
  // This instance is deleted once we exit this scope.
}

void VideoDecoderShim::DecoderImpl::OnInitDone(bool success) {
  if (!success) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoDecoderShim::OnInitializeFailed, shim_));
    return;
  }

  initialized_ = true;
  DoDecode();
}

void VideoDecoderShim::DecoderImpl::DoDecode() {
  if (!initialized_ || pending_decodes_.empty() || awaiting_decoder_)
    return;

  awaiting_decoder_ = true;
  const PendingDecode& decode = pending_decodes_.front();
  decode_id_ = decode.decode_id;
  decoder_->Decode(
      decode.buffer,
      base::BindOnce(&VideoDecoderShim::DecoderImpl::OnDecodeComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  pending_decodes_.pop();
}

void VideoDecoderShim::DecoderImpl::OnDecodeComplete(
    media::DecodeStatus status) {
  DCHECK(awaiting_decoder_);
  awaiting_decoder_ = false;

  int32_t result;
  switch (status) {
    case media::DecodeStatus::OK:
    case media::DecodeStatus::ABORTED:
      result = PP_OK;
      break;
    case media::DecodeStatus::DECODE_ERROR:
      result = PP_ERROR_RESOURCE_FAILED;
      break;
  }

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::OnDecodeComplete, shim_,
                                result, decode_id_));

  DoDecode();
}

void VideoDecoderShim::DecoderImpl::OnOutputComplete(
    scoped_refptr<media::VideoFrame> frame) {
  // Software decoders are expected to generated frames only when a Decode()
  // call is pending.
  DCHECK(awaiting_decoder_);

  std::unique_ptr<PendingFrame> pending_frame;
  if (!frame->metadata()->IsTrue(media::VideoFrameMetadata::END_OF_STREAM))
    pending_frame.reset(new PendingFrame(decode_id_, std::move(frame)));
  else
    pending_frame.reset(new PendingFrame(decode_id_));

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::OnOutputComplete, shim_,
                                std::move(pending_frame)));
}

void VideoDecoderShim::DecoderImpl::OnResetComplete() {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::OnResetComplete, shim_));
}

VideoDecoderShim::VideoDecoderShim(PepperVideoDecoderHost* host,
                                   uint32_t texture_pool_size)
    : state_(UNINITIALIZED),
      host_(host),
      media_task_runner_(
          RenderThreadImpl::current()->GetMediaThreadTaskRunner()),
      context_provider_(
          RenderThreadImpl::current()->SharedMainThreadContextProvider()),
      texture_pool_size_(texture_pool_size),
      num_pending_decodes_(0),
      yuv_converter_(new YUVConverter(context_provider_)) {
  DCHECK(host_);
  DCHECK(media_task_runner_.get());
  DCHECK(context_provider_.get());
  decoder_impl_.reset(new DecoderImpl(weak_ptr_factory_.GetWeakPtr()));
}

VideoDecoderShim::~VideoDecoderShim() {
  DCHECK(RenderThreadImpl::current());
  // Delete any remaining textures.
  auto it = texture_id_map_.begin();
  for (; it != texture_id_map_.end(); ++it)
    DeleteTexture(it->second);
  texture_id_map_.clear();

  FlushCommandBuffer();

  weak_ptr_factory_.InvalidateWeakPtrs();
  // No more callbacks from the delegate will be received now.

  // The callback now holds the only reference to the DecoderImpl, which will be
  // deleted when Stop completes.
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::DecoderImpl::Stop,
                                base::Owned(decoder_impl_.release())));
}

bool VideoDecoderShim::Initialize(const Config& vda_config, Client* client) {
  DCHECK_EQ(client, host_);
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, UNINITIALIZED);

  if (vda_config.is_encrypted()) {
    NOTREACHED() << "Encrypted streams are not supported";
    return false;
  }

  media::VideoCodec codec = media::kUnknownVideoCodec;
  if (vda_config.profile <= media::H264PROFILE_MAX)
    codec = media::kCodecH264;
  else if (vda_config.profile <= media::VP8PROFILE_MAX)
    codec = media::kCodecVP8;
  else if (vda_config.profile <= media::VP9PROFILE_MAX)
    codec = media::kCodecVP9;
  DCHECK_NE(codec, media::kUnknownVideoCodec);

  if (!IsCodecSupported(codec))
    return false;

  if (!yuv_converter_->Initialize())
    return false;

  media::VideoDecoderConfig video_decoder_config(
      codec, vda_config.profile,
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::kNoTransformation,
      gfx::Size(32, 24),  // Small sizes that won't fail.
      gfx::Rect(32, 24), gfx::Size(32, 24),
      // TODO(bbudge): Verify extra data isn't needed.
      media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted);

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::DecoderImpl::Initialize,
                                base::Unretained(decoder_impl_.get()),
                                video_decoder_config));

  state_ = DECODING;

  // Return success, even though we are asynchronous, to mimic
  // media::VideoDecodeAccelerator.
  return true;
}

void VideoDecoderShim::Decode(media::BitstreamBuffer bitstream_buffer) {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, DECODING);

  // We need the address of the shared memory, so we can copy the buffer.
  const uint8_t* buffer = host_->DecodeIdToAddress(bitstream_buffer.id());
  DCHECK(buffer);

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoDecoderShim::DecoderImpl::Decode,
          base::Unretained(decoder_impl_.get()), bitstream_buffer.id(),
          media::DecoderBuffer::CopyFrom(buffer, bitstream_buffer.size())));
  num_pending_decodes_++;
}

void VideoDecoderShim::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK(RenderThreadImpl::current());
  DCHECK_NE(state_, UNINITIALIZED);
  if (buffers.empty()) {
    NOTREACHED();
    return;
  }
  std::vector<gpu::Mailbox> mailboxes = host_->TakeMailboxes();
  DCHECK_EQ(buffers.size(), mailboxes.size());
  GLuint num_textures = base::checked_cast<GLuint>(buffers.size());
  std::vector<uint32_t> local_texture_ids(num_textures);
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  for (uint32_t i = 0; i < num_textures; i++) {
    DCHECK_EQ(1u, buffers[i].client_texture_ids().size());
    local_texture_ids[i] =
        gles2->CreateAndConsumeTextureCHROMIUM(mailboxes[i].name);
    // Map the plugin texture id to the local texture id.
    uint32_t plugin_texture_id = buffers[i].client_texture_ids()[0];
    texture_id_map_[plugin_texture_id] = local_texture_ids[i];
    available_textures_.insert(plugin_texture_id);
  }
  SendPictures();
}

void VideoDecoderShim::ReusePictureBuffer(int32_t picture_buffer_id) {
  DCHECK(RenderThreadImpl::current());
  uint32_t texture_id = static_cast<uint32_t>(picture_buffer_id);
  if (textures_to_dismiss_.find(texture_id) != textures_to_dismiss_.end()) {
    DismissTexture(texture_id);
  } else if (texture_id_map_.find(texture_id) != texture_id_map_.end()) {
    available_textures_.insert(texture_id);
    SendPictures();
  } else {
    NOTREACHED();
  }
}

void VideoDecoderShim::Flush() {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, DECODING);
  state_ = FLUSHING;
}

void VideoDecoderShim::Reset() {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(state_, DECODING);
  state_ = RESETTING;
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderShim::DecoderImpl::Reset,
                                base::Unretained(decoder_impl_.get())));
}

void VideoDecoderShim::Destroy() {
  delete this;
}

void VideoDecoderShim::OnInitializeFailed() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  host_->NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
}

void VideoDecoderShim::OnDecodeComplete(int32_t result, uint32_t decode_id) {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  if (result == PP_ERROR_RESOURCE_FAILED) {
    host_->NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }

  num_pending_decodes_--;
  completed_decodes_.push(decode_id);

  // If frames are being queued because we're out of textures, don't notify
  // the host that decode has completed. This exerts "back pressure" to keep
  // the host from sending buffers that will cause pending_frames_ to grow.
  if (pending_frames_.empty())
    NotifyCompletedDecodes();
}

void VideoDecoderShim::OnOutputComplete(std::unique_ptr<PendingFrame> frame) {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  if (frame->video_frame) {
    if (texture_size_ != frame->video_frame->coded_size()) {
      // If the size has changed, all current textures must be dismissed. Add
      // all textures to |textures_to_dismiss_| and dismiss any that aren't in
      // use by the plugin. We will dismiss the rest as they are recycled.
      for (TextureIdMap::const_iterator it = texture_id_map_.begin();
           it != texture_id_map_.end();
           ++it) {
        textures_to_dismiss_.insert(it->first);
      }
      for (auto it = available_textures_.begin();
           it != available_textures_.end(); ++it) {
        DismissTexture(*it);
      }
      available_textures_.clear();
      FlushCommandBuffer();

      host_->ProvidePictureBuffers(texture_pool_size_, media::PIXEL_FORMAT_ARGB,
                                   1, frame->video_frame->coded_size(),
                                   GL_TEXTURE_2D);
      texture_size_ = frame->video_frame->coded_size();
    }

    pending_frames_.push(std::move(frame));
    SendPictures();
  }
}

void VideoDecoderShim::SendPictures() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);
  while (!pending_frames_.empty() && !available_textures_.empty()) {
    const std::unique_ptr<PendingFrame>& frame = pending_frames_.front();

    auto it = available_textures_.begin();
    uint32_t texture_id = *it;
    available_textures_.erase(it);

    uint32_t local_texture_id = texture_id_map_[texture_id];

    yuv_converter_->Convert(frame->video_frame.get(), local_texture_id);

    host_->PictureReady(media::Picture(texture_id, frame->decode_id,
                                       frame->video_frame->visible_rect(),
                                       gfx::ColorSpace(), false));
    pending_frames_.pop();
  }

  FlushCommandBuffer();

  if (pending_frames_.empty()) {
    // If frames aren't backing up, notify the host of any completed decodes so
    // it can send more buffers.
    NotifyCompletedDecodes();

    if (state_ == FLUSHING && !num_pending_decodes_) {
      state_ = DECODING;
      host_->NotifyFlushDone();
    }
  }
}

void VideoDecoderShim::OnResetComplete() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(host_);

  while (!pending_frames_.empty())
    pending_frames_.pop();
  NotifyCompletedDecodes();

  // Dismiss any old textures now.
  while (!textures_to_dismiss_.empty())
    DismissTexture(*textures_to_dismiss_.begin());

  state_ = DECODING;
  host_->NotifyResetDone();
}

void VideoDecoderShim::NotifyCompletedDecodes() {
  while (!completed_decodes_.empty()) {
    host_->NotifyEndOfBitstreamBuffer(completed_decodes_.front());
    completed_decodes_.pop();
  }
}

void VideoDecoderShim::DismissTexture(uint32_t texture_id) {
  DCHECK(host_);
  textures_to_dismiss_.erase(texture_id);
  DCHECK(texture_id_map_.find(texture_id) != texture_id_map_.end());
  DeleteTexture(texture_id_map_[texture_id]);
  texture_id_map_.erase(texture_id);
  host_->DismissPictureBuffer(texture_id);
}

void VideoDecoderShim::DeleteTexture(uint32_t texture_id) {
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  gles2->DeleteTextures(1, &texture_id);
}

void VideoDecoderShim::FlushCommandBuffer() {
  context_provider_->ContextGL()->Flush();
}

}  // namespace content
