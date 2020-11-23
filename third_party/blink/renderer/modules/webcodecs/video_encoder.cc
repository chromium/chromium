// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "media/base/async_destroy_video_encoder.h"
#include "media/base/media_util.h"
#include "media/base/mime_util.h"
#include "media/base/offloading_video_encoder.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_encoder.h"
#if BUILDFLAG(ENABLE_OPENH264)
#include "media/video/openh264_video_encoder.h"
#endif
#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/video/vpx_video_encoder.h"
#endif
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator_adapter.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"
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

std::unique_ptr<media::VideoEncoder> CreateAcceleratedVideoEncoder(
    media::VideoCodecProfile profile,
    const media::VideoEncoder::Options& options) {
  auto* gpu_factories = Platform::Current()->GetGpuFactories();
  if (!gpu_factories || !gpu_factories->IsGpuVideoAcceleratorEnabled())
    return nullptr;

  auto supported_profiles =
      gpu_factories->GetVideoEncodeAcceleratorSupportedProfiles().value_or(
          media::VideoEncodeAccelerator::SupportedProfiles());

  bool found_supported_profile = false;
  for (auto& supported_profile : supported_profiles) {
    if (supported_profile.profile != profile)
      continue;

    if (supported_profile.min_resolution.width() > options.width ||
        supported_profile.min_resolution.height() > options.height) {
      continue;
    }

    if (supported_profile.max_resolution.width() < options.width ||
        supported_profile.max_resolution.height() < options.height) {
      continue;
    }

    double max_supported_framerate =
        double{supported_profile.max_framerate_numerator} /
        supported_profile.max_framerate_denominator;
    if (options.framerate.has_value() &&
        options.framerate.value() > max_supported_framerate) {
      continue;
    }

    found_supported_profile = true;
    break;
  }

  if (!found_supported_profile)
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

std::unique_ptr<media::VideoEncoder> CreateOpenH264VideoEncoder() {
#if BUILDFLAG(ENABLE_OPENH264)
  return std::make_unique<media::OpenH264VideoEncoder>();
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_OPENH264)
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
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      state_(V8CodecState::Enum::kUnconfigured),
      script_state_(script_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);

  ExecutionContext* context = GetExecutionContext();

  DCHECK(context);

  parent_media_log_ = Platform::Current()->GetMediaLog(
      MediaInspectorContextImpl::From(*context),
      Thread::MainThread()->GetTaskRunner());

  if (!parent_media_log_)
    parent_media_log_ = std::make_unique<media::NullMediaLog>();

  // This allows us to destroy |parent_media_log_| and stop logging,
  // without causing problems to |media_log_| users.
  media_log_ = parent_media_log_->Clone();

  media_log_->SetProperty<media::MediaLogProperty::kFrameTitle>(
      std::string("VideoEncoder(WebCodecs)"));
  media_log_->SetProperty<media::MediaLogProperty::kFrameUrl>(
      GetExecutionContext()->Url().GetString().Ascii());

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

VideoEncoder::ParsedConfig* VideoEncoder::ParseConfig(
    const VideoEncoderConfig* config,
    ExceptionState& exception_state) {
  constexpr int kMaxSupportedFrameSize = 8000;
  auto* parsed = MakeGarbageCollected<ParsedConfig>();

  parsed->options.height = config->height();
  if (parsed->options.height == 0 ||
      parsed->options.height > kMaxSupportedFrameSize) {
    exception_state.ThrowTypeError("Invalid height.");
    return nullptr;
  }

  parsed->options.width = config->width();
  if (parsed->options.width == 0 ||
      parsed->options.width > kMaxSupportedFrameSize) {
    exception_state.ThrowTypeError("Invalid width.");
    return nullptr;
  }

  if (config->hasFramerate())
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
  parsed->codec_string = config->codec();

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
      break;

    default:
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        "Unsupported codec type.");
      return false;
  }

  return true;
}

void VideoEncoder::UpdateEncoderLog(std::string encoder_name,
                                    bool is_hw_accelerated) {
  // TODO(https://crbug.com/1139089) : Add encoder properties.
  media_log_->SetProperty<media::MediaLogProperty::kVideoDecoderName>(
      encoder_name);
  media_log_->SetProperty<media::MediaLogProperty::kIsPlatformVideoDecoder>(
      is_hw_accelerated);
}

