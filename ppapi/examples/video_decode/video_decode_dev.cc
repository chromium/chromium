// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <string.h>

#include <iostream>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/dev/video_decoder_client_dev.h"
#include "ppapi/cpp/dev/video_decoder_dev.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/examples/video_decode/testdata.h"
#include "ppapi/lib/gl/include/GLES2/gl2.h"
#include "ppapi/lib/gl/include/GLES2/gl2ext.h"
#include "ppapi/utility/completion_callback_factory.h"

// Use assert as a makeshift CHECK, even in non-debug mode.
// Since <assert.h> redefines assert on every inclusion (it doesn't use
// include-guards), make sure this is the last file #include'd in this file.
#undef NDEBUG
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

// Assert |context_| isn't holding any GL Errors.  Done as a macro instead of a
// function to preserve line number information in the failure message.
#define assertNoGLError() \
  assert(!gles2_if_->GetError(context_->pp_resource()));

namespace {

struct PictureBufferInfo {
  PP_PictureBuffer_Dev buffer;
  GLenum texture_target;
};

struct Shader {
  Shader() : program(0),
             texcoord_scale_location(0) {}

  GLuint program;
  GLint texcoord_scale_location;
};

class VideoDecodeDemoInstance : public pp::Instance,
                                public pp::Graphics3DClient,
                                public pp::VideoDecoderClient_Dev {
 public:
  VideoDecodeDemoInstance(PP_Instance instance, pp::Module* module);
  virtual ~VideoDecodeDemoInstance();

  // pp::Instance implementation (see PPP_Instance).
  virtual void DidChangeView(const pp::Rect& position,
                             const pp::Rect& clip_ignored);

  // pp::Graphics3DClient implementation.
  virtual void Graphics3DContextLost() {
    // TODO(vrk/fischman): Properly reset after a lost graphics context.  In
    // particular need to delete context_ and re-create textures.
    // Probably have to recreate the decoder from scratch, because old textures
    // can still be outstanding in the decoder!
    assert(false && "Unexpectedly lost graphics context");
  }

  // pp::VideoDecoderClient_Dev implementation.
  virtual void ProvidePictureBuffers(
      PP_Resource decoder,
      uint32_t req_num_of_bufs,
      const PP_Size& dimensions,
      uint32_t texture_target);
  virtual void DismissPictureBuffer(PP_Resource decoder,
                                    int32_t picture_buffer_id);
  virtual void PictureReady(PP_Resource decoder, const PP_Picture_Dev& picture);
  virtual void NotifyError(PP_Resource decoder, PP_VideoDecodeError_Dev error);

 private:
  enum { kNumConcurrentDecodes = 7,
         kNumDecoders = 2 };  // Baked into viewport rendering.

  // A single decoder's client interface.
  class DecoderClient {
   public:
    DecoderClient(VideoDecodeDemoInstance* gles2,
                  pp::VideoDecoder_Dev* decoder);
    ~DecoderClient();

    void DecodeNextNALUs();

    // Per-decoder implementation of part of pp::VideoDecoderClient_Dev.
    void ProvidePictureBuffers(
        uint32_t req_num_of_bufs,
        PP_Size dimensions,
        uint32_t texture_target);
    void DismissPictureBuffer(int32_t picture_buffer_id);

    const PictureBufferInfo& GetPictureBufferInfoById(int id);
    pp::VideoDecoder_Dev* decoder() { return decoder_; }

   private:
    void DecodeNextNALU();
    static void GetNextNALUBoundary(size_t start_pos, size_t* end_pos);
    void DecoderBitstreamDone(int32_t result, int bitstream_buffer_id);
    void DecoderFlushDone(int32_t result);

    VideoDecodeDemoInstance* gles2_;
    pp::VideoDecoder_Dev* decoder_;
    pp::CompletionCallbackFactory<DecoderClient> callback_factory_;
    int next_picture_buffer_id_;
    int next_bitstream_buffer_id_;
    size_t encoded_data_next_pos_to_decode_;
    std::set<int> bitstream_ids_at_decoder_;
    // Map of texture buffers indexed by buffer id.
    typedef std::map<int, PictureBufferInfo> PictureBufferMap;
    PictureBufferMap picture_buffers_by_id_;
    // Map of bitstream buffers indexed by id.
    typedef std::map<int, pp::Buffer_Dev*> BitstreamBufferMap;
    BitstreamBufferMap bitstream_buffers_by_id_;
  };

  // Initialize Video Decoders.
  void InitializeDecoders();

  // GL-related functions.
  void InitGL();
  GLuint CreateTexture(int32_t width, int32_t height, GLenum texture_target);
  void CreateGLObjects();
  void Create2DProgramOnce();
  void CreateRectangleARBProgramOnce();
  Shader CreateProgram(const char* vertex_shader,
                       const char* fragment_shader);
  void CreateShader(GLuint program, GLenum type, const char* source, int size);
  void DeleteTexture(GLuint id);
  void PaintFinished(int32_t result, PP_Resource decoder,
                     int picture_buffer_id);

  // Log an error to the developer console and stderr (though the latter may be
  // closed due to sandboxing or blackholed for other reasons) by creating a
  // temporary of this type and streaming to it.  Example usage:
  // LogError(this).s() << "Hello world: " << 42;
  class LogError {
   public:
    LogError(VideoDecodeDemoInstance* demo) : demo_(demo) {}
    ~LogError() {
      const std::string& msg = stream_.str();
      demo_->console_if_->Log(demo_->pp_instance(), PP_LOGLEVEL_ERROR,
                              pp::Var(msg).pp_var());
      std::cerr << msg << std::endl;
    }
    // Impl note: it would have been nicer to have LogError derive from
    // std::ostringstream so that it can be streamed to directly, but lookup
    // rules turn streamed string literals to hex pointers on output.
    std::ostringstream& s() { return stream_; }
   private:
    VideoDecodeDemoInstance* demo_;  // Unowned.
    std::ostringstream stream_;
  };

  pp::Size plugin_size_;
  bool is_painting_;
  // When decode outpaces render, we queue up decoded pictures for later
  // painting.  Elements are <decoder,picture>.
  std::list<std::pair<PP_Resource, PP_Picture_Dev> > pictures_pending_paint_;
  int num_frames_rendered_;
  PP_TimeTicks first_frame_delivered_ticks_;
  PP_TimeTicks last_swap_request_ticks_;
  PP_TimeTicks swap_ticks_;
  pp::CompletionCallbackFactory<VideoDecodeDemoInstance> callback_factory_;

  // Unowned pointers.
  const PPB_Console* console_if_;
  const PPB_Core* core_if_;
  const PPB_OpenGLES2* gles2_if_;

  // Owned data.
  pp::Graphics3D* context_;
  typedef std::map<int, DecoderClient*> Decoders;
  Decoders video_decoders_;

  // Shader program to draw GL_TEXTURE_2D target.
  Shader shader_2d_;
  // Shader program to draw GL_TEXTURE_RECTANGLE_ARB target.
  Shader shader_rectangle_arb_;
};

VideoDecodeDemoInstance::DecoderClient::DecoderClient(
      VideoDecodeDemoInstance* gles2, pp::VideoDecoder_Dev* decoder)
    : gles2_(gles2), decoder_(decoder), callback_factory_(this),
      next_picture_buffer_id_(0),
      next_bitstream_buffer_id_(0), encoded_data_next_pos_to_decode_(0) {
}

VideoDecodeDemoInstance::DecoderClient::~DecoderClient() {
  delete decoder_;
  decoder_ = NULL;

  for (BitstreamBufferMap::iterator it = bitstream_buffers_by_id_.begin();
       it != bitstream_buffers_by_id_.end(); ++it) {
    delete it->second;
  }
  bitstream_buffers_by_id_.clear();

  for (PictureBufferMap::iterator it = picture_buffers_by_id_.begin();
       it != picture_buffers_by_id_.end(); ++it) {
    gles2_->DeleteTexture(it->second.buffer.texture_id);
  }
  picture_buffers_by_id_.clear();
}

VideoDecodeDemoInstance::VideoDecodeDemoInstance(PP_Instance instance,
                                                 pp::Module* module)
    : pp::Instance(instance), pp::Graphics3DClient(this),
      pp::VideoDecoderClient_Dev(this),
      is_painting_(false),
      num_frames_rendered_(0),
      first_frame_delivered_ticks_(-1),
      swap_ticks_(0),
      callback_factory_(this),
      console_if_(static_cast<const PPB_Console*>(
          module->GetBrowserInterface(PPB_CONSOLE_INTERFACE))),
      core_if_(static_cast<const PPB_Core*>(
          module->GetBrowserInterface(PPB_CORE_INTERFACE))),
      gles2_if_(static_cast<const PPB_OpenGLES2*>(
          module->GetBrowserInterface(PPB_OPENGLES2_INTERFACE))),
      context_(NULL) {
  assert(console_if_);
  assert(core_if_);
  assert(gles2_if_);
}

VideoDecodeDemoInstance::~VideoDecodeDemoInstance() {
  if (shader_2d_.program)
    gles2_if_->DeleteProgram(context_->pp_resource(), shader_2d_.program);
  if (shader_rectangle_arb_.program) {
    gles2_if_->DeleteProgram(
        context_->pp_resource(), shader_rectangle_arb_.program);
  }

  for (Decoders::iterator it = video_decoders_.begin();
       it != video_decoders_.end(); ++it) {
    delete it->second;
  }
  video_decoders_.clear();
  delete context_;
}

void VideoDecodeDemoInstance::DidChangeView(
    const pp::Rect& position, const pp::Rect& clip_ignored) {
  if (position.width() == 0 || position.height() == 0)
    return;
  if (plugin_size_.width()) {
    assert(position.size() == plugin_size_);
    return;
  }
  plugin_size_ = position.size();

  // Initialize graphics.
  InitGL();
  InitializeDecoders();
}

void VideoDecodeDemoInstance::InitializeDecoders() {
  assert(video_decoders_.empty());
  for (int i = 0; i < kNumDecoders; ++i) {
    DecoderClient* client = new DecoderClient(
        this, new pp::VideoDecoder_Dev(
            this, *context_, PP_VIDEODECODER_H264PROFILE_MAIN));
    assert(!client->decoder()->is_null());
    assert(video_decoders_.insert(std::make_pair(
        client->decoder()->pp_resource(), client)).second);
    client->DecodeNextNALUs();
  }
}

void VideoDecodeDemoInstance::DecoderClient::DecoderBitstreamDone(
    int32_t result, int bitstream_buffer_id) {
  assert(bitstream_ids_at_decoder_.erase(bitstream_buffer_id) == 1);
  BitstreamBufferMap::iterator it =
      bitstream_buffers_by_id_.find(bitstream_buffer_id);
  assert(it != bitstream_buffers_by_id_.end());
  delete it->second;
  bitstream_buffers_by_id_.erase(it);
  DecodeNextNALUs();
}

void VideoDecodeDemoInstance::DecoderClient::DecoderFlushDone(int32_t result) {
  assert(result == PP_OK);
  // Check that each bitstream buffer ID we handed to the decoder got handed
  // back to us.
  assert(bitstream_ids_at_decoder_.empty());
  delete decoder_;
  decoder_ = NULL;
}

static bool LookingAtNAL(const unsigned char* encoded, size_t pos) {
  return pos + 3 < kDataLen &&
      encoded[pos] == 0 && encoded[pos + 1] == 0 &&
      encoded[pos + 2] == 0 && encoded[pos + 3] == 1;
}

void VideoDecodeDemoInstance::DecoderClient::GetNextNALUBoundary(
    size_t start_pos, size_t* end_pos) {
  assert(LookingAtNAL(kData, start_pos));
  *end_pos = start_pos;
  *end_pos += 4;
  while (*end_pos + 3 < kDataLen &&
         !LookingAtNAL(kData, *end_pos)) {
    ++*end_pos;
  }
  if (*end_pos + 3 >= kDataLen) {
    *end_pos = kDataLen;
    return;
  }
}

void VideoDecodeDemoInstance::DecoderClient::DecodeNextNALUs() {
  while (encoded_data_next_pos_to_decode_ <= kDataLen &&
         bitstream_ids_at_decoder_.size() < kNumConcurrentDecodes) {
    DecodeNextNALU();
  }
}

void VideoDecodeDemoInstance::DecoderClient::DecodeNextNALU() {
  if (encoded_data_next_pos_to_decode_ == kDataLen) {
    ++encoded_data_next_pos_to_decode_;
    pp::CompletionCallback cb = callback_factory_.NewCallback(
        &VideoDecodeDemoInstance::DecoderClient::DecoderFlushDone);
    decoder_->Flush(cb);
    return;
  }
  size_t start_pos = encoded_data_next_pos_to_decode_;
  size_t end_pos;
  GetNextNALUBoundary(start_pos, &end_pos);
  pp::Buffer_Dev* buffer = new pp::Buffer_Dev(
      gles2_, static_cast<uint32_t>(end_pos - start_pos));
  PP_VideoBitstreamBuffer_Dev bitstream_buffer;
  int id = ++next_bitstream_buffer_id_;
  bitstream_buffer.id = id;
  bitstream_buffer.size = static_cast<uint32_t>(end_pos - start_pos);
  bitstream_buffer.data = buffer->pp_resource();
  memcpy(buffer->data(), kData + start_pos, end_pos - start_pos);
  assert(bitstream_buffers_by_id_.insert(std::make_pair(id, buffer)).second);

  pp::CompletionCallback cb =
      callback_factory_.NewCallback(
          &VideoDecodeDemoInstance::DecoderClient::DecoderBitstreamDone, id);
  assert(bitstream_ids_at_decoder_.insert(id).second);
  encoded_data_next_pos_to_decode_ = end_pos;
  decoder_->Decode(bitstream_buffer, cb);
}

void VideoDecodeDemoInstance::ProvidePictureBuffers(PP_Resource decoder,
                                                    uint32_t req_num_of_bufs,
                                                    const PP_Size& dimensions,
                                                    uint32_t texture_target) {
  DecoderClient* client = video_decoders_[decoder];
  assert(client);
  client->ProvidePictureBuffers(req_num_of_bufs, dimensions, texture_target);
}

void VideoDecodeDemoInstance::DecoderClient::ProvidePictureBuffers(
    uint32_t req_num_of_bufs,
    PP_Size dimensions,
    uint32_t texture_target) {
  std::vector<PP_PictureBuffer_Dev> buffers;
  for (uint32_t i = 0; i < req_num_of_bufs; ++i) {
    PictureBufferInfo info;
    info.buffer.size = dimensions;
    info.texture_target = texture_target;
    info.buffer.texture_id = gles2_->CreateTexture(
        dimensions.width, dimensions.height, info.texture_target);
    int id = ++next_picture_buffer_id_;
    info.buffer.id = id;
    buffers.push_back(info.buffer);
    assert(picture_buffers_by_id_.insert(std::make_pair(id, info)).second);
  }
  decoder_->AssignPictureBuffers(buffers);
}

const PictureBufferInfo&
VideoDecodeDemoInstance::DecoderClient::GetPictureBufferInfoById(
    int id) {
  PictureBufferMap::iterator it = picture_buffers_by_id_.find(id);
  assert(it != picture_buffers_by_id_.end());
  return it->second;
}

void VideoDecodeDemoInstance::DismissPictureBuffer(PP_Resource decoder,
                                             int32_t picture_buffer_id) {
  DecoderClient* client = video_decoders_[decoder];
  assert(client);
  client->DismissPictureBuffer(picture_buffer_id);
}

void VideoDecodeDemoInstance::DecoderClient::DismissPictureBuffer(
    int32_t picture_buffer_id) {
  gles2_->DeleteTexture(GetPictureBufferInfoById(
      picture_buffer_id).buffer.texture_id);
  picture_buffers_by_id_.erase(picture_buffer_id);
}

void VideoDecodeDemoInstance::PictureReady(PP_Resource decoder,
                                     const PP_Picture_Dev& picture) {
  if (first_frame_delivered_ticks_ == -1)
    assert((first_frame_delivered_ticks_ = core_if_->GetTimeTicks()) != -1);
  if (is_painting_) {
    pictures_pending_paint_.push_back(std::make_pair(decoder, picture));
    return;
  }
  DecoderClient* client = video_decoders_[decoder];
  assert(client);
  const PictureBufferInfo& info =
      client->GetPictureBufferInfoById(picture.picture_buffer_id);
  assert(!is_painting_);
  is_painting_ = true;
  int x = 0;
  int y = 0;
  if (client != video_decoders_.begin()->second) {
    x = plugin_size_.width() / kNumDecoders;
    y = plugin_size_.height() / kNumDecoders;
  }

  if (info.texture_target == GL_TEXTURE_2D) {
    Create2DProgramOnce();
    gles2_if_->UseProgram(context_->pp_resource(), shader_2d_.program);
    gles2_if_->Uniform2f(
        context_->pp_resource(), shader_2d_.texcoord_scale_location, 1.0, 1.0);
  } else {
    assert(info.texture_target == GL_TEXTURE_RECTANGLE_ARB);
    CreateRectangleARBProgramOnce();
    gles2_if_->UseProgram(
        context_->pp_resource(), shader_rectangle_arb_.program);
    gles2_if_->Uniform2f(context_->pp_resource(),
                         shader_rectangle_arb_.texcoord_scale_location,
                         static_cast<GLfloat>(info.buffer.size.width),
                         static_cast<GLfloat>(info.buffer.size.height));
  }

  gles2_if_->Viewport(context_->pp_resource(), x, y,
                      plugin_size_.width() / kNumDecoders,
                      plugin_size_.height() / kNumDecoders);
  gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE0);
  gles2_if_->BindTexture(
      context_->pp_resource(), info.texture_target, info.buffer.texture_id);
  gles2_if_->DrawArrays(context_->pp_resource(), GL_TRIANGLE_STRIP, 0, 4);

