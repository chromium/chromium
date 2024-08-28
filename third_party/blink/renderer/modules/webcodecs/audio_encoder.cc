// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"

#include <cinttypes>
#include <limits>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/audio/audio_opus_encoder.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "media/base/mime_util.h"
#include "media/base/offloading_audio_encoder.h"
#include "media/mojo/clients/mojo_audio_encoder.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_aac_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_support.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_opus_application.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_opus_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_opus_signal.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/array_buffer_util.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

constexpr const char kCategory[] = "media";

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
constexpr uint32_t kDefaultOpusComplexity = 5;
#else
constexpr uint32_t kDefaultOpusComplexity = 9;
#endif

template <typename T>
bool VerifyParameterValues(const T& value,
                           String error_message_base_base,
                           WTF::Vector<T> supported_values,
                           String* js_error_message) {
  if (base::Contains(supported_values, value)) {
    return true;
  }

  WTF::StringBuilder error_builder;
  error_builder.Append(error_message_base_base);
  error_builder.Append(" Supported values: ");
  for (auto i = 0u; i < supported_values.size(); i++) {
    if (i != 0) {
      error_builder.Append(", ");
    }
    error_builder.AppendNumber(supported_values[i]);
  }
  *js_error_message = error_builder.ToString();
  return false;
}

AudioEncoderTraits::ParsedConfig* ParseAacConfigStatic(
    const AacEncoderConfig* aac_config,
    AudioEncoderTraits::ParsedConfig* result,
    ExceptionState& exception_state) {
  result->options.aac = media::AudioEncoder::AacOptions();
  switch (aac_config->format().AsEnum()) {
    case V8AacBitstreamFormat::Enum::kAac:
      result->options.aac->format = media::AudioEncoder::AacOutputFormat::AAC;
      return result;
    case V8AacBitstreamFormat::Enum::kAdts:
      result->options.aac->format = media::AudioEncoder::AacOutputFormat::ADTS;
      return result;
  }
  return result;
}

