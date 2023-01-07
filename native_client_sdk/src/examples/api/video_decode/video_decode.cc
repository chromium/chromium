// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <queue>
#include <sstream>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/graphics_3d_client.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/video_decoder.h"
#include "ppapi/utility/completion_callback_factory.h"

// VP8 is more likely to work on different versions of Chrome. Undefine this
// to decode H264.
#define USE_VP8_TESTDATA_INSTEAD_OF_H264
#include "testdata.h"

// Use assert as a makeshift CHECK, even in non-debug mode.
// Since <assert.h> redefines assert on every inclusion (it doesn't use
// include-guards), make sure this is the last file #include'd in this file.
#undef NDEBUG
#include <assert.h>

// Assert |context_| isn't holding any GL Errors.  Done as a macro instead of a
// function to preserve line number information in the failure message.
#define assertNoGLError() assert(!gles2_if_->GetError(context_->pp_resource()));

namespace {

struct Shader {
  Shader() : program(0), texcoord_scale_location(0) {}
  ~Shader() {}

  GLuint program;
  GLint texcoord_scale_location;
};

class Decoder;
class MyInstance;

struct PendingPicture {
  PendingPicture(Decoder* decoder, const PP_VideoPicture& picture)
      : decoder(decoder), picture(picture) {}
  ~PendingPicture() {}

  Decoder* decoder;
  PP_VideoPicture picture;
};

class MyInstance : public pp::Instance, public pp::Graphics3DClient {
 public:
  MyInstance(PP_Instance instance, pp::Module* module);
  virtual ~MyInstance();

  // pp::Instance implementation.
  virtual void DidChangeView(const pp::Rect& position,
                             const pp::Rect& clip_ignored);
  virtual bool HandleInputEvent(const pp::InputEvent& event);

  // pp::Graphics3DClient implementation.
  virtual void Graphics3DContextLost() {
    // TODO(vrk/fischman): Properly reset after a lost graphics context.  In
    // particular need to delete context_ and re-create textures.
    // Probably have to recreate the decoder from scratch, because old textures
    // can still be outstanding in the decoder!
    assert(false && "Unexpectedly lost graphics context");
  }

  void PaintPicture(Decoder* decoder, const PP_VideoPicture& picture);

 private:
  // Log an error to the developer console and stderr by creating a temporary
  // object of this type and streaming to it.  Example usage:
  // LogError(this).s() << "Hello world: " << 42;
  class LogError {
   public:
    LogError(MyInstance* instance) : instance_(instance) {}
    ~LogError() {
      const std::string& msg = stream_.str();
      instance_->console_if_->Log(
          instance_->pp_instance(), PP_LOGLEVEL_ERROR, pp::Var(msg).pp_var());
      std::cerr << msg << std::endl;
    }
    // Impl note: it would have been nicer to have LogError derive from
    // std::ostringstream so that it can be streamed to directly, but lookup
    // rules turn streamed string literals to hex pointers on output.
    std::ostringstream& s() { return stream_; }

   private:
    MyInstance* instance_;
    std::ostringstream stream_;
  };

  void InitializeDecoders();

  // GL-related functions.
  void InitGL();
  void CreateGLObjects();
  void Create2DProgramOnce();
  void CreateRectangleARBProgramOnce();
  void CreateExternalOESProgramOnce();
  Shader CreateProgram(const char* vertex_shader, const char* fragment_shader);
  void CreateShader(GLuint program, GLenum type, const char* source, int size);
  void PaintNextPicture();
  void PaintFinished(int32_t result);

  pp::Size plugin_size_;
  bool is_painting_;
  // When decode outpaces render, we queue up decoded pictures for later
  // painting.
  typedef std::queue<PendingPicture> PendingPictureQueue;
  PendingPictureQueue pending_pictures_;

  int num_frames_rendered_;
  PP_TimeTicks first_frame_delivered_ticks_;
  PP_TimeTicks last_swap_request_ticks_;
  PP_TimeTicks swap_ticks_;
  pp::CompletionCallbackFactory<MyInstance> callback_factory_;

