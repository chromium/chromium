// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/image_decoder_external.h"

#include <limits>

#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_track.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

// static
ImageDecoderExternal* ImageDecoderExternal::Create(
    ScriptState* script_state,
    const ImageDecoderInit* init,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<ImageDecoderExternal>(script_state, init,
                                                    exception_state);
}

ImageDecoderExternal::DecodeRequest::DecodeRequest(
    ScriptPromiseResolver* resolver,
    uint32_t frame_index,
    bool complete_frames_only)
    : resolver(resolver),
      frame_index(frame_index),
      complete_frames_only(complete_frames_only) {}

void ImageDecoderExternal::DecodeRequest::Trace(Visitor* visitor) const {
  visitor->Trace(resolver);
  visitor->Trace(result);
  visitor->Trace(exception);
}

// static
bool ImageDecoderExternal::canDecodeType(String type) {
  return type.ContainsOnlyASCIIOrEmpty() &&
         IsSupportedImageMimeType(type.Ascii());
}

ImageDecoderExternal::ImageDecoderExternal(ScriptState* script_state,
                                           const ImageDecoderInit* init,
                                           ExceptionState& exception_state)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      script_state_(script_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);

  // |data| is a required field.
  DCHECK(init->hasData());
  DCHECK(!init->data().IsNull());

  options_ =
      init->hasOptions() ? init->options() : ImageBitmapOptions::Create();

  mime_type_ = init->type();
  if (!canDecodeType(mime_type_)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Unsupported image format");
    return;
  }

  if (init->hasPreferAnimation())
    prefer_animation_ = init->preferAnimation();

  if (init->data().IsReadableStream()) {
    if (init->data().GetAsReadableStream()->IsLocked()) {
      exception_state.ThrowTypeError(
          "ImageDecoder can only accept readable streams that are not yet "
          "locked to a reader");
      return;
    }
    consumer_ = MakeGarbageCollected<ReadableStreamBytesConsumer>(
        script_state, init->data().GetAsReadableStream());

    stream_buffer_ = WTF::SharedBuffer::Create();
    CreateImageDecoder();

    // We need one initial call to OnStateChange() to start reading, but
    // thereafter calls will be driven by the ReadableStreamBytesConsumer.
    consumer_->SetClient(this);
    OnStateChange();
    return;
  }

  // Since we don't make a copy of buffer passed in, we must retain a reference.
  init_data_ = init;

  DOMArrayPiece buffer;
  if (init->data().IsArrayBuffer()) {
    buffer = DOMArrayPiece(init->data().GetAsArrayBuffer());
  } else if (init->data().IsArrayBufferView()) {
    buffer = DOMArrayPiece(init->data().GetAsArrayBufferView().View());
  } else {
    NOTREACHED();
    return;
  }

  if (!buffer.ByteLength()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "No image data provided");
    return;
  }

  // Since data is owned by the caller who may be free to manipulate it, we must
  // check HasValidEncodedData() before attempting to access |decoder_|.
  segment_reader_ = SegmentReader::CreateFromSkData(
      SkData::MakeWithoutCopy(buffer.Data(), buffer.ByteLength()));
  if (!segment_reader_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "Failed to read image data");
    return;
  }

  data_complete_ = true;

  CreateImageDecoder();
  MaybeUpdateMetadata();
  if (decoder_->Failed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Image decoding failed");
    return;
  }
}

ImageDecoderExternal::~ImageDecoderExternal() {
  DVLOG(1) << __func__;
}

ScriptPromise ImageDecoderExternal::decode(uint32_t frame_index,
                                           bool complete_frames_only) {
  DVLOG(1) << __func__;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  auto promise = resolver->Promise();
  pending_decodes_.push_back(MakeGarbageCollected<DecodeRequest>(
      resolver, frame_index, complete_frames_only));
  MaybeSatisfyPendingDecodes();
  return promise;
}

ScriptPromise ImageDecoderExternal::decodeMetadata() {
  DVLOG(1) << __func__;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  auto promise = resolver->Promise();
  pending_metadata_decodes_.push_back(resolver);
  MaybeSatisfyPendingMetadataDecodes();
  return promise;
}

void ImageDecoderExternal::selectTrack(uint32_t track_id,
                                       ExceptionState& exception_state) {
  if (track_id >= tracks_.size()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "Track index out of range");
    return;
  }

  // Returning early allows us to avoid churn from unnecessarily destructing the
  // underlying ImageDecoder interface.
  if (tracks_.size() == 1 || selected_track_id_ == track_id)
    return;

  for (auto& request : pending_decodes_) {
    request->resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "Aborted by track change"));
  }

  pending_decodes_.clear();
  incomplete_frames_.clear();

  // TODO(crbug.com/1073995): We eventually need a formal track selection
  // mechanism. For now we can only select between the still and animated images
  // and must destruct the decoder for changes.
  decoder_.reset();
  selected_track_id_ = track_id;
  prefer_animation_ = tracks_[track_id]->animated();

  CreateImageDecoder();
  MaybeUpdateMetadata();
  MaybeSatisfyPendingDecodes();
}

