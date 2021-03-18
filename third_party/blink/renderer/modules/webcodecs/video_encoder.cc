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
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/async_destroy_video_encoder.h"
#include "media/base/mime_util.h"
#include "media/base/offloading_video_encoder.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_encoder.h"
#include "media/base/video_util.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator_adapter.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_avc_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_state_helper.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

#if BUILDFLAG(ENABLE_OPENH264)
#include "media/video/openh264_video_encoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/video/vpx_video_encoder.h"
#endif

namespace WTF {

template <>
struct CrossThreadCopier<media::Status>
    : public CrossThreadCopierPassThrough<media::Status> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

namespace {

media::GpuVideoAcceleratorFactories* GetGpuFactoriesOnMainThread() {
  DCHECK(IsMainThread());
  return Platform::Current()->GetGpuFactories();
}

std::unique_ptr<media::VideoEncoder> CreateAcceleratedVideoEncoder(
    media::VideoCodecProfile profile,
    const media::VideoEncoder::Options& options,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  if (!gpu_factories || !gpu_factories->IsGpuVideoAcceleratorEnabled())
    return nullptr;

  auto supported_profiles =
      gpu_factories->GetVideoEncodeAcceleratorSupportedProfiles().value_or(
          media::VideoEncodeAccelerator::SupportedProfiles());

  bool found_supported_profile = false;
  for (auto& supported_profile : supported_profiles) {
    if (supported_profile.profile != profile)
      continue;

    if (supported_profile.min_resolution.width() > options.frame_size.width() ||
        supported_profile.min_resolution.height() >
            options.frame_size.height()) {
      continue;
    }

    if (supported_profile.max_resolution.width() < options.frame_size.width() ||
        supported_profile.max_resolution.height() <
            options.frame_size.height()) {
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

  auto task_runner = Thread::Current()->GetTaskRunner();
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

}  // namespace

// static
const char* VideoEncoderTraits::GetNameForDevTools() {
  return "VideoEncoder(WebCodecs)";
}

// static
VideoEncoder* VideoEncoder::Create(ScriptState* script_state,
                                   const VideoEncoderInit* init,
                                   ExceptionState& exception_state) {
  auto* result =
      MakeGarbageCollected<VideoEncoder>(script_state, init, exception_state);
  return exception_state.HadException() ? nullptr : result;
}

VideoEncoder::VideoEncoder(ScriptState* script_state,
                           const VideoEncoderInit* init,
                           ExceptionState& exception_state)
    : Base(script_state, init, exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

VideoEncoder::~VideoEncoder() = default;

VideoEncoder::ParsedConfig* VideoEncoder::ParseConfig(
    const VideoEncoderConfig* config,
    ExceptionState& exception_state) {
  constexpr int kMaxSupportedFrameSize = 8000;
  auto* parsed = MakeGarbageCollected<ParsedConfig>();

  parsed->options.frame_size.set_height(config->height());
  if (parsed->options.frame_size.height() == 0 ||
      parsed->options.frame_size.height() > kMaxSupportedFrameSize) {
    exception_state.ThrowTypeError("Invalid height.");
    return nullptr;
  }

  parsed->options.frame_size.set_width(config->width());
  if (parsed->options.frame_size.width() == 0 ||
      parsed->options.frame_size.width() > kMaxSupportedFrameSize) {
    exception_state.ThrowTypeError("Invalid width.");
    return nullptr;
  }

  if (config->hasFramerate())
    parsed->options.framerate = config->framerate();

  if (config->hasBitrate())
    parsed->options.bitrate = config->bitrate();

  // https://w3c.github.io/webrtc-svc/
  if (config->hasScalabilityMode()) {
    if (config->scalabilityMode() == "L1T2") {
      parsed->options.temporal_layers = 2;
    } else if (config->scalabilityMode() == "L1T3") {
      parsed->options.temporal_layers = 3;
    } else {
      exception_state.ThrowTypeError("Unsupported scalabilityMode.");
      return nullptr;
    }
  }

  // The IDL defines a default value of "allow".
  DCHECK(config->hasHardwareAcceleration());

  parsed->hw_pref = StringToHardwarePreference(
      IDLEnumAsString(config->hardwareAcceleration()));

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

  // We are done with the parsing.
  if (!config->hasAvc())
    return parsed;

  // We should only get here with H264 codecs.
  if (parsed->codec != media::VideoCodec::kCodecH264) {
    exception_state.ThrowTypeError(
        "'avcOptions' can only be used with AVC codecs");
    return nullptr;
  }

  std::string avc_format = IDLEnumAsString(config->avc()->format()).Utf8();
  if (avc_format == "avc") {
    parsed->options.avc.produce_annexb = false;
  } else if (avc_format == "annexb") {
    parsed->options.avc.produce_annexb = true;
  } else {
    NOTREACHED();
  }

  return parsed;
}

bool VideoEncoder::VerifyCodecSupport(ParsedConfig* config,
                                      ExceptionState& exception_state) {
  switch (config->codec) {
    case media::kCodecVP8:
      break;

    case media::kCodecVP9:
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

VideoFrame* VideoEncoder::CloneFrame(VideoFrame* frame,
                                     ExecutionContext* context) {
  return frame->CloneFromNative(context);
}

void VideoEncoder::UpdateEncoderLog(std::string encoder_name,
                                    bool is_hw_accelerated) {
  // TODO(https://crbug.com/1139089) : Add encoder properties.
  media::MediaLog* log = logger_->log();

  log->SetProperty<media::MediaLogProperty::kVideoEncoderName>(encoder_name);
  log->SetProperty<media::MediaLogProperty::kIsPlatformVideoEncoder>(
      is_hw_accelerated);
}

void VideoEncoder::CreateAndInitializeEncoderWithoutAcceleration(
    Request* request) {
  CreateAndInitializeEncoderOnEncoderSupportKnown(request, nullptr);
}

void VideoEncoder::CreateAndInitializeEncoderOnEncoderSupportKnown(
    Request* request,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  DCHECK(active_config_);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media_encoder_ = CreateMediaVideoEncoder(*active_config_, gpu_factories);
  if (!media_encoder_) {
    HandleError(logger_->MakeException(
        "Encoder creation error.",
        media::Status(media::StatusCode::kEncoderInitializationError,
                      "Unable to create encoder (most likely unsupported "
                      "codec/acceleration requirement combination)")));
    return;
  }

  auto output_cb = ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
      &VideoEncoder::CallOutputCallback, WrapCrossThreadWeakPersistent(this),
      // We can't use |active_config_| from |this| because it can change by
      // the time the callback is executed.
      WrapCrossThreadPersistent(active_config_.Get()), reset_count_));

  auto done_callback = [](VideoEncoder* self, Request* req,
                          media::Status status) {
    if (!self || self->reset_count_ != req->reset_count)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    DCHECK(self->active_config_);

    if (!status.is_ok()) {
      self->HandleError(self->logger_->MakeException(
          "Encoder initialization error.", status));
    }

    self->stall_request_processing_ = false;
    self->ProcessRequests();
  };

  media_encoder_->Initialize(
      active_config_->profile, active_config_->options, std::move(output_cb),
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          done_callback, WrapCrossThreadWeakPersistent(this),
          WrapCrossThreadPersistent(request))));
}

std::unique_ptr<media::VideoEncoder> VideoEncoder::CreateMediaVideoEncoder(
    const ParsedConfig& config,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  // TODO(https://crbug.com/1119636): Implement / call a proper method for
  // detecting support of encoder configs.
  switch (config.hw_pref) {
    case HardwarePreference::kRequire: {
      auto result = CreateAcceleratedVideoEncoder(
          config.profile, config.options, gpu_factories);
      if (result)
        UpdateEncoderLog("AcceleratedVideoEncoder", true);
      return result;
    }
    case HardwarePreference::kAllow:
      if (auto result = CreateAcceleratedVideoEncoder(
              config.profile, config.options, gpu_factories)) {
        UpdateEncoderLog("AcceleratedVideoEncoder", true);
        return result;
      }
      FALLTHROUGH;
    case HardwarePreference::kDeny: {
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
  // codec implementation and can be changed on the fly.
  return original_config.codec == new_config.codec &&
         original_config.profile == new_config.profile &&
         original_config.level == new_config.level &&
         original_config.color_space == new_config.color_space &&
         original_config.hw_pref == new_config.hw_pref;
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
      self->HandleError(
          self->logger_->MakeException("Encoding error.", status));
    }
    self->ProcessRequests();
  };

  scoped_refptr<media::VideoFrame> frame = request->frame->frame();

  // Currently underlying encoders can't handle frame backed by textures,
  // so let's readback pixel data to CPU memory.
  if (frame->HasTextures() && !frame->HasGpuMemoryBuffer()) {
    scoped_refptr<viz::RasterContextProvider> raster_provider;
    auto wrapper = SharedGpuContext::ContextProviderWrapper();
    if (wrapper && wrapper->ContextProvider())
      raster_provider = wrapper->ContextProvider()->RasterContextProvider();
    if (raster_provider) {
      auto* ri = raster_provider->RasterInterface();
      auto* gr_context = raster_provider->GrContext();

      frame = ReadbackTextureBackedFrameToMemorySync(*frame, ri, gr_context,
                                                     &readback_frame_pool_);
    } else {
      frame.reset();
    }

    if (!frame) {
      auto status = media::Status(media::StatusCode::kEncoderFailedEncode,
                                  "Can't readback frame textures.");
      auto task_runner = Thread::Current()->GetTaskRunner();
      task_runner->PostTask(
          FROM_HERE,
          ConvertToBaseOnceCallback(CrossThreadBindOnce(
              done_callback, WrapCrossThreadWeakPersistent(this),
              WrapCrossThreadPersistent(request), std::move(status))));
      return;
    }
  }

  // Currently underlying encoders can't handle alpha channel, so let's
  // wrap a frame with an alpha channel into a frame without it.
  // For example such frames can come from 2D canvas context with alpha = true.
  if (frame->storage_type() == media::VideoFrame::STORAGE_OWNED_MEMORY &&
      frame->format() == media::PIXEL_FORMAT_I420A) {
    frame = media::WrapAsI420VideoFrame(std::move(frame));
  }

  bool keyframe = request->encodeOpts->hasKeyFrameNonNull() &&
                  request->encodeOpts->keyFrameNonNull();
  --requested_encodes_;
  media_encoder_->Encode(frame, keyframe,
                         ConvertToBaseOnceCallback(CrossThreadBindOnce(
                             done_callback, WrapCrossThreadWeakPersistent(this),
                             WrapCrossThreadPersistent(request))));

  // We passed a copy of frame() above, so this should be safe to close here.
  request->frame->close();
}

void VideoEncoder::OnReceivedGpuFactories(
    Request* request,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!gpu_factories || !gpu_factories->IsGpuVideoAcceleratorEnabled()) {
    CreateAndInitializeEncoderWithoutAcceleration(request);
    return;
  }

  // Delay create the hw encoder until HW encoder support is known, so that
  // GetVideoEncodeAcceleratorSupportedProfiles() can give a reliable answer.
  auto on_encoder_support_known_cb =
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          &VideoEncoder::CreateAndInitializeEncoderOnEncoderSupportKnown,
          WrapCrossThreadWeakPersistent(this),
          WrapCrossThreadPersistent(request),
          CrossThreadUnretained(gpu_factories)));
  gpu_factories->NotifyEncoderSupportKnown(
      std::move(on_encoder_support_known_cb));
}