  // Unowned pointers.
  const PPB_Console* console_if_;
  const PPB_Core* core_if_;
  const PPB_OpenGLES2* gles2_if_;

  // Owned data.
  pp::Graphics3D* context_;
  typedef std::vector<Decoder*> DecoderList;
  DecoderList video_decoders_;

  // Shader program to draw GL_TEXTURE_2D target.
  Shader shader_2d_;
  // Shader program to draw GL_TEXTURE_RECTANGLE_ARB target.
  Shader shader_rectangle_arb_;
  // Shader program to draw GL_TEXTURE_EXTERNAL_OES target.
  Shader shader_external_oes_;
};

class Decoder {
 public:
  Decoder(MyInstance* instance, int id, const pp::Graphics3D& graphics_3d);
  ~Decoder();

  int id() const { return id_; }
  bool flushing() const { return flushing_; }
  bool resetting() const { return resetting_; }

  void Reset();
  void RecyclePicture(const PP_VideoPicture& picture);

  PP_TimeTicks GetAverageLatency() {
    return num_pictures_ ? total_latency_ / num_pictures_ : 0;
  }

 private:
  void InitializeDone(int32_t result);
  void Start();
  void DecodeNextFrame();
  void DecodeDone(int32_t result);
  void PictureReady(int32_t result, PP_VideoPicture picture);
  void FlushDone(int32_t result);
  void ResetDone(int32_t result);

  MyInstance* instance_;
  int id_;

  pp::VideoDecoder* decoder_;
  pp::CompletionCallbackFactory<Decoder> callback_factory_;

  size_t encoded_data_next_pos_to_decode_;
  int next_picture_id_;
  bool flushing_;
  bool resetting_;

  const PPB_Core* core_if_;
  static const int kMaxDecodeDelay = 128;
  PP_TimeTicks decode_time_[kMaxDecodeDelay];
  PP_TimeTicks total_latency_;
  int num_pictures_;
};

#if defined USE_VP8_TESTDATA_INSTEAD_OF_H264

// VP8 is stored in an IVF container.
// Helpful description: http://wiki.multimedia.cx/index.php?title=IVF

static void GetNextFrame(size_t* start_pos, size_t* end_pos) {
  size_t current_pos = *start_pos;
  if (current_pos == 0)
    current_pos = 32;  // Skip stream header.
  uint32_t frame_size = kData[current_pos] + (kData[current_pos + 1] << 8) +
                        (kData[current_pos + 2] << 16) +
                        (kData[current_pos + 3] << 24);
  current_pos += 12;  // Skip frame header.
  *start_pos = current_pos;
  *end_pos = current_pos + frame_size;
}

#else  // !USE_VP8_TESTDATA_INSTEAD_OF_H264

// Returns true if the current position is at the start of a NAL unit.
static bool LookingAtNAL(const unsigned char* encoded, size_t pos) {
  // H264 frames start with 0, 0, 0, 1 in our test data.
  return pos + 3 < kDataLen && encoded[pos] == 0 && encoded[pos + 1] == 0 &&
         encoded[pos + 2] == 0 && encoded[pos + 3] == 1;
}

static void GetNextFrame(size_t* start_pos, size_t* end_pos) {
  assert(LookingAtNAL(kData, *start_pos));
  *end_pos = *start_pos;
  *end_pos += 4;
  while (*end_pos < kDataLen && !LookingAtNAL(kData, *end_pos)) {
    ++*end_pos;
  }
}

#endif  // USE_VP8_TESTDATA_INSTEAD_OF_H264

Decoder::Decoder(MyInstance* instance,
                 int id,
                 const pp::Graphics3D& graphics_3d)
    : instance_(instance),
      id_(id),
      decoder_(new pp::VideoDecoder(instance)),
      callback_factory_(this),
      encoded_data_next_pos_to_decode_(0),
      next_picture_id_(0),
      flushing_(false),
      resetting_(false),
      total_latency_(0.0),
      num_pictures_(0) {
  core_if_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));