AudioEncoderTraits::ParsedConfig* ParseOpusConfigStatic(
    const OpusEncoderConfig* opus_config,
    AudioEncoderTraits::ParsedConfig* result,
    ExceptionState& exception_state) {
  constexpr uint32_t kComplexityUpperBound = 10;
  uint32_t complexity = opus_config->getComplexityOr(kDefaultOpusComplexity);
  if (complexity > kComplexityUpperBound) {
    exception_state.ThrowTypeError(
        ExceptionMessages::IndexExceedsMaximumBound<uint32_t>(
            "Opus complexity", complexity, kComplexityUpperBound));
    return nullptr;
  }

  constexpr uint32_t kPacketLossPercUpperBound = 100;
  uint32_t packet_loss_perc = opus_config->packetlossperc();
  if (packet_loss_perc > kPacketLossPercUpperBound) {
    exception_state.ThrowTypeError(
        ExceptionMessages::IndexExceedsMaximumBound<uint32_t>(
            "Opus packetlossperc", packet_loss_perc,
            kPacketLossPercUpperBound));
    return nullptr;
  }

  // `frame_duration` must be a valid frame duration, defined in section 2.1.4.
  // of RFC6716.
  constexpr base::TimeDelta kFrameDurationLowerBound = base::Microseconds(2500);
  constexpr base::TimeDelta kFrameDurationUpperBound = base::Milliseconds(120);
  uint64_t frame_duration = opus_config->frameDuration();
  if (frame_duration < kFrameDurationLowerBound.InMicroseconds() ||
      frame_duration > kFrameDurationUpperBound.InMicroseconds()) {
    exception_state.ThrowTypeError(
        ExceptionMessages::IndexOutsideRange<uint64_t>(
            "Opus frameDuration", frame_duration,
            kFrameDurationLowerBound.InMicroseconds(),
            ExceptionMessages::BoundType::kInclusiveBound,
            kFrameDurationUpperBound.InMicroseconds(),
            ExceptionMessages::BoundType::kInclusiveBound));
    return nullptr;
  }

  // Any multiple of a frame duration is allowed by RFC6716. Concretely, this
  // means any multiple of 2500 microseconds.
  if (frame_duration % kFrameDurationLowerBound.InMicroseconds() != 0) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid Opus frameDuration; expected a multiple of %" PRIu64
        ", received %" PRIu64 ".",
        kFrameDurationLowerBound.InMicroseconds(), frame_duration));
    return nullptr;
  }

  if (opus_config->format().AsEnum() == V8OpusBitstreamFormat::Enum::kOgg) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Opus Ogg format is unsupported");
    return nullptr;
  }

  media::AudioEncoder::OpusSignal opus_signal;
  switch (opus_config->signal().AsEnum()) {
    case blink::V8OpusSignal::Enum::kAuto:
      opus_signal = media::AudioEncoder::OpusSignal::kAuto;
      break;
    case blink::V8OpusSignal::Enum::kMusic:
      opus_signal = media::AudioEncoder::OpusSignal::kMusic;
      break;
    case blink::V8OpusSignal::Enum::kVoice:
      opus_signal = media::AudioEncoder::OpusSignal::kVoice;
      break;
  }

  media::AudioEncoder::OpusApplication opus_application;
  switch (opus_config->application().AsEnum()) {
    case blink::V8OpusApplication::Enum::kVoip:
      opus_application = media::AudioEncoder::OpusApplication::kVoip;
      break;
    case blink::V8OpusApplication::Enum::kAudio:
      opus_application = media::AudioEncoder::OpusApplication::kAudio;
      break;
    case blink::V8OpusApplication::Enum::kLowdelay:
      opus_application = media::AudioEncoder::OpusApplication::kLowDelay;
      break;
  }

  result->options.opus = {
      .frame_duration = base::Microseconds(frame_duration),
      .signal = opus_signal,
      .application = opus_application,
      .complexity = complexity,
      .packet_loss_perc = packet_loss_perc,
      .use_in_band_fec = opus_config->useinbandfec(),
      .use_dtx = opus_config->usedtx(),
  };

  return result;
}

AudioEncoderTraits::ParsedConfig* ParseConfigStatic(
    const AudioEncoderConfig* config,
    ExceptionState& exception_state) {
  if (!config) {
    exception_state.ThrowTypeError("No config provided");
    return nullptr;
  }

  if (config->codec().LengthWithStrippedWhiteSpace() == 0) {
    exception_state.ThrowTypeError("Invalid codec; codec is required.");
    return nullptr;
  }

  auto* result = MakeGarbageCollected<AudioEncoderTraits::ParsedConfig>();

  result->options.codec = media::AudioCodec::kUnknown;
  bool is_codec_ambiguous = true;
  bool parse_succeeded = ParseAudioCodecString(
      "", config->codec().Utf8(), &is_codec_ambiguous, &result->options.codec);

  if (!parse_succeeded || is_codec_ambiguous) {
    result->options.codec = media::AudioCodec::kUnknown;
    return result;
  }

  result->options.channels = config->numberOfChannels();
  if (result->options.channels == 0) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid channel count; channel count must be non-zero, received %d.",
        result->options.channels));
    return nullptr;
  }

  result->options.sample_rate = config->sampleRate();
  if (result->options.sample_rate == 0) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid sample rate; sample rate must be non-zero, received %d.",
        result->options.sample_rate));
    return nullptr;
  }

  result->codec_string = config->codec();
  if (config->hasBitrate()) {
    if (config->bitrate() > std::numeric_limits<int>::max()) {
      exception_state.ThrowTypeError(String::Format(
          "Bitrate is too large; expected at most %d, received %" PRIu64,
          std::numeric_limits<int>::max(), config->bitrate()));
      return nullptr;
    }
    result->options.bitrate = static_cast<int>(config->bitrate());
  }

  if (config->hasBitrateMode()) {
    result->options.bitrate_mode =
        config->bitrateMode().AsEnum() == V8BitrateMode::Enum::kConstant
            ? media::AudioEncoder::BitrateMode::kConstant
            : media::AudioEncoder::BitrateMode::kVariable;
  }

  switch (result->options.codec) {
    case media::AudioCodec::kOpus:
      return ParseOpusConfigStatic(
          config->hasOpus() ? config->opus() : OpusEncoderConfig::Create(),
          result, exception_state);
    case media::AudioCodec::kAAC: {
      auto* aac_config =
          config->hasAac() ? config->aac() : AacEncoderConfig::Create();
      return ParseAacConfigStatic(aac_config, result, exception_state);
    }
    default:
      return result;
  }
}