  gles2_if_->UseProgram(context_->pp_resource(), 0);

  pp::CompletionCallback cb =
      callback_factory_.NewCallback(
          &VideoDecodeDemoInstance::PaintFinished, decoder, info.buffer.id);
  last_swap_request_ticks_ = core_if_->GetTimeTicks();
  assert(context_->SwapBuffers(cb) == PP_OK_COMPLETIONPENDING);
}

void VideoDecodeDemoInstance::NotifyError(PP_Resource decoder,
                                          PP_VideoDecodeError_Dev error) {
  LogError(this).s() << "Received error: " << error;
  assert(false && "Unexpected error; see stderr for details");
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class VideoDecodeDemoModule : public pp::Module {
 public:
  VideoDecodeDemoModule() : pp::Module() {}
  virtual ~VideoDecodeDemoModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new VideoDecodeDemoInstance(instance, this);
  }
};

void VideoDecodeDemoInstance::InitGL() {
  assert(plugin_size_.width() && plugin_size_.height());
  is_painting_ = false;

  assert(!context_);
  int32_t context_attributes[] = {
    PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
    PP_GRAPHICS3DATTRIB_BLUE_SIZE, 8,
    PP_GRAPHICS3DATTRIB_GREEN_SIZE, 8,
    PP_GRAPHICS3DATTRIB_RED_SIZE, 8,
    PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 0,
    PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 0,
    PP_GRAPHICS3DATTRIB_SAMPLES, 0,
    PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
    PP_GRAPHICS3DATTRIB_WIDTH, plugin_size_.width(),
    PP_GRAPHICS3DATTRIB_HEIGHT, plugin_size_.height(),
    PP_GRAPHICS3DATTRIB_NONE,
  };
  context_ = new pp::Graphics3D(this, context_attributes);
  assert(!context_->is_null());
  assert(BindGraphics(*context_));

  // Clear color bit.
  gles2_if_->ClearColor(context_->pp_resource(), 1, 0, 0, 1);
  gles2_if_->Clear(context_->pp_resource(), GL_COLOR_BUFFER_BIT);

  assertNoGLError();

  CreateGLObjects();
}