#if defined USE_VP8_TESTDATA_INSTEAD_OF_H264
  const PP_VideoProfile kBitstreamProfile = PP_VIDEOPROFILE_VP8_ANY;
#else
  const PP_VideoProfile kBitstreamProfile = PP_VIDEOPROFILE_H264MAIN;
#endif

  assert(!decoder_->is_null());
  decoder_->Initialize(graphics_3d,
                       kBitstreamProfile,
                       PP_HARDWAREACCELERATION_WITHFALLBACK,
                       0,
                       callback_factory_.NewCallback(&Decoder::InitializeDone));
}

Decoder::~Decoder() {
  delete decoder_;
}

void Decoder::InitializeDone(int32_t result) {
  assert(decoder_);
  assert(result == PP_OK);
  Start();
}

void Decoder::Start() {
  assert(decoder_);

  encoded_data_next_pos_to_decode_ = 0;

  // Register callback to get the first picture. We call GetPicture again in
  // PictureReady to continuously receive pictures as they're decoded.
  decoder_->GetPicture(
      callback_factory_.NewCallbackWithOutput(&Decoder::PictureReady));

  // Start the decode loop.
  DecodeNextFrame();
}

void Decoder::Reset() {
  assert(decoder_);
  assert(!resetting_);
  resetting_ = true;
  decoder_->Reset(callback_factory_.NewCallback(&Decoder::ResetDone));
}

void Decoder::RecyclePicture(const PP_VideoPicture& picture) {
  assert(decoder_);
  decoder_->RecyclePicture(picture);
}

void Decoder::DecodeNextFrame() {
  assert(decoder_);
  if (encoded_data_next_pos_to_decode_ <= kDataLen) {
    // If we've just reached the end of the bitstream, flush and wait.
    if (!flushing_ && encoded_data_next_pos_to_decode_ == kDataLen) {
      flushing_ = true;
      decoder_->Flush(callback_factory_.NewCallback(&Decoder::FlushDone));
      return;
    }

    // Find the start of the next frame.
    size_t start_pos = encoded_data_next_pos_to_decode_;
    size_t end_pos;
    GetNextFrame(&start_pos, &end_pos);
    encoded_data_next_pos_to_decode_ = end_pos;
    // Decode the frame. On completion, DecodeDone will call DecodeNextFrame
    // to implement a decode loop.
    uint32_t size = static_cast<uint32_t>(end_pos - start_pos);
    decode_time_[next_picture_id_ % kMaxDecodeDelay] = core_if_->GetTimeTicks();
    decoder_->Decode(next_picture_id_++,
                     size,
                     kData + start_pos,
                     callback_factory_.NewCallback(&Decoder::DecodeDone));
  }
}

void Decoder::DecodeDone(int32_t result) {
  assert(decoder_);
  // Break out of the decode loop on abort.
  if (result == PP_ERROR_ABORTED)
    return;
  assert(result == PP_OK);
  if (!flushing_ && !resetting_)
    DecodeNextFrame();
}

void Decoder::PictureReady(int32_t result, PP_VideoPicture picture) {
  assert(decoder_);
  // Break out of the get picture loop on abort.
  if (result == PP_ERROR_ABORTED)
    return;
  assert(result == PP_OK);

  num_pictures_++;
  PP_TimeTicks latency = core_if_->GetTimeTicks() -
                         decode_time_[picture.decode_id % kMaxDecodeDelay];
  total_latency_ += latency;

  decoder_->GetPicture(
      callback_factory_.NewCallbackWithOutput(&Decoder::PictureReady));
  instance_->PaintPicture(this, picture);
}

void Decoder::FlushDone(int32_t result) {
  assert(decoder_);
  assert(result == PP_OK || result == PP_ERROR_ABORTED);
  assert(flushing_);
  flushing_ = false;
}

