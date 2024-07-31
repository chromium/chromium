// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webcodecs/image_decoder_external.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybufferallowshared_arraybufferviewallowshared_readablestream.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decode_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/webcodecs/image_track.h"
#include "third_party/blink/renderer/modules/webcodecs/image_track_list.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image_metrics.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_skia.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"

namespace blink {

namespace {

bool IsTypeSupportedInternal(String type) {
  if (!type.ContainsOnlyASCIIOrEmpty())
    return false;

  // Disable ICO/CUR decoding since the underlying decoder does not operate like
  // the rest of our blink::ImageDecoders. Each frame is a different sized
  // version of a single image in a BMP or PNG format. CUR files additionally
  // use the mouse position to determine which image to use.
  //
  // While we could expose each frame as a different track or use the desired
  // size provided at construction to choose a frame, the mouse position signal
  // would need further JS exposed API considerations. As such, given the
  // ancient nature of the format, it is not worth implementing at this time.
  //
  // Additionally, since the ICO/CUR formats are simple, it seems fine to allow
  // the parsing to happen in JS while decoding for the individual BMP or PNG
  // files can be done using this API.
  const auto type_lower = type.LowerASCII();
  if (type_lower == "image/x-icon" || type_lower == "image/vnd.microsoft.icon")
    return false;

  return IsSupportedImageMimeType(type.Ascii());
}

ImageDecoder::AnimationOption AnimationOptionFromIsAnimated(bool is_animated) {
  return is_animated ? ImageDecoder::AnimationOption::kPreferAnimation
                     : ImageDecoder::AnimationOption::kPreferStillImage;
}

DOMException* CreateUnsupportedImageTypeException(String type) {
  return MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError,
      String::Format("The provided image type (%s) is not supported",
                     type.Ascii().c_str()));
}

// Helper class for ensuring memory safe usage of ArrayBufferContents by the
// ImageDecoderCore on the decoding thread.
class ArrayBufferContentsSegmentReader : public SegmentReader {
 public:
  explicit ArrayBufferContentsSegmentReader(ArrayBufferContents contents)
      : contents_(std::move(contents)),
        segment_reader_(SegmentReader::CreateFromSkData(
            SkData::MakeWithoutCopy(contents_.Data(),
                                    contents_.DataLength()))) {}

  size_t size() const override { return segment_reader_->size(); }
  size_t GetSomeData(const char*& data, size_t position) const override {
    return segment_reader_->GetSomeData(data, position);
  }
  sk_sp<SkData> GetAsSkData() const override {
    return segment_reader_->GetAsSkData();
  }

 private:
  ArrayBufferContents contents_;  // Must outlive `segment_reader_`.
  scoped_refptr<SegmentReader> segment_reader_;
};

}  // namespace

// static
ImageDecoderExternal* ImageDecoderExternal::Create(
    ScriptState* script_state,
    const ImageDecoderInit* init,
    ExceptionState& exception_state) {
  auto* result = MakeGarbageCollected<ImageDecoderExternal>(script_state, init,
                                                            exception_state);
  return exception_state.HadException() ? nullptr : result;
}

ImageDecoderExternal::DecodeRequest::DecodeRequest(
    ScriptPromiseResolver<ImageDecodeResult>* resolver,
    uint32_t frame_index,
    bool complete_frames_only)
    : resolver(resolver),
      frame_index(frame_index),
      complete_frames_only(complete_frames_only),
      abort_flag(std::make_unique<base::AtomicFlag>()) {}

ImageDecoderExternal::DecodeRequest::~DecodeRequest() {
  // This must have already been released to the decoder thread manually.
  DCHECK(!abort_flag);
}

void ImageDecoderExternal::DecodeRequest::Trace(Visitor* visitor) const {
  visitor->Trace(resolver);
  visitor->Trace(result);
  visitor->Trace(exception);
}