void VideoEncoder::CreateAndInitializeEncoderOnEncoderSupportKnown(
    Request* request) {
  DCHECK(active_config_);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media_encoder_ = CreateMediaVideoEncoder(*active_config_);
  if (!media_encoder_) {
    HandleError(
        "Encoder creation error.",
        media::Status(media::StatusCode::kEncoderInitializationError,
                      "Unable to create encoder (most likely unsupported "
                      "codec/acceleration requirement combination)"));
    return;
  }

  auto output_cb = WTF::BindRepeating(
      &VideoEncoder::CallOutputCallback, WrapCrossThreadWeakPersistent(this),
      // We can't use |active_config_| from |this| because it can change by
      // the time the callback is executed.
      WrapCrossThreadPersistent(active_config_.Get()), reset_count_);

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::Status status) {
    if (!self || self->reset_count_ != req->reset_count)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    DCHECK(self->active_config_);

    if (!status.is_ok()) {
      self->HandleError("Encoder initialization error.", status);
    }

    self->stall_request_processing_ = false;
    self->ProcessRequests();
  };

  media_encoder_->Initialize(
      active_config_->profile, active_config_->options, std::move(output_cb),
      WTF::Bind(done_callback, WrapCrossThreadWeakPersistent(this),
                WrapCrossThreadPersistent(request)));
}

std::unique_ptr<media::VideoEncoder> VideoEncoder::CreateMediaVideoEncoder(
    const ParsedConfig& config) {
  // TODO(https://crbug.com/1119636): Implement / call a proper method for
  // detecting support of encoder configs.
  switch (config.acc_pref) {
    case AccelerationPreference::kRequire: {
      auto result =
          CreateAcceleratedVideoEncoder(config.profile, config.options);
      is_hw_accelerated_ = !!result;
      if (result)
        UpdateEncoderLog("AcceleratedVideoEncoder", true);
      return result;
    }
    case AccelerationPreference::kAllow:
      if (auto result =
              CreateAcceleratedVideoEncoder(config.profile, config.options)) {
        is_hw_accelerated_ = true;
        UpdateEncoderLog("AcceleratedVideoEncoder", true);
        return result;
      }
      FALLTHROUGH;
    case AccelerationPreference::kDeny: {
      std::unique_ptr<media::VideoEncoder> result;
      switch (config.codec) {
        case media::kCodecVP8:
        case media::kCodecVP9:
          result = CreateVpxVideoEncoder();
          UpdateEncoderLog("VpxVideoEncoder", false);
          break;
        case media::kCodecH264:
          result = CreateOpenH264VideoEncoder();
          UpdateEncoderLog("OpenH264VideoEncoder", false);
          break;
        default:
          return nullptr;
      }
      is_hw_accelerated_ = false;
      if (!result)
        return nullptr;
      return std::make_unique<media::OffloadingVideoEncoder>(std::move(result));
    }

    default:
      NOTREACHED();
      return nullptr;
  }
}

bool VideoEncoder::CanReconfigure(ParsedConfig& original_config,
                                  ParsedConfig& new_config) {
  // Reconfigure is intended for things that don't require changing underlying
  // codec implementatio and can be changed on the fly.
  return original_config.codec == new_config.codec &&
         original_config.profile == new_config.profile &&
         original_config.level == new_config.level &&
         original_config.color_space == new_config.color_space &&
         original_config.acc_pref == new_config.acc_pref;
}

void VideoEncoder::configure(const VideoEncoderConfig* config,
                             ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ThrowIfCodecStateClosed(state_, "configure", exception_state))
    return;

  auto* parsed_config = ParseConfig(config, exception_state);
  if (!parsed_config) {
    DCHECK(exception_state.HadException());
    return;
  }

  if (!VerifyCodecSupport(parsed_config, exception_state)) {
    DCHECK(exception_state.HadException());
    return;
  }

  Request* request = MakeGarbageCollected<Request>();
  request->reset_count = reset_count_;
  if (media_encoder_ && active_config_ &&
      state_.AsEnum() == V8CodecState::Enum::kConfigured &&
      CanReconfigure(*active_config_, *parsed_config)) {
    request->type = Request::Type::kReconfigure;
  } else {
    state_ = V8CodecState(V8CodecState::Enum::kConfigured);
    request->type = Request::Type::kConfigure;
  }
  active_config_ = parsed_config;
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

  DCHECK(active_config_);
  if (internal_frame->cropWidth() != uint32_t{active_config_->options.width} ||
      internal_frame->cropHeight() !=
          uint32_t{active_config_->options.height}) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Frame size doesn't match initial encoder parameters.");

    // Free the temporary clone.
    internal_frame->destroy();
    return;
  }

  // At this point, we have "consumed" the frame, and will destroy the clone
  // in ProcessEncode().
  frame->destroy();

  Request* request = MakeGarbageCollected<Request>();
  request->reset_count = reset_count_;
  request->type = Request::Type::kEncode;
  request->frame = internal_frame;
  request->encodeOpts = opts;
  ++requested_encodes_;
  EnqueueRequest(request);
}

void VideoEncoder::close(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ThrowIfCodecStateClosed(state_, "close", exception_state))
    return;

  state_ = V8CodecState(V8CodecState::Enum::kClosed);

  ResetInternal();
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
  request->resolver = MakePromise();
  request->reset_count = reset_count_;
  request->type = Request::Type::kFlush;
  EnqueueRequest(request);
  return request->resolver->Promise();
}