bool VerifyCodecSupportStatic(AudioEncoderTraits::ParsedConfig* config,
                              String* js_error_message) {
  if (config->options.channels < 1 ||
      config->options.channels > media::limits::kMaxChannels) {
    *js_error_message = String::Format(
        "Unsupported channel count; expected range from %d to "
        "%d, received %d.",
        1, media::limits::kMaxChannels, config->options.channels);
    return false;
  }

  if (config->options.sample_rate < media::limits::kMinSampleRate ||
      config->options.sample_rate > media::limits::kMaxSampleRate) {
    *js_error_message = String::Format(
        "Unsupported sample rate; expected range from %d to %d, "
        "received %d.",
        media::limits::kMinSampleRate, media::limits::kMaxSampleRate,
        config->options.sample_rate);
    return false;
  }

  switch (config->options.codec) {
    case media::AudioCodec::kOpus: {
      // TODO(crbug.com/1378399): Support all multiples of basic frame
      // durations.
      if (!VerifyParameterValues(
              config->options.opus->frame_duration.InMicroseconds(),
              "Unsupported Opus frameDuration.",
              {2500, 5000, 10000, 20000, 40000, 60000}, js_error_message)) {
        return false;
      }
      if (config->options.channels > 2) {
        // Our Opus implementation only supports up to 2 channels
        *js_error_message = String::Format(
            "Too many channels for Opus encoder; "
            "expected at most 2, received %d.",
            config->options.channels);
        return false;
      }
      if (config->options.bitrate.has_value() &&
          config->options.bitrate.value() <
              media::AudioOpusEncoder::kMinBitrate) {
        *js_error_message = String::Format(
            "Opus bitrate is too low; expected at least %d, received %d.",
            media::AudioOpusEncoder::kMinBitrate,
            config->options.bitrate.value());
        return false;
      }
      return true;
    }
    case media::AudioCodec::kAAC: {
      if (media::MojoAudioEncoder::IsSupported(media::AudioCodec::kAAC)) {
        if (!VerifyParameterValues(config->options.channels,
                                   "Unsupported number of channels.", {1, 2, 6},
                                   js_error_message)) {
          return false;
        }
        if (config->options.bitrate.has_value()) {
          if (!VerifyParameterValues(
                  config->options.bitrate.value(), "Unsupported bitrate.",
                  {96000, 128000, 160000, 192000}, js_error_message)) {
            return false;
          }
        }
        if (!VerifyParameterValues(config->options.sample_rate,
                                   "Unsupported sample rate.", {44100, 48000},
                                   js_error_message)) {
          return false;
        }
        return true;
      }
      [[fallthrough]];
    }
    default:
      *js_error_message = "Unsupported codec type.";
      return false;
  }
}

AacEncoderConfig* CopyAacConfig(const AacEncoderConfig& config) {
  auto* result = AacEncoderConfig::Create();
  result->setFormat(config.format());
  return result;
}

