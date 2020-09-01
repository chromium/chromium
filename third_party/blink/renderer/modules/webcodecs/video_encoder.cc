// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "media/base/mime_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_encoder.h"
#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/video/vpx_video_encoder.h"
#endif
#include "media/base/async_destroy_video_encoder.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator_adapter.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_state_helper.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_metadata.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/libyuv/include/libyuv.h"

namespace blink {

namespace {
std::unique_ptr<media::VideoEncoder> CreateAcceleratedVideoEncoder() {
  auto* gpu_factories = Platform::Current()->GetGpuFactories();
  if (!gpu_factories || !gpu_factories->IsGpuVideoAcceleratorEnabled())
    return nullptr;

  auto task_runner = Thread::MainThread()->GetTaskRunner();
  return std::make_unique<
      media::AsyncDestroyVideoEncoder<media::VideoEncodeAcceleratorAdapter>>(
      std::make_unique<media::VideoEncodeAcceleratorAdapter>(
          gpu_factories, std::move(task_runner)));
}

std::unique_ptr<media::VideoEncoder> CreateVpxVideoEncoder() {
#if BUILDFLAG(ENABLE_LIBVPX)
  return std::make_unique<media::VpxVideoEncoder>();
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_LIBVPX)
}

scoped_refptr<media::VideoFrame> ConvertToI420Frame(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_EQ(frame->storage_type(),
            media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  // TODO: Support more pixel formats
  if (frame->format() != media::VideoPixelFormat::PIXEL_FORMAT_NV12)
    return nullptr;

  auto* gmb = frame->GetGpuMemoryBuffer();
  if (!gmb->Map())
    return nullptr;
  scoped_refptr<media::VideoFrame> i420_frame = media::VideoFrame::CreateFrame(
      media::VideoPixelFormat::PIXEL_FORMAT_I420, frame->coded_size(),
      frame->visible_rect(), frame->natural_size(), frame->timestamp());
  auto ret = libyuv::NV12ToI420(
      static_cast<const uint8_t*>(gmb->memory(0)), gmb->stride(0),
      static_cast<const uint8_t*>(gmb->memory(1)), gmb->stride(1),
      i420_frame->data(media::VideoFrame::kYPlane),
      i420_frame->stride(media::VideoFrame::kYPlane),
      i420_frame->data(media::VideoFrame::kUPlane),
      i420_frame->stride(media::VideoFrame::kUPlane),
      i420_frame->data(media::VideoFrame::kVPlane),
      i420_frame->stride(media::VideoFrame::kVPlane),
      frame->coded_size().width(), frame->coded_size().height());
  gmb->Unmap();
  if (ret)
    return nullptr;
  return i420_frame;
}

}  // namespace

// static
VideoEncoder* VideoEncoder::Create(ScriptState* script_state,
                                   const VideoEncoderInit* init,
                                   ExceptionState& exception_state) {
  return MakeGarbageCollected<VideoEncoder>(script_state, init,
                                            exception_state);
}

VideoEncoder::VideoEncoder(ScriptState* script_state,
                           const VideoEncoderInit* init,
                           ExceptionState& exception_state)
    : state_(V8CodecState::Enum::kUnconfigured), script_state_(script_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);

  output_callback_ = init->output();
  if (init->hasError())
    error_callback_ = init->error();
}

VideoEncoder::~VideoEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int32_t VideoEncoder::encodeQueueSize() {
  return requested_encodes_;
}