uint32_t ImageDecoderExternal::frameCount() const {
  return frame_count_;
}

String ImageDecoderExternal::type() const {
  return mime_type_;
}

uint32_t ImageDecoderExternal::repetitionCount() const {
  return repetition_count_;
}

bool ImageDecoderExternal::complete() const {
  return data_complete_;
}

const ImageDecoderExternal::ImageTrackList ImageDecoderExternal::tracks()
    const {
  return tracks_;
}

void ImageDecoderExternal::OnStateChange() {
  const char* buffer;
  size_t available;
  while (!data_complete_) {
    auto result = consumer_->BeginRead(&buffer, &available);
    if (result == BytesConsumer::Result::kShouldWait)
      return;

    if (result == BytesConsumer::Result::kOk) {
      if (available > 0)
        stream_buffer_->Append(buffer, SafeCast<wtf_size_t>(available));
      result = consumer_->EndRead(available);
    }

    if (result == BytesConsumer::Result::kError) {
      data_complete_ = true;
      return;
    }

    data_complete_ = result == BytesConsumer::Result::kDone;
    decoder_->SetData(stream_buffer_, data_complete_);

    MaybeUpdateMetadata();
    MaybeSatisfyPendingDecodes();
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
  visitor->Trace(pending_metadata_decodes_);
  visitor->Trace(init_data_);
  visitor->Trace(options_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void ImageDecoderExternal::ContextDestroyed() {}

bool ImageDecoderExternal::HasPendingActivity() const {
  return !pending_metadata_decodes_.IsEmpty() || !pending_decodes_.IsEmpty();
}

void ImageDecoderExternal::CreateImageDecoder() {
  DCHECK(!decoder_);
  DCHECK(HasValidEncodedData());

  // TODO(crbug.com/1073995): We should probably call
  // ImageDecoder::SetMemoryAllocator() so that we can recycle frame buffers for
  // decoded images.

  constexpr char kNoneOption[] = "none";

  auto color_behavior = ColorBehavior::Tag();
  if (options_->colorSpaceConversion() == kNoneOption)
    color_behavior = ColorBehavior::Ignore();

  auto premultiply_alpha = ImageDecoder::kAlphaPremultiplied;
  if (options_->premultiplyAlpha() == kNoneOption)
    premultiply_alpha = ImageDecoder::kAlphaNotPremultiplied;

  // TODO(crbug.com/1073995): Is it okay to use resize size like this?
  auto desired_size = SkISize::MakeEmpty();
  if (options_->hasResizeWidth() && options_->hasResizeHeight()) {
    desired_size =
        SkISize::Make(options_->resizeWidth(), options_->resizeHeight());
  }

  if (stream_buffer_) {
    if (!segment_reader_)
      segment_reader_ = SegmentReader::CreateFromSharedBuffer(stream_buffer_);
  } else {
    DCHECK(data_complete_);
  }

  DCHECK(canDecodeType(mime_type_));
  decoder_ = ImageDecoder::CreateByMimeType(
      mime_type_, segment_reader_, data_complete_, premultiply_alpha,
      ImageDecoder::kHighBitDepthToHalfFloat, color_behavior, desired_size);

  // CreateByImageType() can't fail if we use a supported image type. Which we
  // DCHECK above via canDecodeType().
  DCHECK(decoder_);
}

void ImageDecoderExternal::MaybeSatisfyPendingDecodes() {
  DCHECK(decoder_);
  for (auto& request : pending_decodes_) {
    if (!data_complete_) {
      // We can't fulfill this promise at this time.
      if (request->frame_index >= frame_count_)
        continue;
    } else if (request->frame_index >= frame_count_) {
      // TODO(crbug.com/1073995): Include frameIndex in rejection?
      request->exception = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kConstraintError, "Frame index out of range");
      continue;
    }

    if (!HasValidEncodedData()) {
      request->exception = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          "Source data has been neutered");
      continue;
    }

    auto* image = decoder_->DecodeFrameBufferAtIndex(request->frame_index);
    if (decoder_->Failed() || !image) {
      // TODO(crbug.com/1073995): Include frameIndex in rejection?
      request->exception = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kConstraintError, "Failed to decode frame");
      continue;
    }

    // Only satisfy fully complete decode requests.
    const bool is_complete = image->GetStatus() == ImageFrame::kFrameComplete;
    if (!is_complete && request->complete_frames_only)
      continue;

    if (!is_complete && image->GetStatus() != ImageFrame::kFramePartial)
      continue;

    // Prefer FinalizePixelsAndGetImage() since that will mark the underlying
    // bitmap as immutable, which allows copies to be avoided.
    auto sk_image = is_complete ? image->FinalizePixelsAndGetImage()
                                : SkImage::MakeFromBitmap(image->Bitmap());
    if (!sk_image) {
      // TODO(crbug.com/1073995): Include frameIndex in rejection?
      request->exception = MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, "Failed to decode frame");
      continue;
    }

    if (!is_complete) {
      auto generation_id = image->Bitmap().getGenerationID();
      auto it = incomplete_frames_.find(request->frame_index);
      if (it == incomplete_frames_.end()) {
        incomplete_frames_.Set(request->frame_index, generation_id);
      } else {
        // Don't fulfill the promise until a new bitmap is seen.
        if (it->value == generation_id)
          continue;

        it->value = generation_id;
      }
    } else {
      incomplete_frames_.erase(request->frame_index);
    }

    auto* result = ImageFrameExternal::Create();
    result->setImage(MakeGarbageCollected<ImageBitmap>(
        UnacceleratedStaticBitmapImage::Create(std::move(sk_image),
                                               decoder_->Orientation()),
        base::nullopt, options_));
    result->setDuration(
        decoder_->FrameDurationAtIndex(request->frame_index).InMicroseconds());
    result->setOrientation(
        static_cast<uint32_t>(decoder_->Orientation().Orientation()));
    result->setComplete(is_complete);
    request->result = result;
  }

  auto* new_end =
      std::stable_partition(pending_decodes_.begin(), pending_decodes_.end(),
                            [](const auto& request) {
                              return !request->result && !request->exception;
                            });

  // Copy completed requests to a new local vector to avoid reentrancy issues
  // when resolving and rejecting the promises.
  HeapVector<Member<DecodeRequest>> completed_decodes;
  completed_decodes.AppendRange(new_end, pending_decodes_.end());
  pending_decodes_.Shrink(
      static_cast<wtf_size_t>(new_end - pending_decodes_.begin()));

  // Note: Promise resolution may invoke calls into this class.
  for (auto& request : completed_decodes) {
    if (request->exception)
      request->resolver->Reject(request->exception);
    else
      request->resolver->Resolve(request->result);
  }
}