void VideoEncoder::reset(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ThrowIfCodecStateClosed(state_, "reset", exception_state))
    return;

  state_ = V8CodecState(V8CodecState::Enum::kUnconfigured);
  ResetInternal();
  media_encoder_.reset();
}

void VideoEncoder::ResetInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reset_count_++;
  while (!requests_.empty()) {
    Request* pending_req = requests_.TakeFirst();
    DCHECK(pending_req);
    RejectPromise(pending_req);
  }
  stall_request_processing_ = false;
}

ScriptPromiseResolver* VideoEncoder::MakePromise() {
  outstanding_promises_++;
  return MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
}

void VideoEncoder::ResolvePromise(Request* req) {
  if (!req || !req->resolver)
    return;
  req->resolver.Release()->Resolve();
  DCHECK_GT(outstanding_promises_, 0u);
  outstanding_promises_--;
}

void VideoEncoder::RejectPromise(Request* req, DOMException* ex) {
  if (!req || !req->resolver)
    return;
  auto* resolver = req->resolver.Release();
  if (ex)
    resolver->Reject(ex);
  else
    resolver->Reject();
  DCHECK_GT(outstanding_promises_, 0u);
  outstanding_promises_--;
}

void VideoEncoder::HandleError(DOMException* ex) {
  // Save a temp before we clear the callback.
  V8WebCodecsErrorCallback* error_callback = error_callback_.Get();

  state_ = V8CodecState(V8CodecState::Enum::kClosed);

  ResetInternal();

  // Errors are permanent. Shut everything down.
  error_callback_.Clear();
  media_encoder_.reset();
  output_callback_.Clear();

  if (!script_state_->ContextIsValid() || !error_callback)
    return;

  ScriptState::Scope scope(script_state_);
  error_callback->InvokeAndReportException(nullptr, ex);
}

void VideoEncoder::HandleError(std::string error_message,
                               media::Status status) {
  media_log_->NotifyError(status);

  // For now, the only uses of this method correspond to kOperationErrors.
  auto* ex = MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kOperationError, error_message.c_str());
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
      case Request::Type::kReconfigure:
        ProcessReconfigure(request);
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
    if (!self || self->reset_count_ != req->reset_count)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      self->HandleError("Encoding error.", status);
    }
    self->ProcessRequests();
  };

  scoped_refptr<media::VideoFrame> frame = request->frame->frame();
  if (frame->HasGpuMemoryBuffer() && !is_hw_accelerated_) {
    frame = ConvertToI420Frame(frame);
    if (!frame) {
      HandleError("Unexpected frame format.",
                  media::Status(media::StatusCode::kEncoderFailedEncode,
                                "Unexpected frame format"));
      return;
    }
  }

  bool keyframe = request->encodeOpts->hasKeyFrameNonNull() &&
                  request->encodeOpts->keyFrameNonNull();
  --requested_encodes_;
  media_encoder_->Encode(
      frame, keyframe,
      WTF::Bind(done_callback, WrapCrossThreadWeakPersistent(this),
                WrapCrossThreadPersistent(request)));

  // We passed a copy of frame() above, so this should be safe to destroy
  // here.
  request->frame->destroy();
}