std::unique_ptr<VideoEncoder::ParsedConfig> VideoEncoder::ParseConfig(
    const VideoEncoderConfig* config,
    ExceptionState& exception_state) {
  auto parsed = std::make_unique<ParsedConfig>();

  parsed->options.height = config->height();
  if (parsed->options.height == 0) {
    exception_state.ThrowTypeError("Invalid height.");
    return nullptr;
  }

  parsed->options.width = config->width();
  if (parsed->options.width == 0) {
    exception_state.ThrowTypeError("Invalid width.");
    return nullptr;
  }

  parsed->options.framerate = config->framerate();

  if (config->hasBitrate())
    parsed->options.bitrate = config->bitrate();

  // The IDL defines a default value of "allow".
  DCHECK(config->hasAcceleration());

  std::string preference = IDLEnumAsString(config->acceleration()).Utf8();
  if (preference == "allow") {
    parsed->acc_pref = AccelerationPreference::kAllow;
  } else if (preference == "require") {
    parsed->acc_pref = AccelerationPreference::kRequire;
  } else if (preference == "deny") {
    parsed->acc_pref = AccelerationPreference::kDeny;
  } else {
    NOTREACHED();
  }

  bool is_codec_ambiguous = true;
  parsed->codec = media::kUnknownVideoCodec;
  parsed->profile = media::VIDEO_CODEC_PROFILE_UNKNOWN;
  parsed->color_space = media::VideoColorSpace::REC709();
  parsed->level = 0;

  bool parse_succeeded = media::ParseVideoCodecString(
      "", config->codec().Utf8(), &is_codec_ambiguous, &parsed->codec,
      &parsed->profile, &parsed->level, &parsed->color_space);

  if (!parse_succeeded) {
    exception_state.ThrowTypeError("Invalid codec string.");
    return nullptr;
  }

  if (is_codec_ambiguous) {
    exception_state.ThrowTypeError("Ambiguous codec string.");
    return nullptr;
  }

  return parsed;
}

bool VideoEncoder::VerifyCodecSupport(ParsedConfig* config,
                                      ExceptionState& exception_state) {
  switch (config->codec) {
    case media::kCodecVP8:
      if (config->acc_pref == AccelerationPreference::kRequire) {
        exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                          "Accelerated vp8 is not supported");
        return false;
      }
      break;

    case media::kCodecVP9:
      if (config->acc_pref == AccelerationPreference::kRequire) {
        exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                          "Accelerated vp9 is not supported");
        return false;
      }

      // TODO(https://crbug.com/1119636): Implement / call a proper method for
      // detecting support of encoder configs.
      if (config->profile == media::VideoCodecProfile::VP9PROFILE_PROFILE1 ||
          config->profile == media::VideoCodecProfile::VP9PROFILE_PROFILE3) {
        exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                          "Unsupported vp9 profile.");
        return false;
      }

      break;

    case media::kCodecH264:
      if (config->acc_pref == AccelerationPreference::kDeny) {
        exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                          "Software h264 is not supported yet");
        return false;
      }
      break;

    default:
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        "Unsupported codec type.");
      return false;
  }

  return true;
}

void VideoEncoder::configure(const VideoEncoderConfig* config,
                             ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ThrowIfCodecStateClosed(state_, "configure", exception_state))
    return;

  auto parsed_config = ParseConfig(config, exception_state);

  if (!parsed_config) {
    DCHECK(exception_state.HadException());
    return;
  }

  if (!VerifyCodecSupport(parsed_config.get(), exception_state)) {
    DCHECK(exception_state.HadException());
    return;
  }

  // TODO(https://crbug.com/1119892): flush |media_encoder_| if it already
  // exists, otherwise might could lose frames in flight.

  state_ = V8CodecState(V8CodecState::Enum::kConfigured);

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kConfigure;
  request->config = std::move(parsed_config);
  EnqueueRequest(request);
}

