// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/media_source.h"

#include <memory>
#include <tuple>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "build/chromeos_buildflags.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/logging_override_if_enabled.h"
#include "media/base/media_switches.h"
#include "media/base/media_types.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/video_decoder_config.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/platform/web_media_source.h"
#include "third_party/blink/public/platform/web_source_buffer.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_end_of_stream_error.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_source_buffer_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/html/track/video_track_list.h"
#include "third_party/blink/renderer/modules/mediasource/attachment_creation_pass_key_provider.h"
#include "third_party/blink/renderer/modules/mediasource/cross_thread_media_source_attachment.h"
#include "third_party/blink/renderer/modules/mediasource/handle_attachment_provider.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_handle_impl.h"
#include "third_party/blink/renderer/modules/mediasource/same_thread_media_source_attachment.h"
#include "third_party/blink/renderer/modules/mediasource/same_thread_media_source_tracer.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer_track_base_supplement.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using blink::WebMediaSource;
using blink::WebSourceBuffer;

namespace blink {

namespace {

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)

bool IsMp2tCodecSupported(std::string_view codec_id) {
  if (auto result =
          media::ParseVideoCodecString("", codec_id,
                                       /*allow_ambiguous_matches=*/false)) {
    if (result->codec != media::VideoCodec::kH264) {
      return false;
    }
    return true;
  }

  auto audio_codec = media::AudioCodec::kUnknown;
  bool is_codec_ambiguous = false;
  if (media::ParseAudioCodecString("", codec_id, &is_codec_ambiguous,
                                   &audio_codec)) {
    if (is_codec_ambiguous) {
      return false;
    }

    if (audio_codec != media::AudioCodec::kAAC &&
        audio_codec != media::AudioCodec::kMP3) {
      return false;
    }
    return true;
  }

  return false;
}

#endif  // BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)

}  // namespace

static AtomicString ReadyStateToString(MediaSource::ReadyState state) {
  AtomicString result;
  switch (state) {
    case MediaSource::ReadyState::kOpen:
      result = AtomicString("open");
      break;
    case MediaSource::ReadyState::kClosed:
      result = AtomicString("closed");
      break;
    case MediaSource::ReadyState::kEnded:
      result = AtomicString("ended");
      break;
  }

  return result;
}

static bool ThrowExceptionIfClosed(bool is_open,
                                   ExceptionState& exception_state) {
  if (!is_open) {
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "The MediaSource's readyState is not 'open'.");
    return true;
  }

  return false;
}

static bool ThrowExceptionIfClosedOrUpdating(bool is_open,
                                             bool is_updating,
                                             ExceptionState& exception_state) {
  if (ThrowExceptionIfClosed(is_open, exception_state))
    return true;

  if (is_updating) {
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "The 'updating' attribute is true on one or more of this MediaSource's "
        "SourceBuffers.");
    return true;
  }

  return false;
}

MediaSource* MediaSource::Create(ExecutionContext* context) {
  return MakeGarbageCollected<MediaSource>(context);
}

MediaSource::MediaSource(ExecutionContext* context)
    : ActiveScriptWrappable<MediaSource>({}),
      ExecutionContextLifecycleObserver(context),
      ready_state_(ReadyState::kClosed),
      async_event_queue_(
          MakeGarbageCollected<EventQueue>(GetExecutionContext(),
                                           TaskType::kMediaElementEvent)),
      context_already_destroyed_(false),
      source_buffers_(
          MakeGarbageCollected<SourceBufferList>(GetExecutionContext(),
                                                 async_event_queue_.Get())),
      active_source_buffers_(
          MakeGarbageCollected<SourceBufferList>(GetExecutionContext(),
                                                 async_event_queue_.Get())),
      has_live_seekable_range_(false),
      live_seekable_range_start_(0.0),
      live_seekable_range_end_(0.0) {
  DVLOG(1) << __func__ << " this=" << this;
  if (!IsMainThread()) {
    DCHECK(GetExecutionContext()->IsDedicatedWorkerGlobalScope());
  }
}

MediaSource::~MediaSource() {
  DVLOG(1) << __func__ << " this=" << this;
}

void MediaSource::LogAndThrowDOMException(ExceptionState& exception_state,
                                          DOMExceptionCode error,
                                          const String& message) {
  DVLOG(1) << __func__ << " (error=" << ToExceptionCode(error)
           << ", message=" << message << ")";
  exception_state.ThrowDOMException(error, message);
}

void MediaSource::LogAndThrowTypeError(ExceptionState& exception_state,
                                       const String& message) {
  DVLOG(1) << __func__ << " (message=" << message << ")";
  exception_state.ThrowTypeError(message);
}

SourceBuffer* MediaSource::addSourceBuffer(const String& type,
                                           ExceptionState& exception_state) {
  DVLOG(2) << __func__ << " this=" << this << " type=" << type;

  // 2.2
  // https://www.w3.org/TR/media-source/#dom-mediasource-addsourcebuffer
  // 1. If type is an empty string then throw a TypeError exception
  //    and abort these steps.
  if (type.empty()) {
    LogAndThrowTypeError(exception_state, "The type provided is empty");
    return nullptr;
  }

  // 2. If type contains a MIME type that is not supported ..., then throw a
  // NotSupportedError exception and abort these steps.
  // TODO(crbug.com/535738): Actually relax codec-specificity.
  if (!IsTypeSupportedInternal(
          GetExecutionContext(), type,
          false /* Allow underspecified codecs in |type| */)) {
    LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kNotSupportedError,
        "The type provided ('" + type + "') is unsupported.");
    return nullptr;
  }

  // 4. If the readyState attribute is not in the "open" state then throw an
  // InvalidStateError exception and abort these steps.
  if (!IsOpen()) {
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "The MediaSource's readyState is not 'open'.");
    return nullptr;
  }

  // Do remainder of steps only if attachment is usable and underlying demuxer
  // is protected from destruction (applicable especially for MSE-in-Worker
  // case).
  SourceBuffer* source_buffer = nullptr;

  // Note, here we must be open, therefore we must have an attachment.
  if (!RunUnlessElementGoneOrClosingUs(WTF::BindOnce(
          &MediaSource::AddSourceBuffer_Locked, WrapPersistent(this), type,
          nullptr /* audio_config */, nullptr /* video_config */,
          WTF::Unretained(&exception_state),
          WTF::Unretained(&source_buffer)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "Worker MediaSource attachment is closing");
  }

  return source_buffer;
}