void Decoder::ResetDone(int32_t result) {
  assert(decoder_);
  assert(result == PP_OK);
  assert(resetting_);
  resetting_ = false;

  Start();
}

MyInstance::MyInstance(PP_Instance instance, pp::Module* module)
    : pp::Instance(instance),
      pp::Graphics3DClient(this),
      is_painting_(false),
      num_frames_rendered_(0),
      first_frame_delivered_ticks_(-1),
      last_swap_request_ticks_(-1),
      swap_ticks_(0),
      callback_factory_(this),
      context_(NULL) {
  console_if_ = static_cast<const PPB_Console*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CONSOLE_INTERFACE));
  core_if_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  gles2_if_ = static_cast<const PPB_OpenGLES2*>(
      pp::Module::Get()->GetBrowserInterface(PPB_OPENGLES2_INTERFACE));

  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
}

MyInstance::~MyInstance() {
  if (!context_)
    return;

  PP_Resource graphics_3d = context_->pp_resource();
  if (shader_2d_.program)
    gles2_if_->DeleteProgram(graphics_3d, shader_2d_.program);
  if (shader_rectangle_arb_.program)
    gles2_if_->DeleteProgram(graphics_3d, shader_rectangle_arb_.program);
  if (shader_external_oes_.program)
    gles2_if_->DeleteProgram(graphics_3d, shader_external_oes_.program);

  for (DecoderList::iterator it = video_decoders_.begin();
       it != video_decoders_.end();
       ++it)
    delete *it;

  delete context_;
}

void MyInstance::DidChangeView(const pp::Rect& position,
                               const pp::Rect& clip_ignored) {
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

bool MyInstance::HandleInputEvent(const pp::InputEvent& event) {
  switch (event.GetType()) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN: {
      pp::MouseInputEvent mouse_event(event);
      // Reset all decoders on mouse down.
      if (mouse_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_LEFT) {
        // Reset decoders.
        for (size_t i = 0; i < video_decoders_.size(); i++) {
          if (!video_decoders_[i]->resetting())
            video_decoders_[i]->Reset();
        }
      }
      return true;
    }

    default:
      return false;
  }
}

void MyInstance::InitializeDecoders() {
  assert(video_decoders_.empty());
  // Create two decoders with ids 0 and 1.
  video_decoders_.push_back(new Decoder(this, 0, *context_));
  video_decoders_.push_back(new Decoder(this, 1, *context_));
}

void MyInstance::PaintPicture(Decoder* decoder,
                              const PP_VideoPicture& picture) {
  if (first_frame_delivered_ticks_ == -1)
    assert((first_frame_delivered_ticks_ = core_if_->GetTimeTicks()) != -1);

  pending_pictures_.push(PendingPicture(decoder, picture));
  if (!is_painting_)
    PaintNextPicture();
}