void VideoEncoder::encode(VideoFrame* frame,
                          const VideoEncoderEncodeOptions* opts,
                          ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ThrowIfCodecStateClosed(state_, "encode", exception_state))
    return;

  if (ThrowIfCodecStateUnconfigured(state_, "encode", exception_state))
    return;

  // This will fail if |frame| is already destroyed.
  auto* internal_frame = frame->clone(exception_state);

  if (!internal_frame) {
    // Set a more helpful exception than the cloning error message.
    exception_state.ClearException();
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Cannot encode destroyed frame.");
    return;
  }

  if (internal_frame->cropWidth() != uint32_t{frame_size_.width()} ||
      internal_frame->cropHeight() != uint32_t{frame_size_.height()}) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Frame size doesn't match initial encoder parameters.");

    // Free the temporary clone.
    internal_frame->destroy();
    return;
  }

  // At this point, we have "consumed" the frame, and will destroy the clone in
  // ProcessEncode().
  frame->destroy();

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kEncode;
  request->frame = internal_frame;
  request->encodeOpts = opts;
  ++requested_encodes_;
  return EnqueueRequest(request);
}

void VideoEncoder::close(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ThrowIfCodecStateClosed(state_, "close", exception_state))
    return;

  state_ = V8CodecState(V8CodecState::Enum::kClosed);

  ClearRequests();
  media_encoder_.reset();
  output_callback_.Clear();
  error_callback_.Clear();
}

ScriptPromise VideoEncoder::flush(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ThrowIfCodecStateClosed(state_, "flush", exception_state))
    return ScriptPromise();

  if (ThrowIfCodecStateUnconfigured(state_, "flush", exception_state))
    return ScriptPromise();

  Request* request = MakeGarbageCollected<Request>();
  request->resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  request->type = Request::Type::kFlush;
  EnqueueRequest(request);
  return request->resolver->Promise();
}

void VideoEncoder::reset(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO: Not fully implemented yet
  if (ThrowIfCodecStateClosed(state_, "reset", exception_state))
    return;

  ClearRequests();

  state_ = V8CodecState(V8CodecState::Enum::kUnconfigured);
}

void VideoEncoder::ClearRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!requests_.empty()) {
    Request* pending_req = requests_.TakeFirst();
    DCHECK(pending_req);
    if (pending_req->resolver) {
      auto* ex = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, "reset() was called.");
      pending_req->resolver.Release()->Reject(ex);
    }
  }
}

void VideoEncoder::CallOutputCallback(EncodedVideoChunk* chunk) {
  if (!script_state_->ContextIsValid() || !output_callback_ ||
      state_.AsEnum() != V8CodecState::Enum::kConfigured)
    return;
  ScriptState::Scope scope(script_state_);
  output_callback_->InvokeAndReportException(nullptr, chunk);
}

void VideoEncoder::HandleError(DOMException* ex) {
  // Save a temp before we clear the callback.
  V8WebCodecsErrorCallback* error_callback = error_callback_.Get();

  state_ = V8CodecState(V8CodecState::Enum::kClosed);

  ClearRequests();

  // Errors are permanent. Shut everything down.
  error_callback_.Clear();
  media_encoder_.reset();
  output_callback_.Clear();

  if (!script_state_->ContextIsValid() || !error_callback)
    return;

  ScriptState::Scope scope(script_state_);
  error_callback->InvokeAndReportException(nullptr, ex);
}

void VideoEncoder::HandleError(DOMExceptionCode code, const String& message) {
  auto* ex = MakeGarbageCollected<DOMException>(code, message);
  HandleError(ex);
}

void VideoEncoder::EnqueueRequest(Request* request) {
  requests_.push_back(request);
  ProcessRequests();
}

void VideoEncoder::ProcessRequests() {
  while (!requests_.empty() && !stall_request_processing_) {
    Request* request = requests_.TakeFirst();
    DCHECK(request);
    switch (request->type) {
      case Request::Type::kConfigure:
        ProcessConfigure(request);
        break;
      case Request::Type::kEncode:
        ProcessEncode(request);
        break;
      case Request::Type::kFlush:
        ProcessFlush(request);
        break;
      default:
        NOTREACHED();
    }
  }
}