SourceBuffer* MediaSource::AddSourceBufferUsingConfig(
    ExecutionContext* execution_context,
    const SourceBufferConfig* config,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__ << " this=" << this;

  UseCounter::Count(execution_context,
                    WebFeature::kMediaSourceExtensionsForWebCodecs);

  DCHECK(config);

  // Precisely one of the multiple keys in SourceBufferConfig must be set.
  int num_set = 0;
  if (config->hasAudioConfig())
    num_set++;
  if (config->hasVideoConfig())
    num_set++;
  if (num_set != 1) {
    LogAndThrowTypeError(
        exception_state,
        "SourceBufferConfig must have precisely one media type");
    return nullptr;
  }

  // Determine if the config is valid and supported by creating the necessary
  // media decoder configs using WebCodecs converters. This implies that codecs
  // supported by WebCodecs are also supported by MSE, though MSE may require
  // more precise information in the encoded chunks (such as video chunk
  // duration).
  // TODO(crbug.com/1144908): WebCodecs' determination of decoder configuration
  // support may be changed to be async and thus might also motivate making this
  // method async.
  std::unique_ptr<media::AudioDecoderConfig> audio_config;
  std::unique_ptr<media::VideoDecoderConfig> video_config;
  String console_message;

  if (config->hasAudioConfig()) {
    if (!AudioDecoder::IsValidAudioDecoderConfig(*(config->audioConfig()),
                                                 &console_message /* out */)) {
      LogAndThrowTypeError(exception_state, console_message);
      return nullptr;
    }

    std::optional<media::AudioDecoderConfig> out_audio_config =
        AudioDecoder::MakeMediaAudioDecoderConfig(*(config->audioConfig()),
                                                  &console_message /* out */);

    if (out_audio_config) {
      audio_config =
          std::make_unique<media::AudioDecoderConfig>(*out_audio_config);
    } else {
      LogAndThrowDOMException(exception_state,
                              DOMExceptionCode::kNotSupportedError,
                              console_message);
      return nullptr;
    }
  } else {
    DCHECK(config->hasVideoConfig());
    if (!VideoDecoder::IsValidVideoDecoderConfig(*(config->videoConfig()),
                                                 &console_message /* out */)) {
      LogAndThrowTypeError(exception_state, console_message);
      return nullptr;
    }

    bool converter_needed = false;
    std::optional<media::VideoDecoderConfig> out_video_config =
        VideoDecoder::MakeMediaVideoDecoderConfig(*(config->videoConfig()),
                                                  &console_message /* out */,
                                                  &converter_needed /* out */);
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    // TODO(crbug.com/1144908): Initial prototype does not support h264
    // buffering. See above.
    if (out_video_config && converter_needed) {
      out_video_config = std::nullopt;
      console_message =
          "H.264/H.265 EncodedVideoChunk buffering is not yet supported in "
          "MSE.See https://crbug.com/1144908.";
    }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

    if (out_video_config) {
      video_config =
          std::make_unique<media::VideoDecoderConfig>(*out_video_config);
    } else {
      LogAndThrowDOMException(exception_state,
                              DOMExceptionCode::kNotSupportedError,
                              console_message);
      return nullptr;
    }
  }

  // If the readyState attribute is not in the "open" state then throw an
  // InvalidStateError exception and abort these steps.
  if (!IsOpen()) {
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "The MediaSource's readyState is not 'open'.");
    return nullptr;
  }

  // Do remainder of steps only if attachment is usable and underlying demuxer
  // is protected from destruction (applicable especially for MSE-in-Worker
  // case).
  SourceBuffer* source_buffer = nullptr;
  String null_type;

  // Note, here we must be open, therefore we must have an attachment.
  if (!RunUnlessElementGoneOrClosingUs(WTF::BindOnce(
          &MediaSource::AddSourceBuffer_Locked, WrapPersistent(this), null_type,
          std::move(audio_config), std::move(video_config),
          WTF::Unretained(&exception_state),
          WTF::Unretained(&source_buffer)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "Worker MediaSource attachment is closing");
  }

  return source_buffer;
}

void MediaSource::AddSourceBuffer_Locked(
    const String& type,
    std::unique_ptr<media::AudioDecoderConfig> audio_config,
    std::unique_ptr<media::VideoDecoderConfig> video_config,
    ExceptionState* exception_state,
    SourceBuffer** created_buffer,
    MediaSourceAttachmentSupplement::ExclusiveKey pass_key) {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // 5. Create a new SourceBuffer object and associated resources.
  // TODO(crbug.com/1144908): Plumb the configs through into a new logic in
  // WebSourceBuffer and SourceBufferState such that configs and encoded chunks
  // can be buffered, with appropriate invocations of the
  // InitializationSegmentReceived and AppendError methods.
  ContentType content_type(type);
  String codecs = content_type.Parameter("codecs");
  std::unique_ptr<WebSourceBuffer> web_source_buffer = CreateWebSourceBuffer(
      content_type.GetType(), codecs, std::move(audio_config),
      std::move(video_config), *exception_state);

  if (!web_source_buffer) {
    DCHECK(exception_state->CodeAs<DOMExceptionCode>() ==
               DOMExceptionCode::kNotSupportedError ||
           exception_state->CodeAs<DOMExceptionCode>() ==
               DOMExceptionCode::kQuotaExceededError);
    // 2. If type contains a MIME type that is not supported ..., then throw a
    //    NotSupportedError exception and abort these steps.
    // 3. If the user agent can't handle any more SourceBuffer objects then
    //    throw a QuotaExceededError exception and abort these steps
    *created_buffer = nullptr;
    return;
  }

  bool generate_timestamps_flag =
      web_source_buffer->GetGenerateTimestampsFlag();

  auto* buffer = MakeGarbageCollected<SourceBuffer>(
      std::move(web_source_buffer), this, async_event_queue_.Get());
  // 8. Add the new object to sourceBuffers and queue a simple task to fire a
  //    simple event named addsourcebuffer at sourceBuffers.
  source_buffers_->Add(buffer);

  // Steps 6 and 7 (Set the SourceBuffer's mode attribute based on the byte
  // stream format's generate timestamps flag). We do this after adding to
  // sourceBuffers (step 8) to enable direct reuse of the SetMode_Locked() logic
  // here, which depends on |buffer| being in |source_buffers_| in our
  // implementation.
  if (generate_timestamps_flag) {
    buffer->SetMode_Locked(V8AppendMode::Enum::kSequence, exception_state,
                           pass_key);
  } else {
    buffer->SetMode_Locked(V8AppendMode::Enum::kSegments, exception_state,
                           pass_key);
  }

  // 9. Return the new object to the caller.
  DVLOG(3) << __func__ << " this=" << this << " type=" << type << " -> "
           << buffer;
  *created_buffer = buffer;
  return;
}

void MediaSource::removeSourceBuffer(SourceBuffer* buffer,
                                     ExceptionState& exception_state) {
  DVLOG(2) << __func__ << " this=" << this << " buffer=" << buffer;

  // 2.2
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-removeSourceBuffer-void-SourceBuffer-sourceBuffer

  // 1. If sourceBuffer specifies an object that is not in sourceBuffers then
  //    throw a NotFoundError exception and abort these steps.
  if (!source_buffers_->length() || !source_buffers_->Contains(buffer)) {
    LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kNotFoundError,
        "The SourceBuffer provided is not contained in this MediaSource.");
    return;
  }

  // Do remainder of steps only if attachment is usable and underlying demuxer
  // is protected from destruction (applicable especially for MSE-in-Worker
  // case). Note, we must not be closed (since closing clears our SourceBuffer
  // collections), therefore we must have an attachment.
  if (!RunUnlessElementGoneOrClosingUs(
          WTF::BindOnce(&MediaSource::RemoveSourceBuffer_Locked,
                        WrapPersistent(this), WrapPersistent(buffer)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "Worker MediaSource attachment is closing");
  }
}

void MediaSource::RemoveSourceBuffer_Locked(
    SourceBuffer* buffer,
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // Steps 2-8 are implemented by SourceBuffer::removedFromMediaSource.
  buffer->RemovedFromMediaSource();

  // 9. If sourceBuffer is in activeSourceBuffers, then remove sourceBuffer from
  //    activeSourceBuffers ...
  active_source_buffers_->Remove(buffer);

  // 10. Remove sourceBuffer from sourceBuffers and fire a removesourcebuffer
  //     event on that object.
  source_buffers_->Remove(buffer);

  // 11. Destroy all resources for sourceBuffer.
  //     This should have been done already by
  //     SourceBuffer::removedFromMediaSource (steps 2-8) above.

  SendUpdatedInfoToMainThreadCache();
}

void MediaSource::OnReadyStateChange(const ReadyState old_state,
                                     const ReadyState new_state) {
  if (IsOpen()) {
    ScheduleEvent(event_type_names::kSourceopen);
    return;
  }

  if (old_state == ReadyState::kOpen && new_state == ReadyState::kEnded) {
    ScheduleEvent(event_type_names::kSourceended);
    return;
  }

  DCHECK(IsClosed());

  active_source_buffers_->Clear();

  // Clear SourceBuffer references to this object.
  for (unsigned i = 0; i < source_buffers_->length(); ++i)
    source_buffers_->item(i)->RemovedFromMediaSource();
  source_buffers_->Clear();

  {
    base::AutoLock lock(attachment_link_lock_);
    media_source_attachment_.reset();
    attachment_tracer_ = nullptr;
  }

  ScheduleEvent(event_type_names::kSourceclose);
}