void MyInstance::PaintNextPicture() {
  assert(!is_painting_);
  is_painting_ = true;

  const PendingPicture& next = pending_pictures_.front();
  Decoder* decoder = next.decoder;
  const PP_VideoPicture& picture = next.picture;

  int x = 0;
  int y = 0;
  int half_width = plugin_size_.width() / 2;
  int half_height = plugin_size_.height() / 2;
  if (decoder->id() != 0) {
    x = half_width;
    y = half_height;
  }

  PP_Resource graphics_3d = context_->pp_resource();
  if (picture.texture_target == GL_TEXTURE_2D) {
    Create2DProgramOnce();
    gles2_if_->UseProgram(graphics_3d, shader_2d_.program);
    gles2_if_->Uniform2f(
        graphics_3d, shader_2d_.texcoord_scale_location, 1.0, 1.0);
  } else if (picture.texture_target == GL_TEXTURE_RECTANGLE_ARB) {
    CreateRectangleARBProgramOnce();
    gles2_if_->UseProgram(graphics_3d, shader_rectangle_arb_.program);
    gles2_if_->Uniform2f(graphics_3d,
                         shader_rectangle_arb_.texcoord_scale_location,
                         picture.texture_size.width,
                         picture.texture_size.height);
  } else {
    assert(picture.texture_target == GL_TEXTURE_EXTERNAL_OES);
    CreateExternalOESProgramOnce();
    gles2_if_->UseProgram(graphics_3d, shader_external_oes_.program);
    gles2_if_->Uniform2f(
        graphics_3d, shader_external_oes_.texcoord_scale_location, 1.0, 1.0);
  }

  gles2_if_->Viewport(graphics_3d, x, y, half_width, half_height);
  gles2_if_->ActiveTexture(graphics_3d, GL_TEXTURE0);
  gles2_if_->BindTexture(
      graphics_3d, picture.texture_target, picture.texture_id);
  gles2_if_->DrawArrays(graphics_3d, GL_TRIANGLE_STRIP, 0, 4);

  gles2_if_->UseProgram(graphics_3d, 0);

  last_swap_request_ticks_ = core_if_->GetTimeTicks();
  context_->SwapBuffers(
      callback_factory_.NewCallback(&MyInstance::PaintFinished));
}

void MyInstance::PaintFinished(int32_t result) {
  assert(result == PP_OK);
  swap_ticks_ += core_if_->GetTimeTicks() - last_swap_request_ticks_;
  is_painting_ = false;
  ++num_frames_rendered_;
  if (num_frames_rendered_ % 50 == 0) {
    double elapsed = core_if_->GetTimeTicks() - first_frame_delivered_ticks_;
    double fps = (elapsed > 0) ? num_frames_rendered_ / elapsed : 1000;
    double ms_per_swap = (swap_ticks_ * 1e3) / num_frames_rendered_;
    double secs_average_latency = 0;
    for (DecoderList::iterator it = video_decoders_.begin();
         it != video_decoders_.end();
         ++it)
      secs_average_latency += (*it)->GetAverageLatency();
    secs_average_latency /= video_decoders_.size();
    double ms_average_latency = 1000 * secs_average_latency;
    LogError(this).s() << "Rendered frames: " << num_frames_rendered_
                       << ", fps: " << fps
                       << ", with average ms/swap of: " << ms_per_swap
                       << ", with average latency (ms) of: "
                       << ms_average_latency;
  }

  // If the decoders were reset, this will be empty.
  if (pending_pictures_.empty())
    return;

  const PendingPicture& next = pending_pictures_.front();
  Decoder* decoder = next.decoder;
  const PP_VideoPicture& picture = next.picture;
  decoder->RecyclePicture(picture);
  pending_pictures_.pop();

  // Keep painting as long as we have pictures.
  if (!pending_pictures_.empty())
    PaintNextPicture();
}