OpusEncoderConfig* CopyOpusConfig(const OpusEncoderConfig& config) {
  auto* opus_result = OpusEncoderConfig::Create();
  opus_result->setFormat(config.format());
  opus_result->setSignal(config.signal());
  opus_result->setApplication(config.application());
  opus_result->setFrameDuration(config.frameDuration());
  opus_result->setComplexity(config.getComplexityOr(kDefaultOpusComplexity));
  opus_result->setPacketlossperc(config.packetlossperc());
  opus_result->setUseinbandfec(config.useinbandfec());
  opus_result->setUsedtx(config.usedtx());
  return opus_result;
}

AudioEncoderConfig* CopyConfig(const AudioEncoderConfig& config) {
  auto* result = AudioEncoderConfig::Create();
  result->setCodec(config.codec());
  result->setSampleRate(config.sampleRate());
  result->setNumberOfChannels(config.numberOfChannels());
  if (config.hasBitrate())
    result->setBitrate(config.bitrate());

  if (config.hasBitrateMode()) {
    result->setBitrateMode(config.bitrateMode());
  }

  if (config.hasOpus()) {
    result->setOpus(CopyOpusConfig(*config.opus()));
  }
  if (config.hasAac()) {
    result->setAac(CopyAacConfig(*config.aac()));
  }

  return result;
}

std::unique_ptr<media::AudioEncoder> CreateSoftwareAudioEncoder(
    media::AudioCodec codec) {
  if (codec != media::AudioCodec::kOpus)
    return nullptr;
  auto software_encoder = std::make_unique<media::AudioOpusEncoder>();
  return std::make_unique<media::OffloadingAudioEncoder>(
      std::move(software_encoder));
}

std::unique_ptr<media::AudioEncoder> CreatePlatformAudioEncoder(
    media::AudioCodec codec) {
  if (codec != media::AudioCodec::kAAC)
    return nullptr;

  mojo::PendingRemote<media::mojom::InterfaceFactory> pending_interface_factory;
  mojo::Remote<media::mojom::InterfaceFactory> interface_factory;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      pending_interface_factory.InitWithNewPipeAndPassReceiver());
  interface_factory.Bind(std::move(pending_interface_factory));

  mojo::PendingRemote<media::mojom::AudioEncoder> encoder_remote;
  interface_factory->CreateAudioEncoder(
      encoder_remote.InitWithNewPipeAndPassReceiver());
  return std::make_unique<media::MojoAudioEncoder>(std::move(encoder_remote));
}

}  // namespace

// static
const char* AudioEncoderTraits::GetName() {
  return "AudioEncoder";
}

AudioEncoder* AudioEncoder::Create(ScriptState* script_state,
                                   const AudioEncoderInit* init,
                                   ExceptionState& exception_state) {
  auto* result =
      MakeGarbageCollected<AudioEncoder>(script_state, init, exception_state);
  return exception_state.HadException() ? nullptr : result;
}