bool MediaSource::IsUpdating() const {
  // Return true if any member of |m_sourceBuffers| is updating.
  for (unsigned i = 0; i < source_buffers_->length(); ++i) {
    if (source_buffers_->item(i)->updating())
      return true;
  }

  return false;
}

// static
bool MediaSource::isTypeSupported(ExecutionContext* context,
                                  const String& type) {
  bool result = IsTypeSupportedInternal(
      context, type, true /* Require fully specified mime and codecs */);
  DVLOG(2) << __func__ << "(" << type << ") -> " << (result ? "true" : "false");
  return result;
}

// static
bool MediaSource::IsTypeSupportedInternal(ExecutionContext* context,
                                          const String& type,
                                          bool enforce_codec_specificity) {
  // Even after ExecutionContext teardown notification, bindings may still call
  // code-behinds for a short while. If |context| is null, this is likely
  // happening. To prevent possible null deref of |context| in this path, claim
  // lack of support immediately without proceeding.
  if (!context) {
    DVLOG(1) << __func__ << "(" << type << ", "
             << (enforce_codec_specificity ? "true" : "false")
             << ") -> false (context is null)";
    return false;
  }

  // Section 2.2 isTypeSupported() method steps.
  // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-MediaSource-isTypeSupported-boolean-DOMString-type
  // 1. If type is an empty string, then return false.
  if (type.empty()) {
    DVLOG(1) << __func__ << "(" << type << ", "
             << (enforce_codec_specificity ? "true" : "false")
             << ") -> false (empty input)";
    return false;
  }

  // 2. If type does not contain a valid MIME type string, then return false.
  ContentType content_type(type);
  String mime_type = content_type.GetType();
  if (mime_type.empty()) {
    DVLOG(1) << __func__ << "(" << type << ", "
             << (enforce_codec_specificity ? "true" : "false")
             << ") -> false (invalid mime type)";
    return false;
  }

  String codecs = content_type.Parameter("codecs");
  ContentType filtered_content_type = content_type;

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
  // Mime util doesn't include the mp2t container in order to prevent codec
  // support leaking into HtmlMediaElement.canPlayType. If the stream parser
  // is enabled, we should check that the codecs are valid using the mp4
  // container, since it can support any of the codecs we support for mp2t.
  if (mime_type == "video/mp2t") {
    std::vector<std::string> parsed_codec_ids;
    media::SplitCodecs(codecs.Ascii(), &parsed_codec_ids);
    for (const auto& codec_id : parsed_codec_ids) {
      if (!IsMp2tCodecSupported(codec_id)) {
        return false;
      }
    }
    return true;
  }
#endif

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
  // When build flag ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION and feature
  // kPlatformEncryptedDolbyVision are both enabled, encrypted Dolby Vision is
  // allowed in Media Source while clear Dolby Vision is not allowed.
  // In this case:
  // - isTypeSupported(fully qualified type with DV codec) should say false on
  // such platform, but addSourceBuffer(same) and changeType(same) shouldn't
  // fail just due to having DV codec.
  // - We use `enforce_codec_specificity` to understand if we are servicing
  // isTypeSupported (if true) vs addSourceBuffer or changeType (if false). When
  // `enforce_codec_specificity` is false, we'll remove any detected DV codec
  // from the codecs in the `filtered_content_type`.
  // - When `kAllowClearDolbyVisionInMseWhenPlatformEncryptedDvEnabled` is
  // specified, allow DV regardless of `enforce_codec_specificity`.
  if (base::FeatureList::IsEnabled(media::kPlatformEncryptedDolbyVision) &&
      (base::FeatureList::IsEnabled(
           media::kAllowClearDolbyVisionInMseWhenPlatformEncryptedDvEnabled) ||
       !enforce_codec_specificity)) {
    // Remove any detected DolbyVision codec from the query to GetSupportsType.
    std::string filtered_codecs;
    std::vector<std::string> parsed_codec_ids;
    media::SplitCodecs(codecs.Ascii(), &parsed_codec_ids);
    bool first = true;
    for (const auto& codec_id : parsed_codec_ids) {
      if (auto result =
              media::ParseVideoCodecString(mime_type.Ascii(), codec_id,
                                           /*allow_ambiguous_matches=*/false)) {
        if (result->codec == media::VideoCodec::kDolbyVision) {
          continue;
        }
      }
      if (first)
        first = false;
      else
        filtered_codecs += ",";
      filtered_codecs += codec_id;
    }

    std::string filtered_type =
        mime_type.Ascii() + "; codecs=\"" + filtered_codecs + "\"";
    DVLOG(1) << __func__ << " filtered_type=" << filtered_type;
    filtered_content_type =
        ContentType(String::FromUTF8(filtered_type.c_str()));
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)

  // Note: MediaSource.isTypeSupported() returning true implies that
  // HTMLMediaElement.canPlayType() will return "maybe" or "probably" since it
  // does not make sense for a MediaSource to support a type the
  // HTMLMediaElement knows it cannot play.
  auto get_supports_type_result =
      HTMLMediaElement::GetSupportsType(filtered_content_type);
  if (get_supports_type_result == MIMETypeRegistry::kNotSupported) {
    DVLOG(1) << __func__ << "(" << type << ", "
             << (enforce_codec_specificity ? "true" : "false")
             << ") -> false (not supported by HTMLMediaElement)";
    RecordIdentifiabilityMetric(context, type, false);
    return false;
  }

  // 3. If type contains a media type or media subtype that the MediaSource does
  //    not support, then return false.
  // 4. If type contains at a codec that the MediaSource does not support, then
  //    return false.
  // 5. If the MediaSource does not support the specified combination of media
  //    type, media subtype, and codecs then return false.
  // 6. Return true.
  // For incompletely specified mime-type and codec combinations, we also return
  // false if |enforce_codec_specificity| is true, complying with the
  // non-normative guidance being incubated for the MSE v2 codec switching
  // feature at https://github.com/WICG/media-source/tree/codec-switching.
  // Relaxed codec specificity following similar non-normative guidance is
  // allowed for addSourceBuffer and changeType methods, but this strict codec
  // specificity is and will be retained for isTypeSupported.
  // TODO(crbug.com/535738): Actually relax the codec-specifity for aSB() and
  // cT() (which is when |enforce_codec_specificity| is false).
  MIMETypeRegistry::SupportsType supported =
      MIMETypeRegistry::SupportsMediaSourceMIMEType(mime_type, codecs);

  bool result = supported == MIMETypeRegistry::kSupported;

  DVLOG(2) << __func__ << "(" << type << ", "
           << (enforce_codec_specificity ? "true" : "false") << ") -> "
           << (result ? "true" : "false");
  RecordIdentifiabilityMetric(context, type, result);
  return result;
}

// static
bool MediaSource::canConstructInDedicatedWorker(ExecutionContext* context) {
  return true;
}

void MediaSource::RecordIdentifiabilityMetric(ExecutionContext* context,
                                              const String& type,
                                              bool result) {
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleType(
          blink::IdentifiableSurface::Type::kMediaSource_IsTypeSupported)) {
    return;
  }
  blink::IdentifiabilityMetricBuilder(context->UkmSourceID())
      .Add(blink::IdentifiableSurface::FromTypeAndToken(
               blink::IdentifiableSurface::Type::kMediaSource_IsTypeSupported,
               IdentifiabilityBenignStringToken(type)),
           result)
      .Record(context->UkmRecorder());
}