void MyInstance::InitGL() {
  assert(plugin_size_.width() && plugin_size_.height());
  is_painting_ = false;

  assert(!context_);
  int32_t context_attributes[] = {
      PP_GRAPHICS3DATTRIB_ALPHA_SIZE,     8,
      PP_GRAPHICS3DATTRIB_BLUE_SIZE,      8,
      PP_GRAPHICS3DATTRIB_GREEN_SIZE,     8,
      PP_GRAPHICS3DATTRIB_RED_SIZE,       8,
      PP_GRAPHICS3DATTRIB_DEPTH_SIZE,     0,
      PP_GRAPHICS3DATTRIB_STENCIL_SIZE,   0,
      PP_GRAPHICS3DATTRIB_SAMPLES,        0,
      PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
      PP_GRAPHICS3DATTRIB_WIDTH,          plugin_size_.width(),
      PP_GRAPHICS3DATTRIB_HEIGHT,         plugin_size_.height(),
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

void MyInstance::CreateGLObjects() {
  // Assign vertex positions and texture coordinates to buffers for use in
  // shader program.
  static const float kVertices[] = {
      -1, -1, -1, 1, 1, -1, 1, 1,  // Position coordinates.
      0,  1,  0,  0, 1, 1,  1, 0,  // Texture coordinates.
  };

  GLuint buffer;
  gles2_if_->GenBuffers(context_->pp_resource(), 1, &buffer);
  gles2_if_->BindBuffer(context_->pp_resource(), GL_ARRAY_BUFFER, buffer);

  gles2_if_->BufferData(context_->pp_resource(),
                        GL_ARRAY_BUFFER,
                        sizeof(kVertices),
                        kVertices,
                        GL_STATIC_DRAW);
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

void MyInstance::Create2DProgramOnce() {
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

void MyInstance::CreateRectangleARBProgramOnce() {
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
  assertNoGLError();
}

void MyInstance::CreateExternalOESProgramOnce() {
  if (shader_external_oes_.program)
    return;
  static const char kFragmentShaderExternal[] =
      "#extension GL_OES_EGL_image_external : require\n"
      "precision mediump float;            \n"
      "varying vec2 v_texCoord;            \n"
      "uniform samplerExternalOES s_texture; \n"
      "void main()                         \n"
      "{"
      "    gl_FragColor = texture2D(s_texture, v_texCoord); \n"
      "}";
  shader_external_oes_ = CreateProgram(kVertexShader, kFragmentShaderExternal);
  assertNoGLError();
}

Shader MyInstance::CreateProgram(const char* vertex_shader,
                                 const char* fragment_shader) {
  Shader shader;

  // Create shader program.
  shader.program = gles2_if_->CreateProgram(context_->pp_resource());
  CreateShader(
      shader.program, GL_VERTEX_SHADER, vertex_shader, strlen(vertex_shader));
  CreateShader(shader.program,
               GL_FRAGMENT_SHADER,
               fragment_shader,
               strlen(fragment_shader));
  gles2_if_->LinkProgram(context_->pp_resource(), shader.program);
  gles2_if_->UseProgram(context_->pp_resource(), shader.program);
  gles2_if_->Uniform1i(
      context_->pp_resource(),
      gles2_if_->GetUniformLocation(
          context_->pp_resource(), shader.program, "s_texture"),
      0);
  assertNoGLError();

  shader.texcoord_scale_location = gles2_if_->GetUniformLocation(
      context_->pp_resource(), shader.program, "v_scale");

  GLint pos_location = gles2_if_->GetAttribLocation(
      context_->pp_resource(), shader.program, "a_position");
  GLint tc_location = gles2_if_->GetAttribLocation(
      context_->pp_resource(), shader.program, "a_texCoord");
  assertNoGLError();

  gles2_if_->EnableVertexAttribArray(context_->pp_resource(), pos_location);
  gles2_if_->VertexAttribPointer(
      context_->pp_resource(), pos_location, 2, GL_FLOAT, GL_FALSE, 0, 0);
  gles2_if_->EnableVertexAttribArray(context_->pp_resource(), tc_location);
  gles2_if_->VertexAttribPointer(
      context_->pp_resource(),
      tc_location,
      2,
      GL_FLOAT,
      GL_FALSE,
      0,
      static_cast<float*>(0) + 8);  // Skip position coordinates.

  gles2_if_->UseProgram(context_->pp_resource(), 0);
  assertNoGLError();
  return shader;
}

void MyInstance::CreateShader(GLuint program,
                              GLenum type,
                              const char* source,
                              int size) {
  GLuint shader = gles2_if_->CreateShader(context_->pp_resource(), type);
  gles2_if_->ShaderSource(context_->pp_resource(), shader, 1, &source, &size);
  gles2_if_->CompileShader(context_->pp_resource(), shader);
  gles2_if_->AttachShader(context_->pp_resource(), program, shader);
  gles2_if_->DeleteShader(context_->pp_resource(), shader);
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance, this);
  }
};

}  // anonymous namespace

namespace pp {
// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}
}  // namespace pp