void VideoEncoder::ProcessConfigure(Request* request) {
  DCHECK_NE(state_.AsEnum(), V8CodecState::Enum::kClosed);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK(active_config_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* gpu_factories = Platform::Current()->GetGpuFactories();

  stall_request_processing_ = true;
  bool deny_hardware_encoder =
      active_config_->acc_pref == AccelerationPreference::kDeny;
  if (!deny_hardware_encoder && gpu_factories &&
      gpu_factories->IsGpuVideoAcceleratorEnabled()) {
    // Delay create the hw encoder until HW encoder support is known, so that
    // GetVideoEncodeAcceleratorSupportedProfiles() can give a reliable answer.
    auto on_encoder_support_known_cb = WTF::Bind(
        &VideoEncoder::CreateAndInitializeEncoderOnEncoderSupportKnown,
        WrapCrossThreadWeakPersistent(this),
        WrapCrossThreadPersistent(request));
    gpu_factories->NotifyEncoderSupportKnown(
        std::move(on_encoder_support_known_cb));
  } else {
    CreateAndInitializeEncoderOnEncoderSupportKnown(request);
  }
}

void VideoEncoder::ProcessReconfigure(Request* request) {
  DCHECK_EQ(state_.AsEnum(), V8CodecState::Enum::kConfigured);
  DCHECK_EQ(request->type, Request::Type::kReconfigure);
  DCHECK(active_config_);
  DCHECK(media_encoder_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto reconf_done_callback = [](VideoEncoder* self, Request* req,
                                 media::Status status) {
    if (!self || self->reset_count_ != req->reset_count)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    DCHECK(self->active_config_);

    if (status.is_ok()) {
      self->stall_request_processing_ = false;
      self->ProcessRequests();
    } else {
      // Reconfiguration failed. Either encoder doesn't support changing options
      // or it didn't like this particular change. Let's try to configure it
      // from scratch.
      req->type = Request::Type::kConfigure;
      self->ProcessConfigure(req);
    }
  };

  auto flush_done_callback = [](VideoEncoder* self, Request* req,
                                decltype(reconf_done_callback) reconf_callback,
                                media::Status status) {
    if (!self || self->reset_count_ != req->reset_count)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      self->HandleError("Encoder reconfiguration error.", status);
      self->stall_request_processing_ = false;
      return;
    }

    auto output_cb = WTF::BindRepeating(
        &VideoEncoder::CallOutputCallback, WrapCrossThreadWeakPersistent(self),
        // We can't use |active_config_| from |this| because it can change by
        // the time the callback is executed.
        WrapCrossThreadPersistent(self->active_config_.Get()),
        self->reset_count_);

    self->media_encoder_->ChangeOptions(
        self->active_config_->options, std::move(output_cb),
        WTF::Bind(reconf_callback, WrapCrossThreadWeakPersistent(self),
                  WrapCrossThreadPersistent(req)));
  };

  stall_request_processing_ = true;
  media_encoder_->Flush(WTF::Bind(
      flush_done_callback, WrapCrossThreadWeakPersistent(this),
      WrapCrossThreadPersistent(request), std::move(reconf_done_callback)));
}

void VideoEncoder::ProcessFlush(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kFlush);

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::Status status) {
    if (!self)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (self->reset_count_ != req->reset_count) {
      self->RejectPromise(req);
      return;
    }
    DCHECK(req->resolver);
    if (status.is_ok()) {
      self->ResolvePromise(req);
    } else {
      std::string error_msg = "Flushing error.";
      self->HandleError(error_msg, status);
      auto* ex = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, error_msg.c_str());
      self->RejectPromise(req, ex);
    }
    self->stall_request_processing_ = false;
    self->ProcessRequests();
  };

  stall_request_processing_ = true;

  media_encoder_->Flush(WTF::Bind(done_callback,
                                  WrapCrossThreadWeakPersistent(this),
                                  WrapCrossThreadPersistent(request)));
}

void VideoEncoder::CallOutputCallback(
    ParsedConfig* active_config,
    uint32_t reset_count,
    media::VideoEncoderOutput output,
    base::Optional<media::VideoEncoder::CodecDescription> codec_desc) {
  DCHECK(active_config);
  if (!script_state_->ContextIsValid() || !output_callback_ ||
      state_.AsEnum() != V8CodecState::Enum::kConfigured ||
      reset_count != reset_count_)
    return;

  EncodedVideoMetadata metadata;
  metadata.timestamp = output.timestamp;
  metadata.key_frame = output.key_frame;
  auto deleter = [](void* data, size_t length, void*) {
    delete[] static_cast<uint8_t*>(data);
  };
  ArrayBufferContents data(output.data.release(), output.size, deleter);
  auto* dom_array = MakeGarbageCollected<DOMArrayBuffer>(std::move(data));
  auto* chunk = MakeGarbageCollected<EncodedVideoChunk>(metadata, dom_array);

  VideoDecoderConfig* decoder_config =
      MakeGarbageCollected<VideoDecoderConfig>();
  decoder_config->setCodec(active_config->codec_string);
  decoder_config->setCodedHeight(active_config->options.height);
  decoder_config->setCodedWidth(active_config->options.width);
  if (codec_desc.has_value()) {
    auto* desc_array_buf = DOMArrayBuffer::Create(codec_desc.value().data(),
                                                  codec_desc.value().size());
    decoder_config->setDescription(
        ArrayBufferOrArrayBufferView::FromArrayBuffer(desc_array_buf));
  }
  ScriptState::Scope scope(script_state_);
  output_callback_->InvokeAndReportException(nullptr, chunk, decoder_config);
}

void VideoEncoder::ContextDestroyed() {
  parent_media_log_ = nullptr;
}

bool VideoEncoder::HasPendingActivity() const {
  return outstanding_promises_ > 0;
}

void VideoEncoder::Trace(Visitor* visitor) const {
  visitor->Trace(active_config_);
  visitor->Trace(script_state_);
  visitor->Trace(output_callback_);
  visitor->Trace(error_callback_);
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void VideoEncoder::Request::Trace(Visitor* visitor) const {
  visitor->Trace(frame);
  visitor->Trace(encodeOpts);
  visitor->Trace(resolver);
}
}  // namespace blink