void VideoDecodeDemoInstance::PaintFinished(int32_t result, PP_Resource decoder,
                                      int picture_buffer_id) {
  assert(result == PP_OK);
  swap_ticks_ += core_if_->GetTimeTicks() - last_swap_request_ticks_;
  is_painting_ = false;
  ++num_frames_rendered_;
  if (num_frames_rendered_ % 50 == 0) {
    double elapsed = core_if_->GetTimeTicks() - first_frame_delivered_ticks_;
    double fps = (elapsed > 0) ? num_frames_rendered_ / elapsed : 1000;
    double ms_per_swap = (swap_ticks_ * 1e3) / num_frames_rendered_;
    LogError(this).s() << "Rendered frames: " << num_frames_rendered_
                       << ", fps: " << fps << ", with average ms/swap of: "
                       << ms_per_swap;
  }
  DecoderClient* client = video_decoders_[decoder];
  if (client && client->decoder())
    client->decoder()->ReusePictureBuffer(picture_buffer_id);
  if (!pictures_pending_paint_.empty()) {
    std::pair<PP_Resource, PP_Picture_Dev> decoder_picture =
        pictures_pending_paint_.front();
    pictures_pending_paint_.pop_front();
    PictureReady(decoder_picture.first, decoder_picture.second);
  }
}