bool ImageDecoderExternal::DecodeRequest::IsFinal() const {
  return result || exception || range_error_message;
}

// static
ScriptPromise<IDLBoolean> ImageDecoderExternal::isTypeSupported(
    ScriptState* script_state,
    String type) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(script_state);
  auto promise = resolver->Promise();
  resolver->Resolve(IsTypeSupportedInternal(type));
  return promise;
}

ImageDecoderExternal::ImageDecoderExternal(ScriptState* script_state,
                                           const ImageDecoderInit* init,
                                           ExceptionState& exception_state)
    : ActiveScriptWrappable<ImageDecoderExternal>({}),
      ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      script_state_(script_state),
      tracks_(MakeGarbageCollected<ImageTrackList>(this)),
      completed_property_(
          MakeGarbageCollected<CompletedProperty>(GetExecutionContext())) {
  // ImageDecoder requires an active context to operate correctly.
  if (GetExecutionContext()->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Invalid context.");
    return;
  }

  UseCounter::Count(GetExecutionContext(), WebFeature::kWebCodecs);

  // |data| is a required field.
  DCHECK(init->hasData());
  DCHECK(init->data());

  constexpr char kNoneOption[] = "none";
  auto color_behavior = ColorBehavior::kTag;
  if (init->colorSpaceConversion() == kNoneOption)
    color_behavior = ColorBehavior::kIgnore;

  auto desired_size = SkISize::MakeEmpty();
  if (init->hasDesiredWidth() && init->hasDesiredHeight())
    desired_size = SkISize::Make(init->desiredWidth(), init->desiredHeight());

  mime_type_ = init->type().LowerASCII();
  if (!IsTypeSupportedInternal(mime_type_)) {
    tracks_->OnTracksReady(CreateUnsupportedImageTypeException(mime_type_));
    return;
  }

  if (init->hasPreferAnimation()) {
    prefer_animation_ = init->preferAnimation();
    animation_option_ = AnimationOptionFromIsAnimated(*prefer_animation_);
  }

  decode_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  if (init->data()->IsReadableStream()) {
    if (init->data()->GetAsReadableStream()->IsLocked() ||
        init->data()->GetAsReadableStream()->IsDisturbed()) {
      exception_state.ThrowTypeError(
          "ImageDecoder can only accept readable streams that are not yet "
          "locked to a reader");
      return;
    }

    decoder_ = std::make_unique<WTF::SequenceBound<ImageDecoderCore>>(
        decode_task_runner_, mime_type_, /*data=*/nullptr,
        /*data_complete=*/false, color_behavior, desired_size,
        animation_option_);

    consumer_ = MakeGarbageCollected<ReadableStreamBytesConsumer>(
        script_state, init->data()->GetAsReadableStream());

    construction_succeeded_ = true;

    // We need one initial call to OnStateChange() to start reading, but
    // thereafter calls will be driven by the ReadableStreamBytesConsumer.
    consumer_->SetClient(this);
    OnStateChange();
    return;
  }

  base::span<const uint8_t> array_span;
  switch (init->data()->GetContentType()) {
    case V8ImageBufferSource::ContentType::kArrayBufferAllowShared:
      if (auto* data_ptr = init->data()->GetAsArrayBufferAllowShared()) {
        if (!data_ptr->IsDetached()) {
          array_span = base::span<const uint8_t>(
              reinterpret_cast<const uint8_t*>(data_ptr->DataMaybeShared()),
              data_ptr->ByteLength());
        }
      }
      break;
    case V8ImageBufferSource::ContentType::kArrayBufferViewAllowShared:
      if (auto* data_ptr =
              init->data()->GetAsArrayBufferViewAllowShared().Get()) {
        if (!data_ptr->IsDetached()) {
          array_span =
              base::span<const uint8_t>(reinterpret_cast<const uint8_t*>(
                                            data_ptr->BaseAddressMaybeShared()),
                                        data_ptr->byteLength());
        }
      }
      break;
    case V8ImageBufferSource::ContentType::kReadableStream:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  auto buffer_contents =
      TransferArrayBufferForSpan(init->transfer(), array_span, exception_state,
                                 script_state_->GetIsolate());
  if (exception_state.HadException()) {
    return;
  }

  if (array_span.empty()) {
    exception_state.ThrowTypeError("No image data provided");
    return;
  }

  scoped_refptr<SegmentReader> segment_reader;
  if (buffer_contents.IsValid()) {
    segment_reader = base::MakeRefCounted<ArrayBufferContentsSegmentReader>(
        std::move(buffer_contents));
  } else {
    segment_reader = SegmentReader::CreateFromSkData(
        SkData::MakeWithCopy(array_span.data(), array_span.size()));
  }

  if (!segment_reader) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to read image data");
    return;
  }

  construction_succeeded_ = true;
  data_complete_ = true;
  completed_property_->ResolveWithUndefined();
  decoder_ = std::make_unique<WTF::SequenceBound<ImageDecoderCore>>(
      decode_task_runner_, mime_type_, std::move(segment_reader),
      data_complete_, color_behavior, desired_size, animation_option_);

  DecodeMetadata();
}