const AtomicString& MediaSource::InterfaceName() const {
  return event_target_names::kMediaSource;
}

ExecutionContext* MediaSource::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

// TODO(https://crbug.com/878133): Consider using macros or virtual methods to
// skip the Bind+Run of |cb| when on same-thread, and to instead just run the
// method directly.
bool MediaSource::RunUnlessElementGoneOrClosingUs(
    MediaSourceAttachmentSupplement::RunExclusivelyCB cb) {
  auto [attachment, tracer] = AttachmentAndTracer();
  DCHECK(IsMainThread() ||
         !tracer);  // Cross-thread attachments do not use a tracer.

  if (!attachment) {
    // Element's context destruction may be in flight.
    return false;
  }

  if (!attachment->RunExclusively(true /* abort if not fully attached */,
                                  std::move(cb))) {
    DVLOG(1) << __func__ << ": element is gone or is closing us.";
    // Only in cross-thread case might we not be attached fully.
    DCHECK(!IsMainThread());
    return false;
  }

  return true;
}

void MediaSource::AssertAttachmentsMutexHeldIfCrossThreadForDebugging() const {
#if DCHECK_IS_ON()
  base::AutoLock lock(attachment_link_lock_);
  DCHECK(media_source_attachment_);
  if (!IsMainThread()) {
    DCHECK(!attachment_tracer_);  // Cross-thread attachments use no tracer;
    media_source_attachment_->AssertCrossThreadMutexIsAcquiredForDebugging();
  }
#endif  // DCHECK_IS_ON()
}

void MediaSource::SendUpdatedInfoToMainThreadCache() {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();
  scoped_refptr<MediaSourceAttachmentSupplement> attachment;
  std::tie(attachment, std::ignore) = AttachmentAndTracer();
  attachment->SendUpdatedInfoToMainThreadCache();
}