void ImageDecoderExternal::MaybeSatisfyPendingMetadataDecodes() {
  DCHECK(HasValidEncodedData());
  DCHECK(decoder_);
  if (!decoder_->IsSizeAvailable() && !decoder_->Failed())
    return;

  DCHECK(decoder_->Failed() || decoder_->IsDecodedSizeAvailable());
  for (auto& resolver : pending_metadata_decodes_)
    resolver->Resolve();
  pending_metadata_decodes_.clear();
}

void ImageDecoderExternal::MaybeUpdateMetadata() {
  if (!HasValidEncodedData())
    return;

  // Since we always create the decoder at construction, we need to wait until
  // at least the size is available before signaling that metadata has been
  // retrieved.
  if (!decoder_->IsSizeAvailable() || decoder_->Failed()) {
    MaybeSatisfyPendingMetadataDecodes();
    return;
  }

  const size_t decoded_frame_count = decoder_->FrameCount();
  if (decoder_->Failed()) {
    MaybeSatisfyPendingMetadataDecodes();
    return;
  }

  frame_count_ = static_cast<uint32_t>(decoded_frame_count);

  // The internal value has some magic negative numbers; for external purposes
  // we want to only surface positive repetition counts. The rest is up to the
  // client.
  const int decoded_repetition_count = decoder_->RepetitionCount();
  if (decoded_repetition_count > 0)
    repetition_count_ = decoded_repetition_count;

  // TODO(crbug.com/1073995): None of the underlying ImageDecoders actually
  // expose tracks yet. So for now just assume a still and animated track for
  // images which declare to be multi-image and have animations.
  if (tracks_.IsEmpty()) {
    auto* track = ImageTrackExternal::Create();
    track->setId(0);
    tracks_.push_back(track);

    if (decoder_->ImageHasBothStillAndAnimatedSubImages()) {
      track->setAnimated(false);

      // All multi-track images have a still image track. Even if it's just the
      // first frame of the animation.
      track = ImageTrackExternal::Create();
      track->setId(1);
      track->setAnimated(true);
      tracks_.push_back(track);

      if (prefer_animation_.has_value())
        selected_track_id_ = prefer_animation_.value() ? 1 : 0;
    } else {
      track->setAnimated(frame_count_ > 1);
      selected_track_id_ = 0;
    }
  }

  MaybeSatisfyPendingMetadataDecodes();
}

bool ImageDecoderExternal::HasValidEncodedData() const {
  // If we keep an internal copy of the data, it's always valid.
  if (stream_buffer_)
    return true;

  if (init_data_->data().IsArrayBuffer() &&
      init_data_->data().GetAsArrayBuffer()->IsDetached()) {
    return false;
  }

  if (init_data_->data().IsArrayBufferView() &&
      !init_data_->data().GetAsArrayBufferView()->BaseAddress()) {
    return false;
  }

  return true;
}

}  // namespace blink