ImageDecoderExternal::~ImageDecoderExternal() {
  DVLOG(1) << __func__;

  if (construction_succeeded_)
    base::UmaHistogramBoolean("Blink.WebCodecs.ImageDecoder.Success", !failed_);

  DCHECK_EQ(pending_metadata_requests_, 0);
}

ScriptPromise<ImageDecodeResult> ImageDecoderExternal::decode(
    const ImageDecodeOptions* options) {
  DVLOG(1) << __func__;
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<ImageDecodeResult>>(
          script_state_);
  auto promise = resolver->Promise();

  if (closed_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "The decoder has been closed."));
    return promise;
  }

  if (!decoder_) {
    resolver->Reject(CreateUnsupportedImageTypeException(mime_type_));
    return promise;
  }

  if (!tracks_->IsEmpty() && !tracks_->selectedTrack()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "No selected track."));
    return promise;
  }

  pending_decodes_.push_back(MakeGarbageCollected<DecodeRequest>(
      resolver, options ? options->frameIndex() : 0,
      options ? options->completeFramesOnly() : true));

  MaybeSatisfyPendingDecodes();
  return promise;
}

void ImageDecoderExternal::UpdateSelectedTrack() {
  DCHECK(!closed_);

  reset(MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                           "Aborted by track change"));

  // Track changes recreate a new decoder under the hood, so don't let stale
  // metadata updates come in for the newly selected (or no selected) track.
  weak_factory_.Invalidate();

  // TODO(crbug.com/1073995): We eventually need a formal track selection
  // mechanism. For now we can only select between the still and animated images
  // and must destruct the decoder for changes.
  if (!tracks_->selectedTrack()) {
    decoder_->AsyncCall(&ImageDecoderCore::Clear);
    return;
  }

  animation_option_ =
      AnimationOptionFromIsAnimated(tracks_->selectedTrack()->animated());

  decoder_->AsyncCall(&ImageDecoderCore::Reinitialize)
      .WithArgs(animation_option_);

  DecodeMetadata();
  MaybeSatisfyPendingDecodes();
}

String ImageDecoderExternal::type() const {
  return mime_type_;
}

bool ImageDecoderExternal::complete() const {
  return data_complete_;
}

ScriptPromise<IDLUndefined> ImageDecoderExternal::completed(
    ScriptState* script_state) {
  return completed_property_->Promise(script_state->World());
}

ImageTrackList& ImageDecoderExternal::tracks() const {
  return *tracks_;
}