void VideoEncoder::ProcessEncode(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kEncode);
  DCHECK_GT(requested_encodes_, 0);

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::Status status) {
    if (!self)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      std::string msg = "Encoding error: " + status.message();
      self->HandleError(DOMExceptionCode::kOperationError, msg.c_str());
    }
    self->ProcessRequests();
  };

  scoped_refptr<media::VideoFrame> frame = request->frame->frame();
  if (frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    frame = ConvertToI420Frame(frame);
    if (!frame) {
      HandleError(DOMExceptionCode::kOperationError, "Unexpected frame format");
      return;
    }
  }

  bool keyframe = request->encodeOpts->hasKeyFrameNonNull() &&
                  request->encodeOpts->keyFrameNonNull();
  --requested_encodes_;
  media_encoder_->Encode(frame, keyframe,
                         WTF::Bind(done_callback, WrapWeakPersistent(this),
                                   WrapPersistentIfNeeded(request)));

  // We passed a copy of frame() above, so this should be safe to destroy here.
  request->frame->destroy();
}

void VideoEncoder::ProcessConfigure(Request* request) {
  DCHECK_NE(state_.AsEnum(), V8CodecState::Enum::kClosed);
  DCHECK(request->config);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto config = std::move(request->config);

  switch (config->codec) {
    case media::kCodecVP8:
    case media::kCodecVP9:
      media_encoder_ = CreateVpxVideoEncoder();
      break;

    case media::kCodecH264:
      media_encoder_ = CreateAcceleratedVideoEncoder();
      break;

    default:
      // This should already have been caught in ParseConfig() and
      // VerifyCodecSupport().
      NOTREACHED();
      break;
  }

  if (!media_encoder_) {
    // CreateAcceleratedVideoEncoder() can return a nullptr.
    HandleError(DOMExceptionCode::kOperationError, "Encoder creation error.");
    return;
  }

  frame_size_ = gfx::Size(config->options.width, config->options.height);

  auto output_cb = WTF::BindRepeating(&VideoEncoder::MediaEncoderOutputCallback,
                                      WrapWeakPersistent(this));

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::Status status) {
    if (!self)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      std::string msg = "Encoder initialization error: " + status.message();
      self->HandleError(DOMExceptionCode::kOperationError, msg.c_str());
    }
    self->stall_request_processing_ = false;
    self->ProcessRequests();
  };

  stall_request_processing_ = true;
  media_encoder_->Initialize(config->profile, config->options, output_cb,
                             WTF::Bind(done_callback, WrapWeakPersistent(this),
                                       WrapPersistent(request)));
}

void VideoEncoder::ProcessFlush(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kFlush);

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::Status status) {
    DCHECK(req->resolver);
    if (!self)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (status.is_ok()) {
      req->resolver.Release()->Resolve();
    } else {
      std::string msg = "Flushing error: " + status.message();
      auto* ex = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, msg.c_str());
      self->HandleError(ex);
      req->resolver.Release()->Reject(ex);
    }
    self->stall_request_processing_ = false;
    self->ProcessRequests();
  };

  stall_request_processing_ = true;
  media_encoder_->Flush(WTF::Bind(done_callback, WrapWeakPersistent(this),
                                  WrapPersistentIfNeeded(request)));
}

void VideoEncoder::MediaEncoderOutputCallback(
    media::VideoEncoderOutput output) {
  EncodedVideoMetadata metadata;
  metadata.timestamp = output.timestamp;
  metadata.key_frame = output.key_frame;
  auto deleter = [](void* data, size_t length, void*) {
    delete[] static_cast<uint8_t*>(data);
  };
  ArrayBufferContents data(output.data.release(), output.size, deleter);
  auto* dom_array = MakeGarbageCollected<DOMArrayBuffer>(std::move(data));
  auto* chunk = MakeGarbageCollected<EncodedVideoChunk>(metadata, dom_array);
  CallOutputCallback(chunk);
}

void VideoEncoder::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(output_callback_);
  visitor->Trace(error_callback_);
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
}

void VideoEncoder::Request::Trace(Visitor* visitor) const {
  visitor->Trace(frame);
  visitor->Trace(encodeOpts);
  visitor->Trace(resolver);
}
}  // namespace blink