void VideoEncoder::ProcessConfigure(Request* request) {
  DCHECK_NE(state_.AsEnum(), V8CodecState::Enum::kClosed);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK(active_config_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  stall_request_processing_ = true;

  if (active_config_->hw_pref == HardwarePreference::kDeny) {
    CreateAndInitializeEncoderWithoutAcceleration(request);
    return;
  }

  if (IsMainThread()) {
    OnReceivedGpuFactories(request, Platform::Current()->GetGpuFactories());
    return;
  }

  auto on_gpu_factories_cb = CrossThreadBindOnce(
      &VideoEncoder::OnReceivedGpuFactories,
      WrapCrossThreadWeakPersistent(this), WrapCrossThreadPersistent(request));

  Thread::MainThread()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      ConvertToBaseOnceCallback(
          CrossThreadBindOnce(&GetGpuFactoriesOnMainThread)),
      ConvertToBaseOnceCallback(std::move(on_gpu_factories_cb)));
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
      self->HandleError(self->logger_->MakeException(
          "Encoder initialization error.", status));
      self->stall_request_processing_ = false;
      return;
    }

    auto output_cb =
        ConvertToBaseRepeatingCallback(WTF::CrossThreadBindRepeating(
            &VideoEncoder::CallOutputCallback,
            WrapCrossThreadWeakPersistent(self),
            // We can't use |active_config_| from |this| because it can change
            // by the time the callback is executed.
            WrapCrossThreadPersistent(self->active_config_.Get()),
            self->reset_count_));

    self->first_output_after_configure_ = true;
    self->media_encoder_->ChangeOptions(
        self->active_config_->options, std::move(output_cb),
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            reconf_callback, WrapCrossThreadWeakPersistent(self),
            WrapCrossThreadPersistent(req))));
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
    DCHECK(req);
    DCHECK(req->resolver);
    if (self->reset_count_ != req->reset_count) {
      req->resolver.Release()->Reject();
      return;
    }
    if (status.is_ok()) {
      req->resolver.Release()->Resolve();
    } else {
      self->HandleError(
          self->logger_->MakeException("Flushing error.", status));
      req->resolver.Release()->Reject();
    }
    self->stall_request_processing_ = false;
    self->ProcessRequests();
  };

  stall_request_processing_ = true;
  media_encoder_->Flush(ConvertToBaseOnceCallback(
      CrossThreadBindOnce(done_callback, WrapCrossThreadWeakPersistent(this),
                          WrapCrossThreadPersistent(request))));
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

  auto deleter = [](void* data, size_t length, void*) {
    delete[] static_cast<uint8_t*>(data);
  };
  ArrayBufferContents data(output.data.release(), output.size, deleter);
  auto* dom_array = MakeGarbageCollected<DOMArrayBuffer>(std::move(data));
  auto* chunk = MakeGarbageCollected<EncodedVideoChunk>(
      output.timestamp, output.key_frame, dom_array);

  auto* metadata = MakeGarbageCollected<EncodedVideoChunkMetadata>();
  metadata->setTemporalLayerId(output.temporal_id);

  if (first_output_after_configure_ || codec_desc.has_value()) {
    first_output_after_configure_ = false;
    auto* decoder_config = MakeGarbageCollected<VideoDecoderConfig>();
    decoder_config->setCodec(active_config->codec_string);
    decoder_config->setCodedHeight(active_config->options.frame_size.height());
    decoder_config->setCodedWidth(active_config->options.frame_size.width());
    if (codec_desc.has_value()) {
      auto* desc_array_buf = DOMArrayBuffer::Create(codec_desc.value().data(),
                                                    codec_desc.value().size());
      decoder_config->setDescription(
          ArrayBufferOrArrayBufferView::FromArrayBuffer(desc_array_buf));
    }
    metadata->setDecoderConfig(decoder_config);
  }

  ScriptState::Scope scope(script_state_);
  output_callback_->InvokeAndReportException(nullptr, chunk, metadata);
}

}  // namespace blink