void ImageDecoderExternal::reset(DOMException* exception) {
  if (!exception) {
    exception = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "Aborted by reset.");
  }

  num_submitted_decodes_ = 0u;
  decode_weak_factory_.Invalidate();

  // Move all state to local variables since promise resolution is re-entrant.
  HeapVector<Member<DecodeRequest>> local_pending_decodes;
  local_pending_decodes.swap(pending_decodes_);

  for (auto& request : local_pending_decodes) {
    request->resolver->Reject(exception);
    request->abort_flag->Set();

    // Since the AtomicFlag may still be referenced by the decoder sequence, we
    // need to delete it on that sequence.
    decode_task_runner_->DeleteSoon(FROM_HERE, std::move(request->abort_flag));
  }
}

void ImageDecoderExternal::close() {
  if (closed_)
    return;

  auto* exception = MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError,
      failed_ ? "Aborted by close." : "Aborted by failure.");

  // Failure cases should have already rejected the tracks ready promise.
  if (!failed_ && decoder_ && tracks_->IsEmpty())
    tracks_->OnTracksReady(exception);

  if (!data_complete_)
    completed_property_->Reject(exception);

  CloseInternal(exception);
}

void ImageDecoderExternal::CloseInternal(DOMException* exception) {
  reset(exception);
  if (consumer_)
    consumer_->Cancel();
  weak_factory_.Invalidate();
  pending_metadata_requests_ = 0;
  consumer_ = nullptr;
  decoder_.reset();
  tracks_->Disconnect();
  mime_type_ = "";
  closed_ = true;
}

void ImageDecoderExternal::OnStateChange() {
  DCHECK(!closed_);
  DCHECK(consumer_);

  const char* buffer;
  size_t available;
  while (!internal_data_complete_) {
    auto result = consumer_->BeginRead(&buffer, &available);
    if (result == BytesConsumer::Result::kShouldWait)
      return;

    std::unique_ptr<uint8_t[]> data;
    if (result == BytesConsumer::Result::kOk) {
      if (available > 0) {
        data.reset(new uint8_t[available]);
        memcpy(data.get(), buffer, available);
        bytes_read_ += available;
      }
      result = consumer_->EndRead(available);
    }

    const bool data_complete = result == BytesConsumer::Result::kDone ||
                               result == BytesConsumer::Result::kError;
    if (available > 0 || data_complete != internal_data_complete_) {
      decoder_->AsyncCall(&ImageDecoderCore::AppendData)
          .WithArgs(available, std::move(data), data_complete);
      // Note: Requiring a selected track to DecodeMetadata() means we won't
      // resolve completed if all data comes in while there's no selected
      // track. This is intentional since if we resolve completed while there's
      // no underlying decoder, we may signal completed while the tracks have
      // out of date metadata in them.
      if (tracks_->IsEmpty() || tracks_->selectedTrack()) {
        DecodeMetadata();
        MaybeSatisfyPendingDecodes();
      }
    }
    internal_data_complete_ = data_complete;
  }
}

String ImageDecoderExternal::DebugName() const {
  return "ImageDecoderExternal";
}