AudioEncoder::AudioEncoder(ScriptState* script_state,
                           const AudioEncoderInit* init,
                           ExceptionState& exception_state)
    : Base(script_state, init, exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

AudioEncoder::~AudioEncoder() = default;

std::unique_ptr<media::AudioEncoder> AudioEncoder::CreateMediaAudioEncoder(
    const ParsedConfig& config) {
  if (auto result = CreatePlatformAudioEncoder(config.options.codec)) {
    is_platform_encoder_ = true;
    return result;
  }
  is_platform_encoder_ = false;
  return CreateSoftwareAudioEncoder(config.options.codec);
}

void AudioEncoder::ProcessConfigure(Request* request) {
  DCHECK_NE(state_.AsEnum(), V8CodecState::Enum::kClosed);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK(request->config);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  request->StartTracing();

  active_config_ = request->config;
  String js_error_message;
  if (!VerifyCodecSupport(active_config_, &js_error_message)) {
    blocking_request_in_progress_ = request;
    QueueHandleError(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, js_error_message));
    request->EndTracing();
    return;
  }

  media_encoder_ = CreateMediaAudioEncoder(*active_config_);
  if (!media_encoder_) {
    blocking_request_in_progress_ = request;
    QueueHandleError(MakeOperationError(
        "Encoder creation error.",
        media::EncoderStatus(
            media::EncoderStatus::Codes::kEncoderInitializationError,
            "Unable to create encoder (most likely unsupported "
            "codec/acceleration requirement combination)")));
    request->EndTracing();
    return;
  }

  auto output_cb = ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
      &AudioEncoder::CallOutputCallback,
      MakeUnwrappingCrossThreadWeakHandle(this),
      // We can't use |active_config_| from |this| because it can change by
      // the time the callback is executed.
      MakeUnwrappingCrossThreadHandle(active_config_.Get()), reset_count_));

  auto done_callback = [](AudioEncoder* self, media::AudioCodec codec,
                          Request* req, media::EncoderStatus status) {
    if (!self || self->reset_count_ != req->reset_count) {
      req->EndTracing(/*aborted=*/true);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      self->HandleError(
          self->MakeOperationError("Encoding error.", std::move(status)));
    } else {
      base::UmaHistogramEnumeration("Blink.WebCodecs.AudioEncoder.Codec",
                                    codec);
    }

    req->EndTracing();
    self->blocking_request_in_progress_ = nullptr;
    self->ProcessRequests();
  };

  blocking_request_in_progress_ = request;
  first_output_after_configure_ = true;
  media_encoder_->Initialize(
      active_config_->options, std::move(output_cb),
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          done_callback, MakeUnwrappingCrossThreadWeakHandle(this),
          active_config_->options.codec,
          MakeUnwrappingCrossThreadHandle(request))));
}

void AudioEncoder::ProcessEncode(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kEncode);
  DCHECK_GT(requested_encodes_, 0u);

  request->StartTracing();

  auto* audio_data = request->input.Release();

  auto data = audio_data->data();

  // The data shouldn't be closed at this point.
  DCHECK(data);

  auto done_callback = [](AudioEncoder* self, Request* req,
                          media::EncoderStatus status) {
    if (!self || self->reset_count_ != req->reset_count) {
      req->EndTracing(/*aborted=*/true);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      self->HandleError(
          self->MakeEncodingError("Encoding error.", std::move(status)));
    }

    req->EndTracing();
    self->ProcessRequests();
  };

  if (data->channel_count() != active_config_->options.channels ||
      data->sample_rate() != active_config_->options.sample_rate) {
    // Per spec we must queue a task for error handling.
    QueueHandleError(MakeEncodingError(
        "Input audio buffer is incompatible with codec parameters",
        media::EncoderStatus(media::EncoderStatus::Codes::kEncoderFailedEncode)
            .WithData("channels", data->channel_count())
            .WithData("sampleRate", data->sample_rate())));

    request->EndTracing();

    audio_data->close();
    return;
  }

  // If |data|'s memory layout allows it, |audio_bus| will be a simple wrapper
  // around it. Otherwise, |audio_bus| will contain a converted copy of |data|.
  auto audio_bus = media::AudioBuffer::WrapOrCopyToAudioBus(data);

  base::TimeTicks timestamp = base::TimeTicks() + data->timestamp();

  --requested_encodes_;
  ScheduleDequeueEvent();
  media_encoder_->Encode(
      std::move(audio_bus), timestamp,
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          done_callback, MakeUnwrappingCrossThreadWeakHandle(this),
          MakeUnwrappingCrossThreadHandle(request))));

  audio_data->close();
}

void AudioEncoder::ProcessReconfigure(Request* request) {
  // Audio decoders don't currently support any meaningful reconfiguring
}

AudioEncoder::ParsedConfig* AudioEncoder::ParseConfig(
    const AudioEncoderConfig* opts,
    ExceptionState& exception_state) {
  return ParseConfigStatic(opts, exception_state);
}

bool AudioEncoder::CanReconfigure(ParsedConfig& original_config,
                                  ParsedConfig& new_config) {
  return original_config.options.codec == new_config.options.codec &&
         original_config.options.channels == new_config.options.channels &&
         original_config.options.bitrate == new_config.options.bitrate &&
         original_config.options.sample_rate == new_config.options.sample_rate;
}