GLuint VideoDecodeDemoInstance::CreateTexture(int32_t width,
                                              int32_t height,
                                              GLenum texture_target) {
  GLuint texture_id;
  gles2_if_->GenTextures(context_->pp_resource(), 1, &texture_id);
  assertNoGLError();
  // Assign parameters.
  gles2_if_->ActiveTexture(context_->pp_resource(), GL_TEXTURE0);
  gles2_if_->BindTexture(context_->pp_resource(), texture_target, texture_id);
  gles2_if_->TexParameteri(
      context_->pp_resource(), texture_target, GL_TEXTURE_MIN_FILTER,
      GL_NEAREST);
  gles2_if_->TexParameteri(
      context_->pp_resource(), texture_target, GL_TEXTURE_MAG_FILTER,
      GL_NEAREST);
  gles2_if_->TexParameterf(
      context_->pp_resource(), texture_target, GL_TEXTURE_WRAP_S,
      GL_CLAMP_TO_EDGE);
  gles2_if_->TexParameterf(
      context_->pp_resource(), texture_target, GL_TEXTURE_WRAP_T,
      GL_CLAMP_TO_EDGE);

  if (texture_target == GL_TEXTURE_2D) {
    gles2_if_->TexImage2D(
        context_->pp_resource(), texture_target, 0, GL_RGBA, width, height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  }
  assertNoGLError();
  return texture_id;
}

void VideoDecodeDemoInstance::DeleteTexture(GLuint id) {
  gles2_if_->DeleteTextures(context_->pp_resource(), 1, &id);
}

void VideoDecodeDemoInstance::CreateGLObjects() {
  // Assign vertex positions and texture coordinates to buffers for use in
  // shader program.
  static const float kVertices[] = {
    -1, 1, -1, -1, 1, 1, 1, -1,  // Position coordinates.
    0, 1, 0, 0, 1, 1, 1, 0,      // Texture coordinates.
  };

  GLuint buffer;
  gles2_if_->GenBuffers(context_->pp_resource(), 1, &buffer);
  gles2_if_->BindBuffer(context_->pp_resource(), GL_ARRAY_BUFFER, buffer);

  gles2_if_->BufferData(context_->pp_resource(), GL_ARRAY_BUFFER,
                        sizeof(kVertices), kVertices, GL_STATIC_DRAW);
  assertNoGLError();
}

static const char kVertexShader[] =
    "varying vec2 v_texCoord;            \n"
    "attribute vec4 a_position;          \n"
    "attribute vec2 a_texCoord;          \n"
    "uniform vec2 v_scale;               \n"
    "void main()                         \n"
    "{                                   \n"
    "    v_texCoord = v_scale * a_texCoord; \n"
    "    gl_Position = a_position;       \n"
    "}";

void VideoDecodeDemoInstance::Create2DProgramOnce() {
  if (shader_2d_.program)
    return;
  static const char kFragmentShader2D[] =
      "precision mediump float;            \n"
      "varying vec2 v_texCoord;            \n"
      "uniform sampler2D s_texture;        \n"
      "void main()                         \n"
      "{"
      "    gl_FragColor = texture2D(s_texture, v_texCoord); \n"
      "}";
  shader_2d_ = CreateProgram(kVertexShader, kFragmentShader2D);
  assertNoGLError();
}

void VideoDecodeDemoInstance::CreateRectangleARBProgramOnce() {
  if (shader_rectangle_arb_.program)
    return;
  static const char kFragmentShaderRectangle[] =
      "#extension GL_ARB_texture_rectangle : require\n"
      "precision mediump float;            \n"
      "varying vec2 v_texCoord;            \n"
      "uniform sampler2DRect s_texture;    \n"
      "void main()                         \n"
      "{"
      "    gl_FragColor = texture2DRect(s_texture, v_texCoord).rgba; \n"
      "}";
  shader_rectangle_arb_ =
      CreateProgram(kVertexShader, kFragmentShaderRectangle);
}

Shader VideoDecodeDemoInstance::CreateProgram(const char* vertex_shader,
                                              const char* fragment_shader) {
  Shader shader;

  // Create shader program.
  shader.program = gles2_if_->CreateProgram(context_->pp_resource());
  CreateShader(shader.program, GL_VERTEX_SHADER, vertex_shader,
               static_cast<int>(strlen(vertex_shader)));
  CreateShader(shader.program, GL_FRAGMENT_SHADER, fragment_shader,
               static_cast<int>(strlen(fragment_shader)));
  gles2_if_->LinkProgram(context_->pp_resource(), shader.program);
  gles2_if_->UseProgram(context_->pp_resource(), shader.program);
  gles2_if_->Uniform1i(
      context_->pp_resource(),
      gles2_if_->GetUniformLocation(
          context_->pp_resource(), shader.program, "s_texture"), 0);
  assertNoGLError();

  shader.texcoord_scale_location = gles2_if_->GetUniformLocation(
      context_->pp_resource(), shader.program, "v_scale");

  GLint pos_location = gles2_if_->GetAttribLocation(
      context_->pp_resource(), shader.program, "a_position");
  GLint tc_location = gles2_if_->GetAttribLocation(
      context_->pp_resource(), shader.program, "a_texCoord");
  assertNoGLError();

  gles2_if_->EnableVertexAttribArray(context_->pp_resource(), pos_location);
  gles2_if_->VertexAttribPointer(context_->pp_resource(), pos_location, 2,
                                 GL_FLOAT, GL_FALSE, 0, 0);
  gles2_if_->EnableVertexAttribArray(context_->pp_resource(), tc_location);
  gles2_if_->VertexAttribPointer(
      context_->pp_resource(), tc_location, 2, GL_FLOAT, GL_FALSE, 0,
      reinterpret_cast<void*>(8 *
                              sizeof(GLfloat)));  // Skip position coordinates.

  gles2_if_->UseProgram(context_->pp_resource(), 0);
  assertNoGLError();
  return shader;
}

void VideoDecodeDemoInstance::CreateShader(
    GLuint program, GLenum type, const char* source, int size) {
  GLuint shader = gles2_if_->CreateShader(context_->pp_resource(), type);
  gles2_if_->ShaderSource(context_->pp_resource(), shader, 1, &source, &size);
  gles2_if_->CompileShader(context_->pp_resource(), shader);
  gles2_if_->AttachShader(context_->pp_resource(), program, shader);
  gles2_if_->DeleteShader(context_->pp_resource(), shader);
}
}  // anonymous namespace

namespace pp {
// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new VideoDecodeDemoModule();
}
}  // namespace pp