void ImageDecoderExternal::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(consumer_);
  visitor->Trace(tracks_);
  visitor->Trace(pending_decodes_);
  visitor->Trace(completed_property_);
  visitor->Trace(decode_weak_factory_);
  visitor->Trace(weak_factory_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void ImageDecoderExternal::ContextDestroyed() {
  auto* exception = MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Aborted by close.");
  CloseInternal(exception);
}

bool ImageDecoderExternal::HasPendingActivity() const {
  const bool has_pending_activity =
      !pending_decodes_.empty() || pending_metadata_requests_ > 0;
  return has_pending_activity;
}

void ImageDecoderExternal::MaybeSatisfyPendingDecodes() {
  DCHECK(!closed_);
  DCHECK(decoder_);
  DCHECK(failed_ || tracks_->IsEmpty() || tracks_->selectedTrack());

  for (auto& request : pending_decodes_) {
    if (failed_) {
      request->exception = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kEncodingError,
          String::Format("Failed to decode frame at index %d",
                         request->frame_index));
      continue;
    }

    // Ignore already submitted requests and those already satisfied.
    if (request->pending || request->IsFinal())
      continue;

    if (!data_complete_) {
      // When data is incomplete, we must process requests one at a time since
      // we don't know if a given request can be satisfied yet and don't want to
      // fulfill requests out of order.
      if (num_submitted_decodes_ > 0u)
        break;

      // If no data has arrived since we last tried submitting this decode
      // request, do nothing until more data arrives.
      if (request->bytes_read_index && request->bytes_read_index == bytes_read_)
        break;
    }

    request->pending = true;
    request->bytes_read_index = bytes_read_;

    ++num_submitted_decodes_;
    decoder_->AsyncCall(&ImageDecoderCore::Decode)
        .WithArgs(request->frame_index, request->complete_frames_only,
                  WTF::CrossThreadUnretained(request->abort_flag.get()))
        .Then(CrossThreadBindOnce(&ImageDecoderExternal::OnDecodeReady,
                                  MakeUnwrappingCrossThreadHandle(
                                      decode_weak_factory_.GetWeakCell())));
  }

  auto new_end = std::stable_partition(
      pending_decodes_.begin(), pending_decodes_.end(),
      [](const auto& request) { return !request->IsFinal(); });

  // Copy completed requests to a new local vector to avoid reentrancy issues
  // when resolving and rejecting the promises.
  HeapVector<Member<DecodeRequest>> completed_decodes;
  completed_decodes.AppendRange(new_end, pending_decodes_.end());
  pending_decodes_.Shrink(
      static_cast<wtf_size_t>(new_end - pending_decodes_.begin()));

  // Note: Promise resolution may invoke calls into this class.
  for (auto& request : completed_decodes) {
    DCHECK(!request->abort_flag->IsSet());
    if (request->exception) {
      request->resolver->Reject(request->exception);
    } else if (request->range_error_message) {
      ScriptState::Scope scope(script_state_);
      request->resolver->Reject(V8ThrowException::CreateRangeError(
          script_state_->GetIsolate(), *request->range_error_message));
    } else {
      request->resolver->Resolve(request->result);
    }

    // Since the AtomicFlag may still be referenced by the decoder sequence, we
    // need to delete it on that sequence.
    decode_task_runner_->DeleteSoon(FROM_HERE, std::move(request->abort_flag));
  }
}

void ImageDecoderExternal::OnDecodeReady(
    std::unique_ptr<ImageDecoderCore::ImageDecodeResult> result) {
  DCHECK(decoder_);
  DCHECK(!closed_);
  DCHECK(result);
  DCHECK(!pending_decodes_.empty());

  auto& request = pending_decodes_.front();
  DCHECK_EQ(request->frame_index, result->frame_index);
  --num_submitted_decodes_;

  if (result->status == ImageDecoderCore::Status::kDecodeError || failed_) {
    SetFailed();
    return;
  }

  request->pending = false;

  // Abort always invalidates WeakCells, so OnDecodeReady() should never receive
  // the kAborted status.
  DCHECK_NE(result->status, ImageDecoderCore::Status::kAborted);

  if (result->status == ImageDecoderCore::Status::kIndexError) {
    request->range_error_message =
        ExceptionMessages::IndexOutsideRange<uint32_t>(
            "frame index", request->frame_index, 0,
            ExceptionMessages::kInclusiveBound,
            tracks_->selectedTrack()->frameCount(),
            ExceptionMessages::kExclusiveBound);
    MaybeSatisfyPendingDecodes();
    return;
  }

  // If there was nothing to decode yet or no new image, try again; this will do
  // nothing if no new data has been received since the last submitted request.
  if (result->status == ImageDecoderCore::Status::kNoImage) {
    // Once we're data complete, if no further image can be decoded, we should
    // reject the decode() since it can't be satisfied.
    if (data_complete_) {
      request->range_error_message = String::Format(
          "Unexpected end of image. Request for frame index %d "
          "can't be satisfied.",
          request->frame_index);
    }

    MaybeSatisfyPendingDecodes();
    return;
  }

  request->result = ImageDecodeResult::Create();
  request->result->setImage(
      MakeGarbageCollected<VideoFrame>(base::MakeRefCounted<VideoFrameHandle>(
          std::move(result->frame), std::move(result->sk_image))));
  request->result->setComplete(result->complete);
  MaybeSatisfyPendingDecodes();
}