void MediaSource::Trace(Visitor* visitor) const {
  visitor->Trace(async_event_queue_);

  // |attachment_tracer_| is only set when this object is owned by the main
  // thread and is possibly involved in a SameThreadMediaSourceAttachment.
  // Therefore, it is thread-safe to access it here without taking the
  // |attachment_link_lock_|.
  visitor->Trace(TS_UNCHECKED_READ(attachment_tracer_));

  visitor->Trace(worker_media_source_handle_);
  visitor->Trace(source_buffers_);
  visitor->Trace(active_source_buffers_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void MediaSource::CompleteAttachingToMediaElement(
    std::unique_ptr<WebMediaSource> web_media_source) {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  {
    base::AutoLock lock(attachment_link_lock_);

    DCHECK_EQ(!attachment_tracer_, !IsMainThread());

    if (attachment_tracer_) {
      // Use of a tracer means we must be using same-thread attachment.
      TRACE_EVENT_NESTABLE_ASYNC_END0(
          "media", "MediaSource::StartAttachingToMediaElement",
          TRACE_ID_LOCAL(this));
    } else {
      // Otherwise, we must be using a cross-thread MSE-in-Workers attachment.
      TRACE_EVENT_NESTABLE_ASYNC_END0(
          "media", "MediaSource::StartWorkerAttachingToMainThreadMediaElement",
          TRACE_ID_LOCAL(this));
    }
    DCHECK(web_media_source);
    DCHECK(!web_media_source_);
    DCHECK(media_source_attachment_);

    web_media_source_ = std::move(web_media_source);
  }

  SetReadyState(ReadyState::kOpen);
}

double MediaSource::GetDuration_Locked(
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) const {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  if (IsClosed()) {
    return std::numeric_limits<float>::quiet_NaN();
  }

  return web_media_source_->Duration();
}

WebTimeRanges MediaSource::BufferedInternal(
    MediaSourceAttachmentSupplement::ExclusiveKey pass_key) const {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // Implements MediaSource algorithm for HTMLMediaElement.buffered.
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#htmlmediaelement-extensions
  Vector<WebTimeRanges> ranges(active_source_buffers_->length());

  for (unsigned i = 0; i < active_source_buffers_->length(); ++i) {
    active_source_buffers_->item(i)->GetBuffered_Locked(&ranges[i], pass_key);
  }

  WebTimeRanges intersection_ranges;

  // 1. If activeSourceBuffers.length equals 0 then return an empty TimeRanges
  //    object and abort these steps.
  if (ranges.empty())
    return intersection_ranges;

  // 2. Let active ranges be the ranges returned by buffered for each
  //    SourceBuffer object in activeSourceBuffers.
  // 3. Let highest end time be the largest range end time in the active ranges.
  double highest_end_time = -1;
  for (const WebTimeRanges& source_ranges : ranges) {
    if (!source_ranges.empty())
      highest_end_time = std::max(highest_end_time, source_ranges.back().end);
  }

  // Return an empty range if all ranges are empty.
  if (highest_end_time < 0)
    return intersection_ranges;

  // 4. Let intersection ranges equal a TimeRange object containing a single
  //    range from 0 to highest end time.
  intersection_ranges.emplace_back(0, highest_end_time);

  // 5. For each SourceBuffer object in activeSourceBuffers run the following
  //    steps:
  bool ended = ready_state_ == ReadyState::kEnded;
  // 5.1 Let source ranges equal the ranges returned by the buffered attribute
  //     on the current SourceBuffer.
  for (WebTimeRanges& source_ranges : ranges) {
    // 5.2 If readyState is "ended", then set the end time on the last range in
    //     source ranges to highest end time.
    if (ended && !source_ranges.empty())
      source_ranges.Add(source_ranges.back().start, highest_end_time);

    // 5.3 Let new intersection ranges equal the the intersection between the
    //     intersection ranges and the source ranges.
    // 5.4 Replace the ranges in intersection ranges with the new intersection
    //     ranges.
    intersection_ranges.IntersectWith(source_ranges);
  }

  return intersection_ranges;
}

WebTimeRanges MediaSource::SeekableInternal(
    MediaSourceAttachmentSupplement::ExclusiveKey pass_key) const {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();
  {
    base::AutoLock lock(attachment_link_lock_);
    DCHECK(media_source_attachment_)
        << "Seekable should only be used when attached to HTMLMediaElement";
  }

  // Implements MediaSource algorithm for HTMLMediaElement.seekable.
  // http://w3c.github.io/media-source/#htmlmediaelement-extensions
  WebTimeRanges ranges;

  double source_duration = GetDuration_Locked(pass_key);

  // If duration equals NaN: Return an empty TimeRanges object.
  if (std::isnan(source_duration))
    return ranges;

  // If duration equals positive Infinity:
  if (source_duration == std::numeric_limits<double>::infinity()) {
    WebTimeRanges buffered = BufferedInternal(pass_key);

    // 1. If live seekable range is not empty:
    if (has_live_seekable_range_) {
      // 1.1. Let union ranges be the union of live seekable range and the
      //      HTMLMediaElement.buffered attribute.
      // 1.2. Return a single range with a start time equal to the
      //      earliest start time in union ranges and an end time equal to
      //      the highest end time in union ranges and abort these steps.
      if (buffered.empty()) {
        ranges.emplace_back(live_seekable_range_start_,
                            live_seekable_range_end_);
        return ranges;
      }

      ranges.emplace_back(
          std::min(live_seekable_range_start_, buffered.front().start),
          std::max(live_seekable_range_end_, buffered.back().end));
      return ranges;
    }

    // 2. If the HTMLMediaElement.buffered attribute returns an empty TimeRanges
    //    object, then return an empty TimeRanges object and abort these steps.
    if (buffered.empty())
      return ranges;

    // 3. Return a single range with a start time of 0 and an end time equal to
    //    the highest end time reported by the HTMLMediaElement.buffered
    //    attribute.
    ranges.emplace_back(0, buffered.back().end);
    return ranges;
  }

  // 3. Otherwise: Return a single range with a start time of 0 and an end time
  //    equal to duration.
  ranges.emplace_back(0, source_duration);
  return ranges;
}

void MediaSource::OnTrackChanged(TrackBase* track) {
  // TODO(https://crbug.com/878133): Support this in MSE-in-Worker once
  // TrackBase and TrackListBase are usable on worker and do not explicitly
  // require an HTMLMediaElement. The update to |active_source_buffers_| will
  // also require sending updated buffered and seekable information to the main
  // thread, though the CTMSA itself would best know when to do that since it is
  // this method should only be called by an attachment.
  DCHECK(IsMainThread());

  SourceBuffer* source_buffer =
      SourceBufferTrackBaseSupplement::sourceBuffer(*track);
  if (!source_buffer)
    return;

  DCHECK(source_buffers_->Contains(source_buffer));
  if (track->GetType() == WebMediaPlayer::kAudioTrack) {
    source_buffer->audioTracks().ScheduleChangeEvent();
  } else if (track->GetType() == WebMediaPlayer::kVideoTrack) {
    if (static_cast<VideoTrack*>(track)->selected())
      source_buffer->videoTracks().TrackSelected(track->id());
    source_buffer->videoTracks().ScheduleChangeEvent();
  }

  bool is_active = (source_buffer->videoTracks().selectedIndex() != -1) ||
                   source_buffer->audioTracks().HasEnabledTrack();
  SetSourceBufferActive(source_buffer, is_active);
}

void MediaSource::setDuration(double duration,
                              ExceptionState& exception_state) {
  DVLOG(3) << __func__ << " this=" << this << " : duration=" << duration;

  // 2.1 https://www.w3.org/TR/media-source/#widl-MediaSource-duration
  // 1. If the value being set is negative or NaN then throw a TypeError
  // exception and abort these steps.
  if (std::isnan(duration)) {
    LogAndThrowTypeError(exception_state, ExceptionMessages::NotAFiniteNumber(
                                              duration, "duration"));
    return;
  }
  if (duration < 0.0) {
    LogAndThrowTypeError(
        exception_state,
        ExceptionMessages::IndexExceedsMinimumBound("duration", duration, 0.0));
    return;
  }

  // 2. If the readyState attribute is not "open" then throw an
  //    InvalidStateError exception and abort these steps.
  // 3. If the updating attribute equals true on any SourceBuffer in
  //    sourceBuffers, then throw an InvalidStateError exception and abort these
  //    steps.
  if (ThrowExceptionIfClosedOrUpdating(IsOpen(), IsUpdating(), exception_state))
    return;

  // 4. Run the duration change algorithm with new duration set to the value
  //    being assigned to this attribute.
  // Do remainder of steps only if attachment is usable and underlying demuxer
  // is protected from destruction (applicable especially for MSE-in-Worker
  // case). Note, we must be open, therefore we must have an attachment.
  if (!RunUnlessElementGoneOrClosingUs(WTF::BindOnce(
          &MediaSource::DurationChangeAlgorithm, WrapPersistent(this), duration,
          WTF::Unretained(&exception_state)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "Worker MediaSource attachment is closing");
  }
}

double MediaSource::duration() {
  double duration_result = std::numeric_limits<float>::quiet_NaN();
  if (IsClosed())
    return duration_result;

  // Note, here we must be open or ended, therefore we must have an attachment.
  if (!RunUnlessElementGoneOrClosingUs(WTF::BindOnce(
          [](MediaSource* self, double* result,
             MediaSourceAttachmentSupplement::ExclusiveKey pass_key) {
            *result = self->GetDuration_Locked(pass_key);
          },
          WrapPersistent(this), WTF::Unretained(&duration_result)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, result should be in this case. It seems reasonable
    // to behave is if we are in "closed" readyState and report NaN to the app
    // here.
    DCHECK_EQ(duration_result, std::numeric_limits<float>::quiet_NaN());
  }

  return duration_result;
}

void MediaSource::DurationChangeAlgorithm(
    double new_duration,
    ExceptionState* exception_state,
    MediaSourceAttachmentSupplement::ExclusiveKey pass_key) {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // http://w3c.github.io/media-source/#duration-change-algorithm
  // 1. If the current value of duration is equal to new duration, then return.
  double old_duration = GetDuration_Locked(pass_key);
  if (new_duration == old_duration)
    return;

  // 2. If new duration is less than the highest starting presentation
  // timestamp of any buffered coded frames for all SourceBuffer objects in
  // sourceBuffers, then throw an InvalidStateError exception and abort these
  // steps. Note: duration reductions that would truncate currently buffered
  // media are disallowed. When truncation is necessary, use remove() to
  // reduce the buffered range before updating duration.
  double highest_buffered_presentation_timestamp = 0;
  for (unsigned i = 0; i < source_buffers_->length(); ++i) {
    highest_buffered_presentation_timestamp =
        std::max(highest_buffered_presentation_timestamp,
                 source_buffers_->item(i)->HighestPresentationTimestamp());
  }

  if (new_duration < highest_buffered_presentation_timestamp) {
    if (RuntimeEnabledFeatures::MediaSourceNewAbortAndDurationEnabled()) {
      LogAndThrowDOMException(
          *exception_state, DOMExceptionCode::kInvalidStateError,
          "Setting duration below highest presentation timestamp of any "
          "buffered coded frames is disallowed. Instead, first do asynchronous "
          "remove(newDuration, oldDuration) on all sourceBuffers, where "
          "newDuration < oldDuration.");
      return;
    }

    Deprecation::CountDeprecation(
        GetExecutionContext(),
        WebFeature::kMediaSourceDurationTruncatingBuffered);
    // See also deprecated remove(new duration, old duration) behavior below.
  }

  DCHECK_LE(highest_buffered_presentation_timestamp,
            std::isnan(old_duration) ? 0 : old_duration);

  // 3. Set old duration to the current value of duration.
  // Done for step 1 above, already.
  // 4. Update duration to new duration.
  web_media_source_->SetDuration(new_duration);

  if (!RuntimeEnabledFeatures::MediaSourceNewAbortAndDurationEnabled() &&
      new_duration < old_duration) {
    // Deprecated behavior: if the new duration is less than old duration,
    // then call remove(new duration, old duration) on all all objects in
    // sourceBuffers.
    for (unsigned i = 0; i < source_buffers_->length(); ++i) {
      source_buffers_->item(i)->Remove_Locked(new_duration, old_duration,
                                              &ASSERT_NO_EXCEPTION, pass_key);
    }
  }

  // 5. If a user agent is unable to partially render audio frames or text cues
  //    that start before and end after the duration, then run the following
  //    steps:
  //    NOTE: Currently we assume that the media engine is able to render
  //    partial frames/cues. If a media engine gets added that doesn't support
  //    this, then we'll need to add logic to handle the substeps.

  // 6. Update the media controller duration to new duration and run the
  //    HTMLMediaElement duration change algorithm.
  auto [attachment, tracer] = AttachmentAndTracer();
  attachment->NotifyDurationChanged(tracer, new_duration);
}

void MediaSource::SetReadyState(const ReadyState state) {
  DCHECK(state == ReadyState::kOpen || state == ReadyState::kClosed ||
         state == ReadyState::kEnded);

  ReadyState old_state = ready_state_;
  DVLOG(3) << __func__ << " this=" << this << " : "
           << ReadyStateToString(old_state) << " -> "
           << ReadyStateToString(state);

  if (state == ReadyState::kClosed) {
    web_media_source_.reset();
  }

  if (old_state == state)
    return;

  ready_state_ = state;

  OnReadyStateChange(old_state, state);
}

AtomicString MediaSource::readyState() const {
  return ReadyStateToString(ready_state_);
}

void MediaSource::endOfStream(ExceptionState& exception_state) {
  endOfStream(std::nullopt, exception_state);
}

void MediaSource::endOfStream(std::optional<V8EndOfStreamError> error,
                              ExceptionState& exception_state) {
  DVLOG(3) << __func__ << " this=" << this
           << " : error=" << (error.has_value() ? error->AsCStr() : "");

  // https://www.w3.org/TR/media-source/#dom-mediasource-endofstream
  // 1. If the readyState attribute is not in the "open" state then throw an
  //    InvalidStateError exception and abort these steps.
  // 2. If the updating attribute equals true on any SourceBuffer in
  //    sourceBuffers, then throw an InvalidStateError exception and abort these
  //    steps.
  if (ThrowExceptionIfClosedOrUpdating(IsOpen(), IsUpdating(), exception_state))
    return;

  // 3. Run the end of stream algorithm with the error parameter set to error.
  WebMediaSource::EndOfStreamStatus status =
      WebMediaSource::kEndOfStreamStatusNoError;
  if (error.has_value()) {
    switch (error->AsEnum()) {
      case V8EndOfStreamError::Enum::kNetwork:
        status = WebMediaSource::kEndOfStreamStatusNetworkError;
        break;
      case V8EndOfStreamError::Enum::kDecode:
        status = WebMediaSource::kEndOfStreamStatusDecodeError;
        break;
      default:
        NOTREACHED();
    }
  }

  // Do remainder of steps only if attachment is usable and underlying demuxer
  // is protected from destruction (applicable especially for MSE-in-Worker
  // case). Note, we must be open, therefore we must have an attachment.
  if (!RunUnlessElementGoneOrClosingUs(WTF::BindOnce(
          &MediaSource::EndOfStreamAlgorithm, WrapPersistent(this), status))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "Worker MediaSource attachment is closing");
  }
}

void MediaSource::setLiveSeekableRange(double start,
                                       double end,
                                       ExceptionState& exception_state) {
  DVLOG(3) << __func__ << " this=" << this << " : start=" << start
           << ", end=" << end;

  // http://w3c.github.io/media-source/#widl-MediaSource-setLiveSeekableRange-void-double-start-double-end
  // 1. If the readyState attribute is not "open" then throw an
  //    InvalidStateError exception and abort these steps.
  // 2. If the updating attribute equals true on any SourceBuffer in
  //    SourceBuffers, then throw an InvalidStateError exception and abort
  //    these steps.
  //    Note: https://github.com/w3c/media-source/issues/118, once fixed, will
  //    remove the updating check (step 2). We skip that check here already.
  if (ThrowExceptionIfClosed(IsOpen(), exception_state))
    return;

  // 3. If start is negative or greater than end, then throw a TypeError
  //    exception and abort these steps.
  if (start < 0 || start > end) {
    LogAndThrowTypeError(
        exception_state,
        ExceptionMessages::IndexOutsideRange(
            "start value", start, 0.0, ExceptionMessages::kInclusiveBound, end,
            ExceptionMessages::kInclusiveBound));
    return;
  }

  // Note, here we must be open, therefore we must have an attachment.
  if (!RunUnlessElementGoneOrClosingUs(
          WTF::BindOnce(&MediaSource::SetLiveSeekableRange_Locked,
                        WrapPersistent(this), start, end))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "Worker MediaSource attachment is closing");
  }
}

void MediaSource::SetLiveSeekableRange_Locked(
    double start,
    double end,
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // 4. Set live seekable range to be a new normalized TimeRanges object
  //    containing a single range whose start position is start and end
  //    position is end.
  has_live_seekable_range_ = true;
  live_seekable_range_start_ = start;
  live_seekable_range_end_ = end;

  SendUpdatedInfoToMainThreadCache();
}

void MediaSource::clearLiveSeekableRange(ExceptionState& exception_state) {
  DVLOG(3) << __func__ << " this=" << this;

  // http://w3c.github.io/media-source/#widl-MediaSource-clearLiveSeekableRange-void
  // 1. If the readyState attribute is not "open" then throw an
  //    InvalidStateError exception and abort these steps.
  // 2. If the updating attribute equals true on any SourceBuffer in
  //    SourceBuffers, then throw an InvalidStateError exception and abort
  //    these steps.
  //    Note: https://github.com/w3c/media-source/issues/118, once fixed, will
  //    remove the updating check (step 2). We skip that check here already.
  if (ThrowExceptionIfClosed(IsOpen(), exception_state))
    return;

  // Note, here we must be open, therefore we must have an attachment.
  if (!RunUnlessElementGoneOrClosingUs(WTF::BindOnce(
          &MediaSource::ClearLiveSeekableRange_Locked, WrapPersistent(this)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "Worker MediaSource attachment is closing");
  }
}

void MediaSource::ClearLiveSeekableRange_Locked(
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // 3. If live seekable range contains a range, then set live seekable range
  //    to be a new empty TimeRanges object.
  if (!has_live_seekable_range_)
    return;

  has_live_seekable_range_ = false;
  live_seekable_range_start_ = 0.0;
  live_seekable_range_end_ = 0.0;

  SendUpdatedInfoToMainThreadCache();
}

MediaSourceHandleImpl* MediaSource::handle() {
  base::AutoLock lock(attachment_link_lock_);

  DVLOG(3) << __func__;

  // TODO(crbug.com/506273): Support MediaSource srcObject attachment idiom for
  // main-thread-owned MediaSource objects (would need MSE spec updates, too,
  // and might not involve a handle regardless).
  DCHECK(!IsMainThread() &&
         GetExecutionContext()->IsDedicatedWorkerGlobalScope());

  // Per
  // https://www.w3.org/TR/2022/WD-media-source-2-20220921/#dom-mediasource-handle:
  // If the handle for this MediaSource object has not yet been created, then
  // run the following steps:
  // 1.1. Let created handle be the result of creating a new MediaSourceHandle
  //      object and associated resources, linked internally to this
  //      MediaSource.
  // 1.2. Update the attribute to be created handle.
  if (!worker_media_source_handle_) {
    // Lazily create the handle, since it indirectly holds a
    // CrossThreadMediaSourceAttachment (until attachment starts or the handle
    // is transferred) which holds a strong reference to us until attachment is
    // actually started and later closed. PassKey provider usage here ensures
    // that we are allowed to call the attachment constructor.
    scoped_refptr<CrossThreadMediaSourceAttachment> attachment =
        base::MakeRefCounted<CrossThreadMediaSourceAttachment>(
            this, AttachmentCreationPassKeyProvider::GetPassKey());
    scoped_refptr<HandleAttachmentProvider> attachment_provider =
        base::MakeRefCounted<HandleAttachmentProvider>(std::move(attachment));

    // Create, but don't "register" an internal blob URL with the security
    // origin of the worker's execution context for use later in a window thread
    // media element's attachment to the MediaSource leveraging existing URL
    // security checks and logging for legacy MSE object URLs.
    const SecurityOrigin* origin = GetExecutionContext()->GetSecurityOrigin();
    String internal_blob_url = BlobURL::CreatePublicURL(origin).GetString();
    DCHECK(!internal_blob_url.empty());
    worker_media_source_handle_ = MakeGarbageCollected<MediaSourceHandleImpl>(
        std::move(attachment_provider), std::move(internal_blob_url));
  }

  // Per
  // https://www.w3.org/TR/2022/WD-media-source-2-20220921/#dom-mediasource-handle:
  // 2. Return the MediaSourceHandle object that is this attribute's value.
  DCHECK(worker_media_source_handle_);
  return worker_media_source_handle_.Get();
}

bool MediaSource::IsOpen() const {
  return ready_state_ == ReadyState::kOpen;
}

void MediaSource::SetSourceBufferActive(SourceBuffer* source_buffer,
                                        bool is_active) {
  if (!is_active) {
    DCHECK(active_source_buffers_->Contains(source_buffer));
    active_source_buffers_->Remove(source_buffer);
    return;
  }

  if (active_source_buffers_->Contains(source_buffer))
    return;

  // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-MediaSource-activeSourceBuffers
  // SourceBuffer objects in SourceBuffer.activeSourceBuffers must appear in
  // the same order as they appear in SourceBuffer.sourceBuffers.
  // SourceBuffer transitions to active are not guaranteed to occur in the
  // same order as buffers in |m_sourceBuffers|, so this method needs to
  // insert |sourceBuffer| into |m_activeSourceBuffers|.
  wtf_size_t index_in_source_buffers = source_buffers_->Find(source_buffer);
  DCHECK(index_in_source_buffers != kNotFound);

  wtf_size_t insert_position = 0;
  while (insert_position < active_source_buffers_->length() &&
         source_buffers_->Find(active_source_buffers_->item(insert_position)) <
             index_in_source_buffers) {
    ++insert_position;
  }

  active_source_buffers_->insert(insert_position, source_buffer);
}

std::pair<scoped_refptr<MediaSourceAttachmentSupplement>, MediaSourceTracer*>
MediaSource::AttachmentAndTracer() const {
  base::AutoLock lock(attachment_link_lock_);
  return std::make_pair(media_source_attachment_, attachment_tracer_.Get());
}

void MediaSource::EndOfStreamAlgorithm(
    const WebMediaSource::EndOfStreamStatus eos_status,
    MediaSourceAttachmentSupplement::ExclusiveKey pass_key) {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // https://www.w3.org/TR/media-source/#end-of-stream-algorithm
  // 1. Change the readyState attribute value to "ended".
  // 2. Queue a task to fire a simple event named sourceended at the
  //    MediaSource.
  SetReadyState(ReadyState::kEnded);

  // 3. Do various steps based on |eos_status|.
  web_media_source_->MarkEndOfStream(eos_status);

  if (eos_status == WebMediaSource::kEndOfStreamStatusNoError) {
    // The implementation may not have immediately informed the attached element
    // (known by the |media_source_attachment_| and |attachment_tracer_|) of the
    // potentially reduced duration. Prevent app-visible duration race by
    // synchronously running the duration change algorithm. The MSE spec
    // supports this:
    // https://www.w3.org/TR/media-source/#end-of-stream-algorithm
    // 2.4.7.3 (If error is not set)
    // Run the duration change algorithm with new duration set to the largest
    // track buffer ranges end time across all the track buffers across all
    // SourceBuffer objects in sourceBuffers.
    //
    // Since MarkEndOfStream caused the demuxer to update its duration (similar
    // to the MediaSource portion of the duration change algorithm), all that
    // is left is to notify the element.
    // TODO(wolenetz): Consider refactoring the MarkEndOfStream implementation
    // to just mark end of stream, and move the duration reduction logic to here
    // so we can just run DurationChangeAlgorithm(...) here.
    double new_duration = GetDuration_Locked(pass_key);
    auto [attachment, tracer] = AttachmentAndTracer();
    attachment->NotifyDurationChanged(tracer, new_duration);
  } else {
    // Even though error didn't change duration, the transition to kEnded
    // impacts the buffered ranges calculation, so let the attachment know that
    // a cross-thread media element needs to be sent updated information.
    SendUpdatedInfoToMainThreadCache();
  }
}

bool MediaSource::IsClosed() const {
  return ready_state_ == ReadyState::kClosed;
}

void MediaSource::Close() {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();
  SetReadyState(ReadyState::kClosed);
}

MediaSourceTracer* MediaSource::StartAttachingToMediaElement(
    scoped_refptr<SameThreadMediaSourceAttachment> attachment,
    HTMLMediaElement* element) {
  base::AutoLock lock(attachment_link_lock_);

  DCHECK(IsMainThread());

  if (media_source_attachment_ || attachment_tracer_) {
    return nullptr;
  }

  DCHECK(!context_already_destroyed_);
  DCHECK(IsClosed());

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media",
                                    "MediaSource::StartAttachingToMediaElement",
                                    TRACE_ID_LOCAL(this));
  media_source_attachment_ = attachment;
  attachment_tracer_ =
      MakeGarbageCollected<SameThreadMediaSourceTracer>(element, this);
  return attachment_tracer_.Get();
}

bool MediaSource::StartWorkerAttachingToMainThreadMediaElement(
    scoped_refptr<CrossThreadMediaSourceAttachment> attachment) {
  base::AutoLock lock(attachment_link_lock_);

  // Even in worker-owned MSE, the CrossThreadMediaSourceAttachment calls this
  // on the main thread.
  DCHECK(IsMainThread());
  DCHECK(!attachment_tracer_);  // A worker-owned MediaSource has no tracer.

  if (context_already_destroyed_) {
    return false;  // See comments in ContextDestroyed().
  }

  if (media_source_attachment_ || attachment_tracer_) {
    return false;  // Already attached.
  }

  DCHECK(IsClosed());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "media", "MediaSource::StartWorkerAttachingToMainThreadMediaElement",
      TRACE_ID_LOCAL(this));
  media_source_attachment_ = attachment;
  return true;
}

void MediaSource::OpenIfInEndedState() {
  if (ready_state_ != ReadyState::kEnded)
    return;

  // All callers of this method (see SourceBuffer methods) must have already
  // confirmed they are still associated with us, and therefore we must not be
  // closed. In one edge case (!notify_close version of our
  // DetachWorkerOnContextDestruction_Locked), any associated SourceBuffers are
  // not told they're dissociated with us in that method, but it is run on the
  // worker thread that is also synchronously destructing the SourceBuffers'
  // context). Therefore the following should never fail here.
  DCHECK(!IsClosed());

  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  SetReadyState(ReadyState::kOpen);
  web_media_source_->UnmarkEndOfStream();

  // This change impacts buffered and seekable calculations, so let the
  // attachment know to update if cross-thread.
  SendUpdatedInfoToMainThreadCache();
}

bool MediaSource::HasPendingActivity() const {
  // Note that an unrevoked MediaSource objectUrl for an otherwise inactive,
  // unreferenced HTMLME with MSE still attached will prevent GC of the whole
  // group of objects. This is yet further motivation for apps to properly
  // revokeObjectUrl and for the MSE spec, implementations and API users to
  // transition to using HTMLME srcObject for MSE attachment instead of
  // objectUrl. For at least SameThreadMediaSourceAttachments, the
  // RevokeMediaSourceObjectURLOnAttach feature assists in automating this case.
  // But for CrossThreadMediaSourceAttachments, the attachment holds strong
  // references to each side until explicitly detached (or contexts destroyed).
  // The latter applies similarly when using MediaSourceHandle for srcObject
  // attachment of a worker MediaSource: the handle object has a scoped_refptr
  // to the underlying attachment until the handle is GC'ed.
  return async_event_queue_->HasPendingEvents();
}

void MediaSource::ContextDestroyed() {
  DVLOG(1) << __func__ << " this=" << this;

  // In same-thread case, we just close ourselves if not already closed. This is
  // historically the same logic as before MSE-in-Workers. Note that we cannot
  // inspect GetExecutionContext() to determine Window vs Worker here, so we use
  // IsMainThread(). There is no need to RunExclusively() either, because we are
  // on the same thread as the media element.
  if (IsMainThread()) {
    {
      base::AutoLock lock(attachment_link_lock_);
      if (media_source_attachment_) {
        DCHECK(attachment_tracer_);  // Same-thread attachment uses tracer.
        // No need to release |attachment_link_lock_| and RunExclusively(),
        // since it is a same-thread attachment.
        media_source_attachment_->OnMediaSourceContextDestroyed();
      }

      // For consistency, though redundant for same-thread operation, prevent
      // subsequent attachment start from succeeding. This flag is meaningful in
      // cross-thread attachment usage.
      context_already_destroyed_ = true;
    }

    if (!IsClosed()) {
      SetReadyState(ReadyState::kClosed);
    }
    web_media_source_.reset();
    return;
  }

  // Worker context destruction could race CrossThreadMediaSourceAttachment's
  // StartAttachingToMediaElement on the main thread: we could finish
  // ContextDestroyed() here, and in the case of not yet ever having been
  // attached using a particular CrossThreadMediaSourceAttachent, then receive a
  // StartWorkerAttachingToMainThreadMediaElement() call before unregistration
  // of us has completed. Therefore, we use our |attachment_link_lock_| to also
  // protect a flag here that lets us know to fail any future attempt to start
  // attaching to us.
  scoped_refptr<MediaSourceAttachmentSupplement> attachment;
  {
    base::AutoLock lock(attachment_link_lock_);
    context_already_destroyed_ = true;

    // If not yet attached, the flag, above, will prevent us from ever
    // successfully attaching, and we can return. There is no attachment on
    // which we need (or can) call OnMediaSourceContextDestroyed() here. And any
    // attachments owned by this context will soon (or have already been)
    // unregistered.
    attachment = media_source_attachment_;
    if (!attachment) {
      DCHECK(IsClosed());
      DCHECK(!web_media_source_);
      return;
    }
  }

  // We need to let our current attachment know that our context is destroyed.
  // This will let it handle cases like returning sane values for
  // BufferedInternal and SeekableInternal and stop further use of us via the
  // attachment. We need to hold the attachment's |attachment_state_lock_| when
  // doing this detachment.
  bool cb_ran = attachment->RunExclusively(
      true /* abort if unsafe to use underlying demuxer */,
      WTF::BindOnce(&MediaSource::DetachWorkerOnContextDestruction_Locked,
                    WrapPersistent(this),
                    true /* safe to notify underlying demuxer */));

  if (!cb_ran) {
    // Main-thread is already detaching or destructing the underlying demuxer.
    CHECK(attachment->RunExclusively(
        false /* do not abort */,
        WTF::BindOnce(&MediaSource::DetachWorkerOnContextDestruction_Locked,
                      WrapPersistent(this),
                      false /* do not notify underlying demuxer */)));
  }
}

void MediaSource::DetachWorkerOnContextDestruction_Locked(
    bool notify_close,
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  {
    base::AutoLock lock(attachment_link_lock_);

    DCHECK(!IsMainThread());  // Called only on the worker thread.

    DVLOG(1) << __func__ << " this=" << this
             << ", notify_close=" << notify_close;

    // Close() could not race our dispatch: it must happen on worker thread, on
    // which we're called synchronously only if we're attached.
    DCHECK(media_source_attachment_);

    // We're only called for CrossThread attachments, which use no tracer.
    DCHECK(!attachment_tracer_);

    // Let the attachment know to prevent further operations on us.
    media_source_attachment_->OnMediaSourceContextDestroyed();

    if (!notify_close) {
      // In this case, not only is our context shutting down, but the media
      // element is also at least tearing down the WebMediaPlayer (and the
      // underlying demuxer owned by it) already. We can do some simple cleanup,
      // but must not access |*web_media_source_| or our SourceBuffers'
      // |*web_source_buffer_|'s. We're helped by the demuxer not calling us or
      // our SourceBuffers unless in scope of a call initiated by a SourceBuffer
      // during media parsing, which cannot occur after our context destruction.
      // Underlying buffered media is removed during demuxer teardown itself,
      // which is certain to be happening already or soon in this case.
      media_source_attachment_.reset();
      attachment_tracer_ = nullptr;  // For consistency with same-thread usage.
      if (!IsClosed()) {
        ready_state_ = ReadyState::kClosed;
        web_media_source_.reset();
        active_source_buffers_->Clear();
        source_buffers_->Clear();
      }
      return;
    }
  }

  // TODO(https://crbug.com/878133): Here, if we have a |web_media_source_|,
  // determine how to specify notification of a "defunct" worker-thread
  // MediaSource in the case where it was serving as the source for a media
  // element. Directly notifying an error via the |web_media_source_| may be the
  // appropriate route here, but MarkEndOfStream internally has constraints
  // (already initialized demuxer, not already "ended", etc) which make it
  // unsuitable currently for this purpose. Currently, we prevent further usage
  // of the underlying demuxer and return sane values to the element for its
  // queries (nothing buffered, nothing seekable) once the attached media
  // source's context is destroyed. See similar case in
  // CrossThreadMediaSourceAttachment's
  // CompleteAttachingToMediaElementOnWorkerThread(). For now, we'll just do the
  // historical steps to shutdown the MediaSource and SourceBuffers on context
  // destruction.
  if (!IsClosed())
    SetReadyState(ReadyState::kClosed);
  web_media_source_.reset();
}

std::unique_ptr<WebSourceBuffer> MediaSource::CreateWebSourceBuffer(
    const String& type,
    const String& codecs,
    std::unique_ptr<media::AudioDecoderConfig> audio_config,
    std::unique_ptr<media::VideoDecoderConfig> video_config,
    ExceptionState& exception_state) {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  std::unique_ptr<WebSourceBuffer> web_source_buffer;
  WebMediaSource::AddStatus add_status;
  if (audio_config) {
    DCHECK(!video_config);
    DCHECK(type.IsNull() && codecs.IsNull());
    web_source_buffer = web_media_source_->AddSourceBuffer(
        std::move(audio_config), add_status /* out */);
    DCHECK_NE(add_status, WebMediaSource::kAddStatusNotSupported);
  } else if (video_config) {
    DCHECK(type.IsNull() && codecs.IsNull());
    web_source_buffer = web_media_source_->AddSourceBuffer(
        std::move(video_config), add_status /* out */);
    DCHECK_NE(add_status, WebMediaSource::kAddStatusNotSupported);
  } else {
    DCHECK(!type.IsNull());
    web_source_buffer =
        web_media_source_->AddSourceBuffer(type, codecs, add_status /* out */);
  }

  switch (add_status) {
    case WebMediaSource::kAddStatusOk:
      DCHECK(web_source_buffer);
      return web_source_buffer;
    case WebMediaSource::kAddStatusNotSupported:
      // DCHECKs, above, ensure this case doesn't occur for the WebCodecs config
      // overloads of WebMediaSource::AddSourceBuffer(). This case can only
      // occur for the |type| and |codecs| version of that method.
      DCHECK(!web_source_buffer);
      // TODO(crbug.com/1144908): Are we certain that if we originally had an
      // audio_config or video_config, above, that it should be supported? In
      // that case, we could possibly add some DCHECK here if attempt to use
      // them failed in this case.
      //
      // 2.2
      // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
      // Step 2: If type contains a MIME type ... that is not supported with the
      // types specified for the other SourceBuffer objects in sourceBuffers,
      // then throw a NotSupportedError exception and abort these steps.
      LogAndThrowDOMException(
          exception_state, DOMExceptionCode::kNotSupportedError,
          "The type provided ('" + type +
              "') is not supported for SourceBuffer creation.");
      return nullptr;
    case WebMediaSource::kAddStatusReachedIdLimit:
      DCHECK(!web_source_buffer);
      // 2.2
      // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
      // Step 3: If the user agent can't handle any more SourceBuffer objects
      // then throw a QuotaExceededError exception and abort these steps.
      LogAndThrowDOMException(exception_state,
                              DOMExceptionCode::kQuotaExceededError,
                              "This MediaSource has reached the limit of "
                              "SourceBuffer objects it can handle. No "
                              "additional SourceBuffer objects may be added.");
      return nullptr;
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void MediaSource::ScheduleEvent(const AtomicString& event_name) {
  DCHECK(async_event_queue_);

  Event* event = Event::Create(event_name);
  event->SetTarget(this);

  async_event_queue_->EnqueueEvent(FROM_HERE, *event);
}

}  // namespace blink