bool AudioEncoder::VerifyCodecSupport(ParsedConfig* config,
                                      String* js_error_message) {
  return VerifyCodecSupportStatic(config, js_error_message);
}

void AudioEncoder::CallOutputCallback(
    ParsedConfig* active_config,
    uint32_t reset_count,
    media::EncodedAudioBuffer encoded_buffer,
    std::optional<media::AudioEncoder::CodecDescription> codec_desc) {
  DCHECK(active_config);
  if (!script_state_->ContextIsValid() || !output_callback_ ||
      state_.AsEnum() != V8CodecState::Enum::kConfigured ||
      reset_count != reset_count_) {
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MarkCodecActive();

  auto buffer =
      media::DecoderBuffer::FromArray(std::move(encoded_buffer.encoded_data));
  buffer->set_timestamp(encoded_buffer.timestamp - base::TimeTicks());
  buffer->set_is_key_frame(true);
  buffer->set_duration(encoded_buffer.duration);
  auto* chunk = MakeGarbageCollected<EncodedAudioChunk>(std::move(buffer));

  auto* metadata = MakeGarbageCollected<EncodedAudioChunkMetadata>();
  if (first_output_after_configure_ || codec_desc.has_value()) {
    first_output_after_configure_ = false;
    auto* decoder_config = MakeGarbageCollected<AudioDecoderConfig>();
    decoder_config->setCodec(active_config->codec_string);
    decoder_config->setSampleRate(encoded_buffer.params.sample_rate());
    decoder_config->setNumberOfChannels(active_config->options.channels);
    if (codec_desc.has_value()) {
      auto* desc_array_buf = DOMArrayBuffer::Create(codec_desc.value());
      decoder_config->setDescription(
          MakeGarbageCollected<AllowSharedBufferSource>(desc_array_buf));
    }
    metadata->setDecoderConfig(decoder_config);
  }

  TRACE_EVENT_BEGIN1(kCategory, GetTraceNames()->output.c_str(), "timestamp",
                     chunk->timestamp());

  ScriptState::Scope scope(script_state_);
  output_callback_->InvokeAndReportException(nullptr, chunk, metadata);

  TRACE_EVENT_END0(kCategory, GetTraceNames()->output.c_str());
}

// static
ScriptPromise<AudioEncoderSupport> AudioEncoder::isConfigSupported(
    ScriptState* script_state,
    const AudioEncoderConfig* config,
    ExceptionState& exception_state) {
  auto* parsed_config = ParseConfigStatic(config, exception_state);
  if (!parsed_config) {
    DCHECK(exception_state.HadException());
    return EmptyPromise();
  }

  String unused_js_error_message;
  auto* support = AudioEncoderSupport::Create();
  support->setSupported(
      VerifyCodecSupportStatic(parsed_config, &unused_js_error_message));
  support->setConfig(CopyConfig(*config));
  return ToResolvedPromise<AudioEncoderSupport>(script_state, support);
}

const AtomicString& AudioEncoder::InterfaceName() const {
  return event_target_names::kAudioEncoder;
}

DOMException* AudioEncoder::MakeOperationError(std::string error_msg,
                                               media::EncoderStatus status) {
  if (is_platform_encoder_) {
    return logger_->MakeOperationError(std::move(error_msg), std::move(status));
  }
  return logger_->MakeSoftwareCodecOperationError(std::move(error_msg),
                                                  std::move(status));
}

DOMException* AudioEncoder::MakeEncodingError(std::string error_msg,
                                              media::EncoderStatus status) {
  if (is_platform_encoder_) {
    return logger_->MakeEncodingError(std::move(error_msg), std::move(status));
  }
  return logger_->MakeSoftwareCodecEncodingError(std::move(error_msg),
                                                 std::move(status));
}

}  // namespace blink