void ImageDecoderExternal::DecodeMetadata() {
  DCHECK(decoder_);
  DCHECK(tracks_->IsEmpty() || tracks_->selectedTrack());

  ++pending_metadata_requests_;
  DCHECK_GE(pending_metadata_requests_, 1);

  decoder_->AsyncCall(&ImageDecoderCore::DecodeMetadata)
      .Then(CrossThreadBindOnce(
          &ImageDecoderExternal::OnMetadata,
          MakeUnwrappingCrossThreadHandle(weak_factory_.GetWeakCell())));
}

void ImageDecoderExternal::OnMetadata(
    ImageDecoderCore::ImageMetadata metadata) {
  DCHECK(decoder_);
  DCHECK(!closed_);

  --pending_metadata_requests_;
  DCHECK_GE(pending_metadata_requests_, 0);

  const bool did_complete = !data_complete_ && metadata.data_complete;

  // Set public value before resolving.
  data_complete_ = metadata.data_complete;
  if (did_complete)
    completed_property_->ResolveWithUndefined();

  if (metadata.failed || failed_) {
    SetFailed();
    return;
  }

  // If we don't have size metadata yet, don't attempt to setup the tracks since
  // we also won't have a reliable frame count. A later call to DecodeMetadata()
  // will be made as bytes come in.
  if (!metadata.has_size) {
    DCHECK(!data_complete_);
    return;
  }

  if (!tracks_->IsEmpty()) {
    tracks_->selectedTrack()->UpdateTrack(metadata.frame_count,
                                          metadata.repetition_count);
    if (did_complete)
      MaybeSatisfyPendingDecodes();
    return;
  }

  // TODO(crbug.com/1073995): None of the underlying ImageDecoders actually
  // expose tracks yet. So for now just assume a still and animated track for
  // images which declare to be multi-image and have animations.

  if (metadata.image_has_both_still_and_animated_sub_images) {
    int selected_track_id = 1;  // Currently animation is always default.
    if (prefer_animation_.has_value()) {
      selected_track_id = prefer_animation_.value() ? 1 : 0;

      // Sadly there's currently no way to get the frame count information for
      // unselected tracks, so for now just leave frame count as unknown but
      // force repetition count to be animated.
      if (!prefer_animation_.value()) {
        metadata.frame_count = 0;
        metadata.repetition_count = kAnimationLoopOnce;
      }
    }

    // All multi-track images have a still image track. Even if it's just the
    // first frame of the animation.
    tracks_->AddTrack(1, kAnimationNone, selected_track_id == 0);
    tracks_->AddTrack(metadata.frame_count, metadata.repetition_count,
                      selected_track_id == 1);
  } else {
    tracks_->AddTrack(metadata.frame_count, metadata.repetition_count, true);
  }

  tracks_->OnTracksReady();
  if (did_complete)
    MaybeSatisfyPendingDecodes();
}

void ImageDecoderExternal::SetFailed() {
  DVLOG(1) << __func__;
  if (failed_) {
    DCHECK(pending_decodes_.empty());
    return;
  }

  failed_ = true;
  decode_weak_factory_.Invalidate();
  if (tracks_->IsEmpty()) {
    tracks_->OnTracksReady(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to retrieve track metadata."));
  }
  MaybeSatisfyPendingDecodes();
  close();
}

}  // namespace blink
