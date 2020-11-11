/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/mediasource/source_buffer.h"

#include <limits>
#include <memory>
#include <sstream>
#include <utility>

#include "media/base/logging_override_if_enabled.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_source_buffer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/core/html/track/audio_track.h"
#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/html/track/video_track.h"
#include "third_party/blink/renderer/core/html/track/video_track_list.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer_track_base_supplement.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

using blink::WebSourceBuffer;

namespace blink {

namespace {

static bool ThrowExceptionIfRemovedOrUpdating(bool is_removed,
                                              bool is_updating,
                                              ExceptionState& exception_state) {
  if (is_removed) {
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "This SourceBuffer has been removed from the parent media source.");
    return true;
  }
  if (is_updating) {
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "This SourceBuffer is still processing an 'appendBuffer' or "
        "'remove' operation.");
    return true;
  }

  return false;
}

WTF::String WebTimeRangesToString(const WebTimeRanges& ranges) {
  StringBuilder string_builder;
  string_builder.Append('{');
  for (auto& r : ranges) {
    string_builder.Append(" [");
    string_builder.AppendNumber(r.start);
    string_builder.Append(';');
    string_builder.AppendNumber(r.end);
    string_builder.Append(']');
  }
  string_builder.Append(" }");
  return string_builder.ToString();
}

}  // namespace

SourceBuffer::SourceBuffer(std::unique_ptr<WebSourceBuffer> web_source_buffer,
                           MediaSource* source,
                           EventQueue* async_event_queue)
    : ExecutionContextLifecycleObserver(source->GetExecutionContext()),
      web_source_buffer_(std::move(web_source_buffer)),
      source_(source),
      track_defaults_(MakeGarbageCollected<TrackDefaultList>()),
      async_event_queue_(async_event_queue),
      mode_(SegmentsKeyword()),
      updating_(false),
      timestamp_offset_(0),
      append_window_start_(0),
      append_window_end_(std::numeric_limits<double>::infinity()),
      first_initialization_segment_received_(false),
      pending_append_data_offset_(0),
      pending_remove_start_(-1),
      pending_remove_end_(-1) {
  DVLOG(1) << __func__ << " this=" << this;

  DCHECK(web_source_buffer_);
  DCHECK(source_);

  scoped_refptr<MediaSourceAttachmentSupplement> attachment;
  MediaSourceTracer* tracer;
  std::tie(attachment, tracer) = source_->AttachmentAndTracer();
  DCHECK(attachment);

  if (GetExecutionContext()->IsWindow()) {
    DCHECK(IsMainThread());
    DCHECK(tracer);  // Same-thread attachments must use a tracer.

    // Have the attachment construct our audio and video tracklist members for
    // us, since it knows how to do this with knowledge of the attached media
    // element.
    audio_tracks_ = attachment->CreateAudioTrackList(tracer);
    DCHECK(audio_tracks_);
    video_tracks_ = attachment->CreateVideoTrackList(tracer);
    DCHECK(video_tracks_);
  } else {
    DCHECK(RuntimeEnabledFeatures::MediaSourceInWorkersEnabled() &&
           GetExecutionContext()->IsDedicatedWorkerGlobalScope());
    DCHECK(!IsMainThread());

    // TODO(https://crbug.com/878133): Enable construction of media tracks that
    // don't reference the media element if, for instance, they are owned by a
    // different execution context. For now, AudioVideoTracks experimental
    // feature implementation is not complete when MediaSource is in worker.
    DCHECK(!audio_tracks_);
    DCHECK(!video_tracks_);
  }

  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();
  web_source_buffer_->SetClient(this);
}

SourceBuffer::~SourceBuffer() {
  DVLOG(1) << __func__ << " this=" << this;
}

void SourceBuffer::Dispose() {
  // Promptly clears a raw reference from content/ to an on-heap object
  // so that content/ doesn't access it in a lazy sweeping phase.
  web_source_buffer_.reset();
}

AtomicString SourceBuffer::SegmentsKeyword() {
  return "segments";
}

AtomicString SourceBuffer::SequenceKeyword() {
  return "sequence";
}

void SourceBuffer::setMode(const AtomicString& new_mode,
                           ExceptionState& exception_state) {
  DVLOG(3) << __func__ << " this=" << this << " new_mode=" << new_mode;

  // Section 3.1 On setting mode attribute steps.
  // https://www.w3.org/TR/media-source/#dom-sourcebuffer-mode
  // 1. If this object has been removed from the sourceBuffers attribute of the
  //    parent media source, then throw an INVALID_STATE_ERR exception and abort
  //    these steps.
  // 2. If the updating attribute equals true, then throw an INVALID_STATE_ERR
  //    exception and abort these steps.
  // 3. Let new mode equal the new value being assigned to this attribute.
  if (ThrowExceptionIfRemovedOrUpdating(IsRemoved(), updating_,
                                        exception_state)) {
    return;
  }

  // Do remainder of steps only if attachment is usable and underlying demuxer
  // is protected from destruction (applicable especially for MSE-in-Worker
  // case). Note, we must have |source_| and |source_| must have an attachment
  // because !IsRemoved().
  if (!source_->RunUnlessElementGoneOrClosingUs(
          WTF::Bind(&SourceBuffer::SetMode_Locked, WrapPersistent(this),
                    new_mode, WTF::Unretained(&exception_state)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "Worker MediaSource attachment is closing");
  }
}

void SourceBuffer::SetMode_Locked(
    AtomicString new_mode,
    ExceptionState* exception_state,
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  DCHECK(source_);
  DCHECK(!updating_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // 4. If generate timestamps flag equals true and new mode equals "segments",
  //    then throw a TypeError exception and abort these steps.
  if (web_source_buffer_->GetGenerateTimestampsFlag() &&
      new_mode == SegmentsKeyword()) {
    MediaSource::LogAndThrowTypeError(
        *exception_state, "The mode value provided (" + SegmentsKeyword() +
                              ") is invalid for a byte stream format that uses "
                              "generated timestamps.");
    return;
  }

  // 5. If the readyState attribute of the parent media source is in the "ended"
  //    state then run the following steps:
  // 5.1 Set the readyState attribute of the parent media source to "open"
  // 5.2 Queue a task to fire a simple event named sourceopen at the parent
  //     media source.
  source_->OpenIfInEndedState();

  // 6. If the append state equals PARSING_MEDIA_SEGMENT, then throw an
  //    INVALID_STATE_ERR and abort these steps.
  // 7. If the new mode equals "sequence", then set the group start timestamp to
  //    the highest presentation end timestamp.
  WebSourceBuffer::AppendMode append_mode =
      WebSourceBuffer::kAppendModeSegments;
  if (new_mode == SequenceKeyword())
    append_mode = WebSourceBuffer::kAppendModeSequence;
  if (!web_source_buffer_->SetMode(append_mode)) {
    MediaSource::LogAndThrowDOMException(
        *exception_state, DOMExceptionCode::kInvalidStateError,
        "The mode may not be set while the SourceBuffer's append state is "
        "'PARSING_MEDIA_SEGMENT'.");
    return;
  }

  // 8. Update the attribute to new mode.
  mode_ = new_mode;
}

TimeRanges* SourceBuffer::buffered(ExceptionState& exception_state) const {
  // Section 3.1 buffered attribute steps.
  // 1. If this object has been removed from the sourceBuffers attribute of the
  //    parent media source then throw an InvalidStateError exception and abort
  //    these steps.
  if (IsRemoved()) {
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "This SourceBuffer has been removed from the parent media source.");
    return nullptr;
  }

  // Obtain the current buffered ranges only if attachment is usable and
  // underlying demuxer is protected from destruction (applicable especially for
  // MSE-in-Worker case). Note, we must have |source_| and |source_| must have
  // an attachment because !IsRemoved().
  WebTimeRanges ranges;
  if (!source_->RunUnlessElementGoneOrClosingUs(
          WTF::Bind(&SourceBuffer::GetBuffered_Locked, WrapPersistent(this),
                    WTF::Unretained(&ranges)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "Worker MediaSource attachment is closing");
    return nullptr;
  }

  // 2. Return a new static normalized TimeRanges object for the media segments
  //    buffered.
  return MakeGarbageCollected<TimeRanges>(ranges);
}

void SourceBuffer::GetBuffered_Locked(
    WebTimeRanges* ranges /* out parameter */,
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) const {
  DCHECK(!IsRemoved());
  DCHECK(ranges);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  *ranges = web_source_buffer_->Buffered();
}

double SourceBuffer::timestampOffset() const {
  return timestamp_offset_;
}

void SourceBuffer::setTimestampOffset(double offset,
                                      ExceptionState& exception_state) {
  DVLOG(3) << __func__ << " this=" << this << " offset=" << offset;
  // Section 3.1 timestampOffset attribute setter steps.
  // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-SourceBuffer-timestampOffset
  // 1. Let new timestamp offset equal the new value being assigned to this
  //    attribute.
  // 2. If this object has been removed from the sourceBuffers attribute of the
  //    parent media source, then throw an InvalidStateError exception and abort
  //    these steps.
  // 3. If the updating attribute equals true, then throw an InvalidStateError
  //    exception and abort these steps.
  if (ThrowExceptionIfRemovedOrUpdating(IsRemoved(), updating_,
                                        exception_state))
    return;

  // Do the remainder of steps only if attachment is usable and underlying
  // demuxer is protected from destruction (applicable especially for
  // MSE-in-Worker case). Note, we must have |source_| and |source_| must have
  // an attachment because !IsRemoved().
  if (!source_->RunUnlessElementGoneOrClosingUs(WTF::Bind(
          &SourceBuffer::SetTimestampOffset_Locked, WrapPersistent(this),
          offset, WTF::Unretained(&exception_state)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "Worker MediaSource attachment is closing");
  }
}

void SourceBuffer::SetTimestampOffset_Locked(
    double offset,
    ExceptionState* exception_state,
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  DCHECK(source_);
  DCHECK(!updating_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // 4. If the readyState attribute of the parent media source is in the "ended"
  //    state then run the following steps:
  // 4.1 Set the readyState attribute of the parent media source to "open"
  // 4.2 Queue a task to fire a simple event named sourceopen at the parent
  //     media source.
  source_->OpenIfInEndedState();

  // 5. If the append state equals PARSING_MEDIA_SEGMENT, then throw an
  //    INVALID_STATE_ERR and abort these steps.
  // 6. If the mode attribute equals "sequence", then set the group start
  //    timestamp to new timestamp offset.
  if (!web_source_buffer_->SetTimestampOffset(offset)) {
    MediaSource::LogAndThrowDOMException(
        *exception_state, DOMExceptionCode::kInvalidStateError,
        "The timestamp offset may not be set while the SourceBuffer's append "
        "state is 'PARSING_MEDIA_SEGMENT'.");
    return;
  }

  // 7. Update the attribute to new timestamp offset.
  timestamp_offset_ = offset;
}

AudioTrackList& SourceBuffer::audioTracks() {
  DCHECK(HTMLMediaElement::MediaTracksEnabledInternally());

  // TODO(https://crbug.com/878133): Complete the AudioVideoTracks function
  // necessary to enable successful experimental usage of it when MSE is in
  // worker. Note that if this is consulted as part of parent |source_|'s
  // context destruction, then we cannot consult GetExecutionContext() here.
  CHECK(IsMainThread());

  return *audio_tracks_;
}

VideoTrackList& SourceBuffer::videoTracks() {
  DCHECK(HTMLMediaElement::MediaTracksEnabledInternally());

  // TODO(https://crbug.com/878133): Complete the AudioVideoTracks function
  // necessary to enable successful experimental usage of it when MSE is in
  // worker. Note that if this is consulted as part of parent |source_|'s
  // context destruction, then we cannot consult GetExecutionContext() here.
  CHECK(IsMainThread());

  return *video_tracks_;
}

double SourceBuffer::appendWindowStart() const {
  return append_window_start_;
}

void SourceBuffer::setAppendWindowStart(double start,
                                        ExceptionState& exception_state) {
  DVLOG(3) << __func__ << " this=" << this << " start=" << start;
  // Section 3.1 appendWindowStart attribute setter steps.
  // https://www.w3.org/TR/media-source/#widl-SourceBuffer-appendWindowStart
  // 1. If this object has been removed from the sourceBuffers attribute of the
  //    parent media source then throw an InvalidStateError exception and abort
  //    these steps.
  // 2. If the updating attribute equals true, then throw an InvalidStateError
  //    exception and abort these steps.
  if (ThrowExceptionIfRemovedOrUpdating(IsRemoved(), updating_,
                                        exception_state))
    return;

  // 3. If the new value is less than 0 or greater than or equal to
  //    appendWindowEnd then throw a TypeError exception and abort these steps.
  if (start < 0 || start >= append_window_end_) {
    MediaSource::LogAndThrowTypeError(
        exception_state,
        ExceptionMessages::IndexOutsideRange(
            "value", start, 0.0, ExceptionMessages::kExclusiveBound,
            append_window_end_, ExceptionMessages::kInclusiveBound));
    return;
  }

  // Do remainder of steps only if attachment is usable and underlying demuxer
  // is protected from destruction (applicable especially for MSE-in-Worker
  // case). Note, we must have |source_| and |source_| must have an attachment
  // because !IsRemoved().
  if (!source_->RunUnlessElementGoneOrClosingUs(
          WTF::Bind(&SourceBuffer::SetAppendWindowStart_Locked,
                    WrapPersistent(this), start))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "Worker MediaSource attachment is closing");
  }
}

void SourceBuffer::SetAppendWindowStart_Locked(
    double start,
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  DCHECK(source_);
  DCHECK(!updating_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // 4. Update the attribute to the new value.
  web_source_buffer_->SetAppendWindowStart(start);
  append_window_start_ = start;
}

double SourceBuffer::appendWindowEnd() const {
  return append_window_end_;
}

void SourceBuffer::setAppendWindowEnd(double end,
                                      ExceptionState& exception_state) {
  DVLOG(3) << __func__ << " this=" << this << " end=" << end;
  // Section 3.1 appendWindowEnd attribute setter steps.
  // https://www.w3.org/TR/media-source/#widl-SourceBuffer-appendWindowEnd
  // 1. If this object has been removed from the sourceBuffers attribute of the
  //    parent media source then throw an InvalidStateError exception and abort
  //    these steps.
  // 2. If the updating attribute equals true, then throw an InvalidStateError
  //    exception and abort these steps.
  if (ThrowExceptionIfRemovedOrUpdating(IsRemoved(), updating_,
                                        exception_state))
    return;

  // 3. If the new value equals NaN, then throw a TypeError and abort these
  //    steps.
  if (std::isnan(end)) {
    MediaSource::LogAndThrowTypeError(exception_state,
                                      ExceptionMessages::NotAFiniteNumber(end));
    return;
  }
  // 4. If the new value is less than or equal to appendWindowStart then throw a
  //    TypeError exception and abort these steps.
  if (end <= append_window_start_) {
    MediaSource::LogAndThrowTypeError(
        exception_state, ExceptionMessages::IndexExceedsMinimumBound(
                             "value", end, append_window_start_));
    return;
  }

  // Do remainder of steps only if attachment is usable and underlying demuxer
  // is protected from destruction (applicable especially for MSE-in-Worker
  // case). Note, we must have |source_| and |source_| must have an attachment
  // because !IsRemoved().
  if (!source_->RunUnlessElementGoneOrClosingUs(
          WTF::Bind(&SourceBuffer::SetAppendWindowEnd_Locked,
                    WrapPersistent(this), end))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "Worker MediaSource attachment is closing");
  }
}

void SourceBuffer::SetAppendWindowEnd_Locked(
    double end,
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  DCHECK(source_);
  DCHECK(!updating_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // 5. Update the attribute to the new value.
  web_source_buffer_->SetAppendWindowEnd(end);
  append_window_end_ = end;
}

void SourceBuffer::appendBuffer(DOMArrayBuffer* data,
                                ExceptionState& exception_state) {
  DVLOG(2) << __func__ << " this=" << this << " size=" << data->ByteLength();
  // Section 3.2 appendBuffer()
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
  AppendBufferInternal(static_cast<const unsigned char*>(data->Data()),
                       data->ByteLength(), exception_state);
}

void SourceBuffer::appendBuffer(NotShared<DOMArrayBufferView> data,
                                ExceptionState& exception_state) {
  DVLOG(3) << __func__ << " this=" << this
           << " size=" << data.View()->byteLength();
  // Section 3.2 appendBuffer()
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
  AppendBufferInternal(
      static_cast<const unsigned char*>(data.View()->BaseAddress()),
      data.View()->byteLength(), exception_state);
}

void SourceBuffer::abort(ExceptionState& exception_state) {
  DVLOG(2) << __func__ << " this=" << this;
  // http://w3c.github.io/media-source/#widl-SourceBuffer-abort-void
  // 1. If this object has been removed from the sourceBuffers attribute of the
  //    parent media source then throw an InvalidStateError exception and abort
  //    these steps.
  // 2. If the readyState attribute of the parent media source is not in the
  //    "open" state then throw an InvalidStateError exception and abort these
  //    steps.
  if (IsRemoved()) {
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "This SourceBuffer has been removed from the parent media source.");
    return;
  }
  if (!source_->IsOpen()) {
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "The parent media source's readyState is not 'open'.");
    return;
  }

  // 3. If the range removal algorithm is running, then throw an
  //    InvalidStateError exception and abort these steps.
  if (pending_remove_start_ != -1) {
    DCHECK(updating_);
    // Throwing the exception and aborting these steps is new behavior that
    // is implemented behind the MediaSourceNewAbortAndDuration
    // RuntimeEnabledFeature.
    if (RuntimeEnabledFeatures::MediaSourceNewAbortAndDurationEnabled()) {
      MediaSource::LogAndThrowDOMException(
          exception_state, DOMExceptionCode::kInvalidStateError,
          "Aborting asynchronous remove() operation is disallowed.");
      return;
    }

    Deprecation::CountDeprecation(GetExecutionContext(),
                                  WebFeature::kMediaSourceAbortRemove);
    CancelRemove();
  }

  // 4. If the sourceBuffer.updating attribute equals true, then run the
  //    following steps: ...
  AbortIfUpdating();

  // Do remainder of steps only if attachment is usable and underlying demuxer
  // is protected from destruction (applicable especially for MSE-in-Worker
  // case). Note, we must have |source_| and |source_| must have an attachment
  // because !IsRemoved().
  if (!source_->RunUnlessElementGoneOrClosingUs(
          WTF::Bind(&SourceBuffer::Abort_Locked, WrapPersistent(this)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "Worker MediaSource attachment is closing");
  }
}

void SourceBuffer::Abort_Locked(
    MediaSourceAttachmentSupplement::ExclusiveKey pass_key) {
  DCHECK(source_);
  DCHECK(!updating_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // 5. Run the reset parser state algorithm.
  web_source_buffer_->ResetParserState();

  // 6. Set appendWindowStart to 0.
  // Note, there can be no exception, since appendWindowEnd can never be 0
  // (appendWindowStart can never be < 0, nor === appendWindowEnd in regular
  // setAppendWindow{Start,End} steps). Therefore, we can elide some checks and
  // reuse the existing internal helpers here that do not throw JS exception.
  SetAppendWindowStart_Locked(0, pass_key);

  // 7. Set appendWindowEnd to positive Infinity.
  // Note, likewise, no possible exception here, so reusing internal helper.
  SetAppendWindowEnd_Locked(std::numeric_limits<double>::infinity(), pass_key);
}

void SourceBuffer::remove(double start,
                          double end,
                          ExceptionState& exception_state) {
  DVLOG(2) << __func__ << " this=" << this << " start=" << start
           << " end=" << end;

  // Section 3.2 remove() method steps.
  // https://www.w3.org/TR/media-source/#widl-SourceBuffer-remove-void-double-start-unrestricted-double-end
  // 1. If this object has been removed from the sourceBuffers attribute of the
  //    parent media source then throw an InvalidStateError exception and abort
  //    these steps.
  // 2. If the updating attribute equals true, then throw an InvalidStateError
  //    exception and abort these steps.
  if (ThrowExceptionIfRemovedOrUpdating(IsRemoved(), updating_,
                                        exception_state))
    return;

  // Do remainder of steps only if attachment is usable and underlying demuxer
  // is protected from destruction (applicable especially for MSE-in-Worker
  // case). Note, we must have |source_| and |source_| must have an attachment
  // because !IsRemoved().
  if (!source_->RunUnlessElementGoneOrClosingUs(
          WTF::Bind(&SourceBuffer::Remove_Locked, WrapPersistent(this), start,
                    end, WTF::Unretained(&exception_state)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "Worker MediaSource attachment is closing");
  }
}

void SourceBuffer::Remove_Locked(
    double start,
    double end,
    ExceptionState* exception_state,
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  DCHECK(source_);
  DCHECK(!updating_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // 3. If duration equals NaN, then throw a TypeError exception and abort these
  //    steps.
  // 4. If start is negative or greater than duration, then throw a TypeError
  //    exception and abort these steps.
  double duration = source_->duration();
  if (start < 0 || std::isnan(duration) || start > duration) {
    MediaSource::LogAndThrowTypeError(
        *exception_state,
        ExceptionMessages::IndexOutsideRange(
            "start", start, 0.0, ExceptionMessages::kExclusiveBound,
            std::isnan(duration) ? 0 : duration,
            ExceptionMessages::kExclusiveBound));
    return;
  }

  // 5. If end is less than or equal to start or end equals NaN, then throw a
  //    TypeError exception and abort these steps.
  if (end <= start || std::isnan(end)) {
    MediaSource::LogAndThrowTypeError(
        *exception_state,
        "The end value provided (" + String::Number(end) +
            ") must be greater than the start value provided (" +
            String::Number(start) + ").");
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "SourceBuffer::remove",
                                    TRACE_ID_LOCAL(this));

  // 6. If the readyState attribute of the parent media source is in the "ended"
  //    state then run the following steps:
  // 6.1. Set the readyState attribute of the parent media source to "open"
  // 6.2. Queue a task to fire a simple event named sourceopen at the parent
  //      media source .
  source_->OpenIfInEndedState();

  // 7. Run the range removal algorithm with start and end as the start and end
  //    of the removal range.
  // 7.3. Set the updating attribute to true.
  updating_ = true;

  // 7.4. Queue a task to fire a simple event named updatestart at this
  //      SourceBuffer object.
  ScheduleEvent(event_type_names::kUpdatestart);

  // 7.5. Return control to the caller and run the rest of the steps
  //      asynchronously.
  pending_remove_start_ = start;
  pending_remove_end_ = end;
  remove_async_task_handle_ = PostCancellableTask(
      *GetExecutionContext()->GetTaskRunner(TaskType::kMediaElementEvent),
      FROM_HERE,
      WTF::Bind(&SourceBuffer::RemoveAsyncPart, WrapPersistent(this)));
}

void SourceBuffer::changeType(const String& type,
                              ExceptionState& exception_state) {
  DVLOG(2) << __func__ << " this=" << this << " type=" << type;

  // Per 30 May 2018 Codec Switching feature incubation spec:
  // https://rawgit.com/WICG/media-source/3b3742ea788999bb7ae4a4553ac7d574b0547dbe/index.html#dom-sourcebuffer-changetype
  // 1. If type is an empty string then throw a TypeError exception and abort
  //    these steps.
  if (type.IsEmpty()) {
    MediaSource::LogAndThrowTypeError(exception_state,
                                      "The type provided is empty");
    return;
  }

  // 2. If this object has been removed from the sourceBuffers attribute of the
  //    parent media source, then throw an InvalidStateError exception and abort
  //    these steps.
  // 3. If the updating attribute equals true, then throw an InvalidStateError
  //    exception and abort these steps.
  if (ThrowExceptionIfRemovedOrUpdating(IsRemoved(), updating_,
                                        exception_state))
    return;

  // Do remainder of steps only if attachment is usable and underlying demuxer
  // is protected from destruction (applicable especially for MSE-in-Worker
  // case). Note, we must have |source_| and |source_| must have an attachment
  // because !IsRemoved().
  if (!source_->RunUnlessElementGoneOrClosingUs(
          WTF::Bind(&SourceBuffer::ChangeType_Locked, WrapPersistent(this),
                    type, WTF::Unretained(&exception_state)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "Worker MediaSource attachment is closing");
  }
}

void SourceBuffer::ChangeType_Locked(
    const String& type,
    ExceptionState* exception_state,
    MediaSourceAttachmentSupplement::ExclusiveKey pass_key) {
  DCHECK(source_);
  DCHECK(!updating_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // 4. If type contains a MIME type that is not supported or contains a MIME
  //    type that is not supported with the types specified (currently or
  //    previously) of SourceBuffer objects in the sourceBuffers attribute of
  //    the parent media source, then throw a NotSupportedError exception and
  //    abort these steps.
  ContentType content_type(type);
  String codecs = content_type.Parameter("codecs");
  // TODO(wolenetz): Refactor and use a less-strict version of isTypeSupported
  // here. As part of that, CanChangeType in Chromium should inherit relaxation
  // of impl's StreamParserFactory (since it returns true iff a stream parser
  // can be constructed with |type|). See https://crbug.com/535738.
  if (!MediaSource::isTypeSupported(GetExecutionContext(), type) ||
      !web_source_buffer_->CanChangeType(content_type.GetType(), codecs)) {
    MediaSource::LogAndThrowDOMException(
        *exception_state, DOMExceptionCode::kNotSupportedError,
        "Changing to the type provided ('" + type + "') is not supported.");
    return;
  }

  // 5. If the readyState attribute of the parent media source is in the "ended"
  //    state then run the following steps:
  //    1. Set the readyState attribute of the parent media source to "open"
  //    2. Queue a task to fire a simple event named sourceopen at the parent
  //       media source.
  source_->OpenIfInEndedState();

  // 6. Run the reset parser state algorithm.
  web_source_buffer_->ResetParserState();

  // 7. Update the generate timestamps flag on this SourceBuffer object to the
  //    value in the "Generate Timestamps Flag" column of the byte stream format
  //    registry entry that is associated with type.
  // This call also updates the pipeline to switch bytestream parser and codecs.
  web_source_buffer_->ChangeType(content_type.GetType(), codecs);

  // 8. If the generate timestamps flag equals true: Set the mode attribute on
  //    this SourceBuffer object to "sequence", including running the associated
  //    steps for that attribute being set. Otherwise: keep the previous value
  //    of the mode attribute on this SourceBuffer object, without running any
  //    associated steps for that attribute being set.
  if (web_source_buffer_->GetGenerateTimestampsFlag())
    SetMode_Locked(SequenceKeyword(), exception_state, pass_key);

  // 9. Set pending initialization segment for changeType flag to true.
  // The logic for this flag is handled by the pipeline (the new bytestream
  // parser will expect an initialization segment first).
}

void SourceBuffer::setTrackDefaults(TrackDefaultList* track_defaults,
                                    ExceptionState& exception_state) {
  // Per 02 Dec 2014 Editor's Draft
  // http://w3c.github.io/media-source/#widl-SourceBuffer-trackDefaults
  // 1. If this object has been removed from the sourceBuffers attribute of
  //    the parent media source, then throw an InvalidStateError exception
  //    and abort these steps.
  // 2. If the updating attribute equals true, then throw an InvalidStateError
  //    exception and abort these steps.
  if (ThrowExceptionIfRemovedOrUpdating(IsRemoved(), updating_,
                                        exception_state))
    return;

  // 3. Update the attribute to the new value.
  track_defaults_ = track_defaults;
}

void SourceBuffer::CancelRemove() {
  DCHECK(updating_);
  DCHECK_NE(pending_remove_start_, -1);
  remove_async_task_handle_.Cancel();
  pending_remove_start_ = -1;
  pending_remove_end_ = -1;
  updating_ = false;

  if (!RuntimeEnabledFeatures::MediaSourceNewAbortAndDurationEnabled()) {
    ScheduleEvent(event_type_names::kAbort);
    ScheduleEvent(event_type_names::kUpdateend);
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "SourceBuffer::remove",
                                  TRACE_ID_LOCAL(this));
}

void SourceBuffer::AbortIfUpdating() {
  // Section 3.2 abort() method step 4 substeps.
  // http://w3c.github.io/media-source/#widl-SourceBuffer-abort-void

  if (!updating_)
    return;

  DCHECK_EQ(pending_remove_start_, -1);

  const char* trace_event_name = "SourceBuffer::appendBuffer";

  // 4.1. Abort the buffer append and stream append loop algorithms if they are
  //      running.
  append_buffer_async_task_handle_.Cancel();
  pending_append_data_.clear();
  pending_append_data_offset_ = 0;

  // 4.2. Set the updating attribute to false.
  updating_ = false;

  // 4.3. Queue a task to fire a simple event named abort at this SourceBuffer
  //      object.
  ScheduleEvent(event_type_names::kAbort);

  // 4.4. Queue a task to fire a simple event named updateend at this
  //      SourceBuffer object.
  ScheduleEvent(event_type_names::kUpdateend);

  TRACE_EVENT_NESTABLE_ASYNC_END0("media", trace_event_name,
                                  TRACE_ID_LOCAL(this));
}

void SourceBuffer::RemovedFromMediaSource() {
  if (IsRemoved())
    return;

  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  DVLOG(3) << __func__ << " this=" << this;
  if (pending_remove_start_ != -1) {
    CancelRemove();
  } else {
    AbortIfUpdating();
  }

  if (HTMLMediaElement::MediaTracksEnabledInternally()) {
    DCHECK(source_);
    RemoveMediaTracks();
  }

  web_source_buffer_->RemovedFromMediaSource();
  web_source_buffer_.reset();
  source_ = nullptr;
  async_event_queue_ = nullptr;
}

double SourceBuffer::HighestPresentationTimestamp() {
  DCHECK(!IsRemoved());
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  double pts = web_source_buffer_->HighestPresentationTimestamp();
  DVLOG(3) << __func__ << " this=" << this << ", pts=" << pts;
  return pts;
}

void SourceBuffer::RemoveMediaTracks() {
  // Spec:
  // http://w3c.github.io/media-source/#widl-MediaSource-removeSourceBuffer-void-SourceBuffer-sourceBuffer
  DCHECK(HTMLMediaElement::MediaTracksEnabledInternally());
  DCHECK(source_);

  scoped_refptr<MediaSourceAttachmentSupplement> attachment;
  MediaSourceTracer* tracer;
  std::tie(attachment, tracer) = source_->AttachmentAndTracer();
  DCHECK(attachment);

  // One path leading to here is from |source_|'s ContextDestroyed(), so we
  // cannot consult GetExecutionContext() here to determine if this is a
  // worker-thread-owned or main-thread-owned SourceBuffer. Rather, we will rely
  // on IsMainThread().
  if (!IsMainThread()) {
    RemovePlaceholderCrossThreadTracks(attachment, tracer);
    return;
  }

  // For safety, ensure we are using SameThreadAttachment behavior. This is just
  // in case we somehow are incorrectly running on the main thread, but are a
  // worker-thread-owned SourceBuffer with a cross-thread attachment.
  CHECK(tracer);  // Only same-thread attachments have a tracer.

  // 3. Let SourceBuffer audioTracks list equal the AudioTrackList object
  //    returned by sourceBuffer.audioTracks.
  // 4. If the SourceBuffer audioTracks list is not empty, then run the
  //    following steps:
  // 4.1 Let HTMLMediaElement audioTracks list equal the AudioTrackList object
  //     returned by the audioTracks attribute on the HTMLMediaElement.
  // 4.2 Let the removed enabled audio track flag equal false.
  bool removed_enabled_audio_track = false;
  Vector<String> audio_track_removal_ids;
  // 4.3 For each AudioTrack object in the SourceBuffer audioTracks list, run
  //     the following steps:
  while (audioTracks().length() > 0) {
    AudioTrack* audio_track = audioTracks().AnonymousIndexedGetter(0);
    // 4.3.1 Set the sourceBuffer attribute on the AudioTrack object to null.
    SourceBufferTrackBaseSupplement::SetSourceBuffer(*audio_track, nullptr);
    // 4.3.2 If the enabled attribute on the AudioTrack object is true, then set
    //       the removed enabled audio track flag to true.
    if (audio_track->enabled())
      removed_enabled_audio_track = true;
    // 4.3.3 Remove the AudioTrack object from the HTMLMediaElement audioTracks
    //       list.
    // 4.3.4 Queue a task to fire a trusted event named removetrack, that does
    //       not bubble and is not cancelable, and that uses the TrackEvent
    //       interface, at the HTMLMediaElement audioTracks list.
    // We compile the list of audio tracks to remove from the media element here
    // and tell the element to remove them, below, with step 4.4.
    audio_track_removal_ids.push_back(audio_track->id());
    // 4.3.5 Remove the AudioTrack object from the SourceBuffer audioTracks
    //       list.
    // 4.3.6 Queue a task to fire a trusted event named removetrack, that does
    //       not bubble and is not cancelable, and that uses the TrackEvent
    //       interface, at the SourceBuffer audioTracks list.
    audioTracks().Remove(audio_track->id());
  }
  // 4.4 If the removed enabled audio track flag equals true, then queue a task
  //     to fire a simple event named change at the HTMLMediaElement audioTracks
  //     list.
  // Here, we perform batch removal of audio tracks, compiled in step 4.3.4,
  // above, along with conditional enqueueing of change event.
  if (!audio_track_removal_ids.IsEmpty()) {
    attachment->RemoveAudioTracksFromMediaElement(
        tracer, audio_track_removal_ids,
        removed_enabled_audio_track /* enqueue_change_event */);
  }

  // 5. Let SourceBuffer videoTracks list equal the VideoTrackList object
  //    returned by sourceBuffer.videoTracks.
  // 6. If the SourceBuffer videoTracks list is not empty, then run the
  //    following steps:
  // 6.1 Let HTMLMediaElement videoTracks list equal the VideoTrackList object
  //     returned by the videoTracks attribute on the HTMLMediaElement.
  // 6.2 Let the removed selected video track flag equal false.
  bool removed_selected_video_track = false;
  Vector<String> video_track_removal_ids;
  // 6.3 For each VideoTrack object in the SourceBuffer videoTracks list, run
  //     the following steps:
  while (videoTracks().length() > 0) {
    VideoTrack* video_track = videoTracks().AnonymousIndexedGetter(0);
    // 6.3.1 Set the sourceBuffer attribute on the VideoTrack object to null.
    SourceBufferTrackBaseSupplement::SetSourceBuffer(*video_track, nullptr);
    // 6.3.2 If the selected attribute on the VideoTrack object is true, then
    //       set the removed selected video track flag to true.
    if (video_track->selected())
      removed_selected_video_track = true;
    // 6.3.3 Remove the VideoTrack object from the HTMLMediaElement videoTracks
    //       list.
    // 6.3.4 Queue a task to fire a trusted event named removetrack, that does
    //       not bubble and is not cancelable, and that uses the TrackEvent
    //       interface, at the HTMLMediaElement videoTracks list.
    // We compile the list of video tracks to remove from the media element here
    // and tell the element to remove them, below, with step 6.4.
    video_track_removal_ids.push_back(video_track->id());
    // 6.3.5 Remove the VideoTrack object from the SourceBuffer videoTracks
    //       list.
    // 6.3.6 Queue a task to fire a trusted event named removetrack, that does
    //       not bubble and is not cancelable, and that uses the TrackEvent
    //       interface, at the SourceBuffer videoTracks list.
    videoTracks().Remove(video_track->id());
  }
  // 6.4 If the removed selected video track flag equals true, then queue a task
  //     to fire a simple event named change at the HTMLMediaElement videoTracks
  //     list.
  // Here, we perform batch removal of video tracks, compiled in step 6.3.4,
  // above, along with conditional enqueueing of change event.
  if (!video_track_removal_ids.IsEmpty()) {
    attachment->RemoveVideoTracksFromMediaElement(
        tracer, video_track_removal_ids,
        removed_selected_video_track /* enqueue_change_event */);
  }

  // 7-8. TODO(servolk): Remove text tracks once SourceBuffer has text tracks.
}

double SourceBuffer::GetMediaTime() {
  DCHECK(source_);
  scoped_refptr<MediaSourceAttachmentSupplement> attachment;
  MediaSourceTracer* tracer;
  std::tie(attachment, tracer) = source_->AttachmentAndTracer();
  DCHECK(attachment);
  return attachment->GetRecentMediaTime(tracer);
}

template <class T>
T* FindExistingTrackById(const TrackListBase<T>& track_list, const String& id) {
  // According to MSE specification
  // (https://w3c.github.io/media-source/#sourcebuffer-init-segment-received)
  // step 3.1:
  // > If more than one track for a single type are present (ie 2 audio tracks),
  // then the Track IDs match the ones in the first initialization segment.
  // I.e. we only need to search by TrackID if there is more than one track,
  // otherwise we can assume that the only track of the given type is the same
  // one that we had in previous init segments.
  if (track_list.length() == 1)
    return track_list.AnonymousIndexedGetter(0);
  return track_list.getTrackById(id);
}

const TrackDefault* SourceBuffer::GetTrackDefault(
    const AtomicString& track_type,
    const AtomicString& byte_stream_track_id) const {
  // This is a helper for implementation of default track label and default
  // track language algorithms.
  // defaultTrackLabel spec:
  // https://w3c.github.io/media-source/#sourcebuffer-default-track-label
  // defaultTrackLanguage spec:
  // https://w3c.github.io/media-source/#sourcebuffer-default-track-language

  // 1. If trackDefaults contains a TrackDefault object with a type attribute
  //    equal to type and a byteStreamTrackID attribute equal to
  //    byteStreamTrackID, then return the value of the label/language attribute
  //    on this matching object and abort these steps.
  // 2. If trackDefaults contains a TrackDefault object with a type attribute
  //    equal to type and a byteStreamTrackID attribute equal to an empty
  //    string, then return the value of the label/language attribute on this
  //    matching object and abort these steps.
  // 3. Return an empty string to the caller
  const TrackDefault* track_default_with_empty_bytestream_id = nullptr;
  for (unsigned i = 0; i < track_defaults_->length(); ++i) {
    const TrackDefault* track_default = track_defaults_->item(i);
    if (track_default->type() != track_type)
      continue;
    if (track_default->byteStreamTrackID() == byte_stream_track_id)
      return track_default;
    if (!track_default_with_empty_bytestream_id &&
        track_default->byteStreamTrackID() == "")
      track_default_with_empty_bytestream_id = track_default;
  }
  return track_default_with_empty_bytestream_id;
}

AtomicString SourceBuffer::DefaultTrackLabel(
    const AtomicString& track_type,
    const AtomicString& byte_stream_track_id) const {
  // Spec: https://w3c.github.io/media-source/#sourcebuffer-default-track-label
  const TrackDefault* track_default =
      GetTrackDefault(track_type, byte_stream_track_id);
  return track_default ? AtomicString(track_default->label()) : "";
}

AtomicString SourceBuffer::DefaultTrackLanguage(
    const AtomicString& track_type,
    const AtomicString& byte_stream_track_id) const {
  // Spec:
  // https://w3c.github.io/media-source/#sourcebuffer-default-track-language
  const TrackDefault* track_default =
      GetTrackDefault(track_type, byte_stream_track_id);
  return track_default ? AtomicString(track_default->language()) : "";
}

void SourceBuffer::AddPlaceholderCrossThreadTracks(
    const WebVector<MediaTrackInfo>& new_tracks,
    scoped_refptr<MediaSourceAttachmentSupplement> attachment) {
  // TODO(https://crbug.com/878133): Complete the MSE-in-Workers function
  // necessary to enable successful experimental usage of AudioVideoTracks
  // feature when MSE is in worker. Meanwhile, at least notify the attachment
  // to tell the media element to populate appropriately identified tracks so
  // that the BackgroundVideoOptimization feature functions for MSE-in-Workers
  // playbacks.
  DCHECK(RuntimeEnabledFeatures::MediaSourceInWorkersEnabled());
  DCHECK(!IsMainThread());
  DCHECK(!first_initialization_segment_received_);
  DCHECK(source_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // Perform placeholder track additions on the main thread for each audio
  // and video track in the initialization segment. Note that this depends
  // on the caller already verifying correctness of the track metadata (see
  // SourceBufferState::OnNewConfigs()).
  bool enable_next_audio_track = true;
  bool select_next_video_track = true;
  DCHECK(audio_track_ids_for_crossthread_removal_.IsEmpty());
  DCHECK(video_track_ids_for_crossthread_removal_.IsEmpty());
  for (const MediaTrackInfo& track_info : new_tracks) {
    if (track_info.track_type == WebMediaPlayer::kAudioTrack) {
      WebString label = track_info.label;
      if (label.IsEmpty()) {
        label = DefaultTrackLabel(TrackDefault::AudioKeyword(),
                                  track_info.byte_stream_track_id);
      }

      WebString language = track_info.language;
      if (language.IsEmpty()) {
        language = DefaultTrackLanguage(TrackDefault::AudioKeyword(),
                                        track_info.byte_stream_track_id);
      }

      attachment->AddMainThreadAudioTrackToMediaElement(
          track_info.id, track_info.kind, std::move(label), std::move(language),
          enable_next_audio_track);

      // Only enable the first audio track for this SourceBuffer.
      enable_next_audio_track = false;

      // Remember to remove this track from the element later.
      audio_track_ids_for_crossthread_removal_.push_back(track_info.id);
    } else if (track_info.track_type == WebMediaPlayer::kVideoTrack) {
      WebString label = track_info.label;
      if (label.IsEmpty()) {
        label = DefaultTrackLabel(TrackDefault::VideoKeyword(),
                                  track_info.byte_stream_track_id);
      }

      WebString language = track_info.language;
      if (language.IsEmpty()) {
        language = DefaultTrackLanguage(TrackDefault::VideoKeyword(),
                                        track_info.byte_stream_track_id);
      }
      attachment->AddMainThreadVideoTrackToMediaElement(
          track_info.id, track_info.kind, std::move(label), std::move(language),
          select_next_video_track);

      // Only select the first video track for this SourceBuffer.
      select_next_video_track = false;

      // Remember to remove this track from the element later.
      video_track_ids_for_crossthread_removal_.push_back(track_info.id);
    }
  }
}

void SourceBuffer::RemovePlaceholderCrossThreadTracks(
    scoped_refptr<MediaSourceAttachmentSupplement> attachment,
    MediaSourceTracer* tracer) {
  // TODO(https://crbug.com/878133): Remove this special-casing once worker
  // thread track creation and tracklist modifications are supported.
  DCHECK(RuntimeEnabledFeatures::MediaSourceInWorkersEnabled());
  DCHECK(!IsMainThread());
  DCHECK(!tracer);  // Cross-thread attachments don't use a tracer.

  // Remove all of this SourceBuffer's cross-thread media element audio and
  // video tracks, and enqueue a change event against the appropriate track
  // lists on the media element. The event(s) may be extra, but likely unseen by
  // application unless it is attempting experimental AudioVideoTracks usage,
  // too.
  if (!audio_track_ids_for_crossthread_removal_.IsEmpty()) {
    attachment->RemoveAudioTracksFromMediaElement(
        tracer, std::move(audio_track_ids_for_crossthread_removal_),
        true /* enqueue_change_event */);
  }

  if (!video_track_ids_for_crossthread_removal_.IsEmpty()) {
    attachment->RemoveVideoTracksFromMediaElement(
        tracer, std::move(video_track_ids_for_crossthread_removal_),
        true /* enqueue_change_event */);
  }
}

bool SourceBuffer::InitializationSegmentReceived(
    const WebVector<MediaTrackInfo>& new_tracks) {
  DVLOG(3) << __func__ << " this=" << this << " tracks=" << new_tracks.size();
  DCHECK(source_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  scoped_refptr<MediaSourceAttachmentSupplement> attachment;
  MediaSourceTracer* tracer;
  std::tie(attachment, tracer) = source_->AttachmentAndTracer();
  DCHECK(attachment);
  DCHECK_EQ(!tracer, !IsMainThread());

  DCHECK(updating_);

  // Feature and execution-context conditioning may disable full population of
  // tracks in SourceBuffer (and maybe even in media element).
  bool finish_early = false;

  if (!HTMLMediaElement::MediaTracksEnabledInternally()) {
    // Don't populate SourceBuffer tracks. Let the media element figure out
    // internally any tracks it wants to populate (it may do placeholders).
    finish_early = true;
  } else if (GetExecutionContext()->IsDedicatedWorkerGlobalScope()) {
    finish_early = true;
    if (!first_initialization_segment_received_) {
      AddPlaceholderCrossThreadTracks(new_tracks, attachment);
    }
  }

  if (finish_early) {
    if (!first_initialization_segment_received_) {
      source_->SetSourceBufferActive(this, true);
      first_initialization_segment_received_ = true;
    }
    return true;
  }

  DCHECK(GetExecutionContext()->IsWindow());
  DCHECK(IsMainThread());

  // Implementation of Initialization Segment Received, see
  // https://w3c.github.io/media-source/#sourcebuffer-init-segment-received

  // Sort newTracks into audio and video tracks to facilitate implementation
  // of subsequent steps of this algorithm.
  Vector<MediaTrackInfo> new_audio_tracks;
  Vector<MediaTrackInfo> new_video_tracks;
  for (const MediaTrackInfo& track_info : new_tracks) {
    const TrackBase* track = nullptr;
    if (track_info.track_type == WebMediaPlayer::kAudioTrack) {
      new_audio_tracks.push_back(track_info);
      if (first_initialization_segment_received_)
        track = FindExistingTrackById(audioTracks(), track_info.id);
    } else if (track_info.track_type == WebMediaPlayer::kVideoTrack) {
      new_video_tracks.push_back(track_info);
      if (first_initialization_segment_received_)
        track = FindExistingTrackById(videoTracks(), track_info.id);
    } else {
      DVLOG(3) << __func__ << " this=" << this
               << " failed: unsupported track type " << track_info.track_type;
      // TODO(servolk): Add handling of text tracks.
      NOTREACHED();
    }
    if (first_initialization_segment_received_ && !track) {
      DVLOG(3) << __func__ << " this=" << this
               << " failed: tracks mismatch the first init segment.";
      return false;
    }
#if DCHECK_IS_ON()
    const char* log_track_type_str =
        (track_info.track_type == WebMediaPlayer::kAudioTrack) ? "audio"
                                                               : "video";
    DVLOG(3) << __func__ << " this=" << this << " : " << log_track_type_str
             << " track "
             << " id=" << String(track_info.id)
             << " byteStreamTrackID=" << String(track_info.byte_stream_track_id)
             << " kind=" << String(track_info.kind)
             << " label=" << String(track_info.label)
             << " language=" << String(track_info.language);
#endif
  }

  // 1. Update the duration attribute if it currently equals NaN:
  // TODO(servolk): Pass also stream duration into initSegmentReceived.

  // 2. If the initialization segment has no audio, video, or text tracks, then
  //    run the append error algorithm with the decode error parameter set to
  //    true and abort these steps.
  if (new_tracks.empty()) {
    DVLOG(3) << __func__ << " this=" << this
             << " failed: no tracks found in the init segment.";
    // The append error algorithm will be called at the top level after we
    // return false here to indicate failure.
    return false;
  }

  // 3. If the first initialization segment received flag is true, then run the
  //    following steps:
  if (first_initialization_segment_received_) {
    // 3.1 Verify the following properties. If any of the checks fail then run
    //     the append error algorithm with the decode error parameter set to
    //     true and abort these steps.
    bool tracks_match_first_init_segment = true;
    // - The number of audio, video, and text tracks match what was in the first
    //   initialization segment.
    if (new_audio_tracks.size() != audioTracks().length() ||
        new_video_tracks.size() != videoTracks().length()) {
      tracks_match_first_init_segment = false;
    }
    // - The codecs for each track, match what was specified in the first
    //   initialization segment.
    // This is currently done in MediaSourceState::OnNewConfigs.
    // - If more than one track for a single type are present (ie 2 audio
    //   tracks), then the Track IDs match the ones in the first initialization
    //   segment.
    if (tracks_match_first_init_segment && new_audio_tracks.size() > 1) {
      for (wtf_size_t i = 0; i < new_audio_tracks.size(); ++i) {
        const String& new_track_id = new_video_tracks[i].id;
        if (new_track_id !=
            String(audioTracks().AnonymousIndexedGetter(i)->id())) {
          tracks_match_first_init_segment = false;
          break;
        }
      }
    }

    if (tracks_match_first_init_segment && new_video_tracks.size() > 1) {
      for (wtf_size_t i = 0; i < new_video_tracks.size(); ++i) {
        const String& new_track_id = new_video_tracks[i].id;
        if (new_track_id !=
            String(videoTracks().AnonymousIndexedGetter(i)->id())) {
          tracks_match_first_init_segment = false;
          break;
        }
      }
    }

    if (!tracks_match_first_init_segment) {
      DVLOG(3) << __func__ << " this=" << this
               << " failed: tracks mismatch the first init segment.";
      // The append error algorithm will be called at the top level after we
      // return false here to indicate failure.
      return false;
    }

    // 3.2 Add the appropriate track descriptions from this initialization
    //     segment to each of the track buffers.  This is done in Chromium code
    //     in stream parsers and demuxer implementations.

    // 3.3 Set the need random access point flag on all track buffers to true.
    // This is done in Chromium code, see MediaSourceState::OnNewConfigs.
  }

  // 4. Let active track flag equal false.
  bool active_track = false;

  // 5. If the first initialization segment received flag is false, then run the
  //    following steps:
  if (!first_initialization_segment_received_) {
    // 5.1 If the initialization segment contains tracks with codecs the user
    //     agent does not support, then run the append error algorithm with the
    //     decode error parameter set to true and abort these steps.
    // This is done in Chromium code, see MediaSourceState::OnNewConfigs.

    // 5.2 For each audio track in the initialization segment, run following
    //     steps:
    for (const MediaTrackInfo& track_info : new_audio_tracks) {
      // 5.2.1 Let audio byte stream track ID be the Track ID for the current
      //       track being processed.
      const auto& byte_stream_track_id = track_info.byte_stream_track_id;
      // 5.2.2 Let audio language be a BCP 47 language tag for the language
      //       specified in the initialization segment for this track or an
      //       empty string if no language info is present.
      WebString language = track_info.language;
      // 5.2.3 If audio language equals an empty string or the 'und' BCP 47
      //       value, then run the default track language algorithm with
      //       byteStreamTrackID set to audio byte stream track ID and type set
      //       to "audio" and assign the value returned by the algorithm to
      //       audio language.
      if (language.IsEmpty() || language == "und")
        language = DefaultTrackLanguage(TrackDefault::AudioKeyword(),
                                        byte_stream_track_id);
      // 5.2.4 Let audio label be a label specified in the initialization
      //       segment for this track or an empty string if no label info is
      //       present.
      WebString label = track_info.label;
      // 5.3.5 If audio label equals an empty string, then run the default track
      //       label algorithm with byteStreamTrackID set to audio byte stream
      //       track ID and type set to "audio" and assign the value returned by
      //       the algorithm to audio label.
      if (label.IsEmpty())
        label = DefaultTrackLabel(TrackDefault::AudioKeyword(),
                                  byte_stream_track_id);
      // 5.2.6 Let audio kinds be an array of kind strings specified in the
      //       initialization segment for this track or an empty array if no
      //       kind information is provided.
      const auto& kind = track_info.kind;
      // 5.2.7 TODO(servolk): Implement track kind processing.
      // 5.2.8.2 Let new audio track be a new AudioTrack object.
      auto* audio_track = MakeGarbageCollected<AudioTrack>(
          track_info.id, kind, std::move(label), std::move(language), false);
      SourceBufferTrackBaseSupplement::SetSourceBuffer(*audio_track, this);
      // 5.2.8.7 If audioTracks.length equals 0, then run the following steps:
      if (audioTracks().length() == 0) {
        // 5.2.8.7.1 Set the enabled property on new audio track to true.
        audio_track->setEnabled(true);
        // 5.2.8.7.2 Set active track flag to true.
        active_track = true;
      }
      // 5.2.8.8 Add new audio track to the audioTracks attribute on this
      //         SourceBuffer object.
      // 5.2.8.9 Queue a task to fire a trusted event named addtrack, that does
      //         not bubble and is not cancelable, and that uses the TrackEvent
      //         interface, at the AudioTrackList object referenced by the
      //         audioTracks attribute on this SourceBuffer object.
      audioTracks().Add(audio_track);
      // 5.2.8.10 Add new audio track to the audioTracks attribute on the
      //          HTMLMediaElement.
      // 5.2.8.11 Queue a task to fire a trusted event named addtrack, that does
      //          not bubble and is not cancelable, and that uses the TrackEvent
      //          interface, at the AudioTrackList object referenced by the
      //          audioTracks attribute on the HTMLMediaElement.
      attachment->AddAudioTrackToMediaElement(tracer, audio_track);
    }

    // 5.3. For each video track in the initialization segment, run following
    //      steps:
    for (const MediaTrackInfo& track_info : new_video_tracks) {
      // 5.3.1 Let video byte stream track ID be the Track ID for the current
      //       track being processed.
      const auto& byte_stream_track_id = track_info.byte_stream_track_id;
      // 5.3.2 Let video language be a BCP 47 language tag for the language
      //       specified in the initialization segment for this track or an
      //       empty string if no language info is present.
      WebString language = track_info.language;
      // 5.3.3 If video language equals an empty string or the 'und' BCP 47
      //       value, then run the default track language algorithm with
      //       byteStreamTrackID set to video byte stream track ID and type set
      //       to "video" and assign the value returned by the algorithm to
      //       video language.
      if (language.IsEmpty() || language == "und")
        language = DefaultTrackLanguage(TrackDefault::VideoKeyword(),
                                        byte_stream_track_id);
      // 5.3.4 Let video label be a label specified in the initialization
      //       segment for this track or an empty string if no label info is
      //       present.
      WebString label = track_info.label;
      // 5.3.5 If video label equals an empty string, then run the default track
      //       label algorithm with byteStreamTrackID set to video byte stream
      //       track ID and type set to "video" and assign the value returned by
      //       the algorithm to video label.
      if (label.IsEmpty())
        label = DefaultTrackLabel(TrackDefault::VideoKeyword(),
                                  byte_stream_track_id);
      // 5.3.6 Let video kinds be an array of kind strings specified in the
      //       initialization segment for this track or an empty array if no
      //       kind information is provided.
      const auto& kind = track_info.kind;
      // 5.3.7 TODO(servolk): Implement track kind processing.
      // 5.3.8.2 Let new video track be a new VideoTrack object.
      auto* video_track = MakeGarbageCollected<VideoTrack>(
          track_info.id, kind, std::move(label), std::move(language), false);
      SourceBufferTrackBaseSupplement::SetSourceBuffer(*video_track, this);
      // 5.3.8.7 If videoTracks.length equals 0, then run the following steps:
      if (videoTracks().length() == 0) {
        // 5.3.8.7.1 Set the selected property on new audio track to true.
        video_track->setSelected(true);
        // 5.3.8.7.2 Set active track flag to true.
        active_track = true;
      }
      // 5.3.8.8 Add new video track to the videoTracks attribute on this
      //         SourceBuffer object.
      // 5.3.8.9 Queue a task to fire a trusted event named addtrack, that does
      //         not bubble and is not cancelable, and that uses the TrackEvent
      //         interface, at the VideoTrackList object referenced by the
      //         videoTracks attribute on this SourceBuffer object.
      videoTracks().Add(video_track);
      // 5.3.8.10 Add new video track to the videoTracks attribute on the
      //          HTMLMediaElement.
      // 5.3.8.11 Queue a task to fire a trusted event named addtrack, that does
      //          not bubble and is not cancelable, and that uses the TrackEvent
      //          interface, at the VideoTrackList object referenced by the
      //          videoTracks attribute on the HTMLMediaElement.
      attachment->AddVideoTrackToMediaElement(tracer, video_track);
    }

    // 5.4 TODO(servolk): Add text track processing here.

    // 5.5 If active track flag equals true, then run the following steps:
    // activesourcebuffers.
    if (active_track) {
      // 5.5.1 Add this SourceBuffer to activeSourceBuffers.
      // 5.5.2 Queue a task to fire a simple event named addsourcebuffer at
      //       activeSourceBuffers
      source_->SetSourceBufferActive(this, true);
    }

    // 5.6. Set first initialization segment received flag to true.
    first_initialization_segment_received_ = true;
  }

  return true;
}

void SourceBuffer::NotifyParseWarning(const ParseWarning warning) {
  DCHECK(source_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  switch (warning) {
    case WebSourceBufferClient::kKeyframeTimeGreaterThanDependant:
      // Report this problematic GOP structure to help inform follow-up work.
      // Media engine also records RAPPOR for these, up to once per track, at
      // Media.OriginUrl.MSE.KeyframeTimeGreaterThanDependant.
      // TODO(wolenetz): Use the data to scope additional work. See
      // https://crbug.com/739931.
      UseCounter::Count(
          GetExecutionContext(),
          WebFeature::kMediaSourceKeyframeTimeGreaterThanDependant);
      break;
    case WebSourceBufferClient::kMuxedSequenceMode:
      // Report this problematic API usage to help inform follow-up work.
      // Media engine also records RAPPOR for these, up to once per
      // SourceBuffer, at Media.OriginUrl.MSE.MuxedSequenceModeSourceBuffer.
      // TODO(wolenetz): Use the data to scope additional work. See
      // https://crbug.com/737757.
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kMediaSourceMuxedSequenceMode);
      break;
    case WebSourceBufferClient::kGroupEndTimestampDecreaseWithinMediaSegment:
      // Report this problematic Media Segment structure usage to help inform
      // follow-up work.
      // TODO(wolenetz): Use the data to scope additional work. See
      // https://crbug.com/920853 and
      // https://github.com/w3c/media-source/issues/203.
      UseCounter::Count(
          GetExecutionContext(),
          WebFeature::kMediaSourceGroupEndTimestampDecreaseWithinMediaSegment);
      break;
  }
}

bool SourceBuffer::HasPendingActivity() const {
  return updating_ || append_buffer_async_task_handle_.IsActive() ||
         remove_async_task_handle_.IsActive() ||
         (async_event_queue_ && async_event_queue_->HasPendingEvents());
}

void SourceBuffer::ContextDestroyed() {
  append_buffer_async_task_handle_.Cancel();
  pending_append_data_.clear();
  pending_append_data_offset_ = 0;

  remove_async_task_handle_.Cancel();
  pending_remove_start_ = -1;
  pending_remove_end_ = -1;

  updating_ = false;
}

ExecutionContext* SourceBuffer::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& SourceBuffer::InterfaceName() const {
  return event_target_names::kSourceBuffer;
}

bool SourceBuffer::IsRemoved() const {
  return !source_;
}

void SourceBuffer::ScheduleEvent(const AtomicString& event_name) {
  DCHECK(async_event_queue_);

  Event* event = Event::Create(event_name);
  event->SetTarget(this);

  async_event_queue_->EnqueueEvent(FROM_HERE, *event);
}

bool SourceBuffer::PrepareAppend(double media_time,
                                 size_t new_data_size,
                                 ExceptionState& exception_state) {
  // Runs the remainder of prepare append algorithm steps beyond those already
  // done by the caller.
  // http://w3c.github.io/media-source/#sourcebuffer-prepare-append
  // 3.5.4 Prepare Append Algorithm
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "SourceBuffer::prepareAppend",
                                    TRACE_ID_LOCAL(this));
  // 3. If the HTMLMediaElement.error attribute is not null, then throw an
  //    InvalidStateError exception and abort these steps.
  DCHECK(source_);
  scoped_refptr<MediaSourceAttachmentSupplement> attachment;
  MediaSourceTracer* tracer;
  std::tie(attachment, tracer) = source_->AttachmentAndTracer();
  DCHECK(attachment);
  DCHECK_EQ(!tracer, !IsMainThread());
  if (attachment->GetElementError(tracer)) {
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "The HTMLMediaElement.error attribute is not null.");
    TRACE_EVENT_NESTABLE_ASYNC_END0("media", "SourceBuffer::prepareAppend",
                                    TRACE_ID_LOCAL(this));
    return false;
  }

  // 4. If the readyState attribute of the parent media source is in the "ended"
  //    state then run the following steps:
  //    1. Set the readyState attribute of the parent media source to "open"
  //    2. Queue a task to fire a simple event named sourceopen at the parent
  //       media source.
  source_->OpenIfInEndedState();

  // 5. Run the coded frame eviction algorithm.
  if (!EvictCodedFrames(media_time, new_data_size) ||
      !base::CheckedNumeric<wtf_size_t>(new_data_size).IsValid()) {
    // 6. If the buffer full flag equals true, then throw a QUOTA_EXCEEDED_ERR
    //    exception and abort these steps.
    //    If the incoming data exceeds wtf_size_t::max, then our implementation
    //    cannot deal with it, so we also throw a QuotaExceededError.
    DVLOG(3) << __func__ << " this=" << this << " -> throw QuotaExceededError";
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kQuotaExceededError,
        "The SourceBuffer is full, and cannot free space to append additional "
        "buffers.");
    TRACE_EVENT_NESTABLE_ASYNC_END0("media", "SourceBuffer::prepareAppend",
                                    TRACE_ID_LOCAL(this));
    return false;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "SourceBuffer::prepareAppend",
                                  TRACE_ID_LOCAL(this));
  return true;
}

bool SourceBuffer::EvictCodedFrames(double media_time, size_t new_data_size) {
  DCHECK(source_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // Nothing to do if this SourceBuffer does not yet have frames to evict.
  if (!first_initialization_segment_received_)
    return true;

  bool result = web_source_buffer_->EvictCodedFrames(media_time, new_data_size);
  if (!result) {
    DVLOG(3) << __func__ << " this=" << this
             << " failed. newDataSize=" << new_data_size
             << " media_time=" << media_time << " buffered="
             << WebTimeRangesToString(web_source_buffer_->Buffered());
  }
  return result;
}

void SourceBuffer::AppendBufferInternal(const unsigned char* data,
                                        size_t size,
                                        ExceptionState& exception_state) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "SourceBuffer::appendBuffer",
                                    TRACE_ID_LOCAL(this), "size", size);
  // Section 3.2 appendBuffer()
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
  //
  // 1. Run the prepare append algorithm.
  //
  // http://w3c.github.io/media-source/#sourcebuffer-prepare-append
  // 3.5.4 Prepare Append Algorithm
  //
  // Do the first two steps of the prepare append algorithm here, so that we can
  // be assured if they succeed that the remainder of this scope runs with the
  // attachment's |attachment_state_lock_| mutex held.
  //
  // 1. If the SourceBuffer has been removed from the sourceBuffers attribute of
  //    the parent media source then throw an InvalidStateError exception and
  //    abort these steps.
  // 2. If the updating attribute equals true, then throw an InvalidStateError
  //    exception and abort these steps.
  if (ThrowExceptionIfRemovedOrUpdating(IsRemoved(), updating_,
                                        exception_state)) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("media", "SourceBuffer::appendBuffer",
                                    TRACE_ID_LOCAL(this));
    return;
  }

  // Do remainder of steps of prepare append algorithm and appendBuffer only if
  // attachment is usable and underlying demuxer is protected from destruction
  // (applicable especially for MSE-in-Worker case). Note, we must have
  // |source_| and |source_| must have an attachment because !IsRemoved().
  if (!source_->RunUnlessElementGoneOrClosingUs(WTF::Bind(
          &SourceBuffer::AppendBufferInternal_Locked, WrapPersistent(this),
          WTF::Unretained(data), size, WTF::Unretained(&exception_state)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, exception should be for this case.
    MediaSource::LogAndThrowDOMException(
        exception_state, DOMExceptionCode::kInvalidStateError,
        "Worker MediaSource attachment is closing");
  }
}

void SourceBuffer::AppendBufferInternal_Locked(
    const unsigned char* data,
    size_t size,
    ExceptionState* exception_state,
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  DCHECK(source_);
  DCHECK(!updating_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // Finish the prepare append algorithm begun by the caller.
  double media_time = GetMediaTime();
  if (!PrepareAppend(media_time, size, *exception_state)) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("media", "SourceBuffer::appendBuffer",
                                    TRACE_ID_LOCAL(this));
    return;
  }
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "prepareAsyncAppend",
                                    TRACE_ID_LOCAL(this));

  // 2. Add data to the end of the input buffer.
  DCHECK(data || size == 0);
  if (data)
    pending_append_data_.Append(data, base::checked_cast<wtf_size_t>(size));
  pending_append_data_offset_ = 0;

  // 3. Set the updating attribute to true.
  updating_ = true;

  // 4. Queue a task to fire a simple event named updatestart at this
  //    SourceBuffer object.
  ScheduleEvent(event_type_names::kUpdatestart);

  // 5. Asynchronously run the buffer append algorithm.
  append_buffer_async_task_handle_ = PostCancellableTask(
      *GetExecutionContext()->GetTaskRunner(TaskType::kMediaElementEvent),
      FROM_HERE,
      WTF::Bind(&SourceBuffer::AppendBufferAsyncPart, WrapPersistent(this)));

  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "prepareAsyncAppend",
                                  TRACE_ID_LOCAL(this));
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "delay", TRACE_ID_LOCAL(this),
                                    "type", "initialDelay");
}

void SourceBuffer::AppendBufferAsyncPart() {
  // Do the async append operation only if attachment is usable and underlying
  // demuxer is protected from destruction (applicable especially for
  // MSE-in-Worker case).
  DCHECK(!IsRemoved());  // So must have |source_| and it must have attachment.
  if (!source_->RunUnlessElementGoneOrClosingUs(WTF::Bind(
          &SourceBuffer::AppendBufferAsyncPart_Locked, WrapPersistent(this)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, behavior should be for this case. In this
    // implementation:
    // 1) If main context isn't destroyed yet, then there must be a pending
    // MediaSource::Close() call which will call RemovedFromMediaSource()
    // eventually if still safe to do so (and that will cleanly shutdown pending
    // async append state if we just do nothing here now, or
    // 2) If main context is destroyed, then our context will be destroyed soon.
    // We cannot safely access the underlying demuxer. So the safest thing to do
    // is nothing here now.
    DVLOG(1) << __func__ << " this=" << this
             << ": Worker MediaSource attachment is closing";
  }
}

void SourceBuffer::AppendBufferAsyncPart_Locked(
    MediaSourceAttachmentSupplement::ExclusiveKey pass_key) {
  DCHECK(source_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();
  DCHECK(updating_);

  // Section 3.5.4 Buffer Append Algorithm
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-buffer-append

  // 1. Run the segment parser loop algorithm.
  // Step 2 doesn't apply since we run Step 1 synchronously here.
  DCHECK_GE(pending_append_data_.size(), pending_append_data_offset_);
  wtf_size_t append_size =
      pending_append_data_.size() - pending_append_data_offset_;

  // Impose an arbitrary max size for a single append() call so that an append
  // doesn't block the renderer event loop very long. This value was selected
  // by looking at YouTube SourceBuffer usage across a variety of bitrates.
  // This value allows relatively large appends while keeping append() call
  // duration in the  ~5-15ms range. Note that even in MSE-in-Worker case, we
  // retain this behavior because some synchronous operations done by the main
  // thread media element on our attachment block until we are finished and have
  // exited the attachment's RunExclusively() callback scope.
  const wtf_size_t kMaxAppendSize = 128 * 1024;
  if (append_size > kMaxAppendSize)
    append_size = kMaxAppendSize;

  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "delay", TRACE_ID_LOCAL(this));
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "appending", TRACE_ID_LOCAL(this),
                                    "appendSize", append_size);

  // |zero| is used for 0 byte appends so we always have a valid pointer.
  // We need to convey all appends, even 0 byte ones to |m_webSourceBuffer|
  // so that it can clear its end of stream state if necessary.
  unsigned char zero = 0;
  unsigned char* append_data = &zero;
  if (append_size)
    append_data = pending_append_data_.data() + pending_append_data_offset_;

  bool append_success =
      web_source_buffer_->Append(append_data, append_size, &timestamp_offset_);

  if (!append_success) {
    pending_append_data_.clear();
    pending_append_data_offset_ = 0;
    AppendError(pass_key);
  } else {
    pending_append_data_offset_ += append_size;

    if (pending_append_data_offset_ < pending_append_data_.size()) {
      append_buffer_async_task_handle_ = PostCancellableTask(
          *GetExecutionContext()->GetTaskRunner(TaskType::kMediaElementEvent),
          FROM_HERE,
          WTF::Bind(&SourceBuffer::AppendBufferAsyncPart,
                    WrapPersistent(this)));
      TRACE_EVENT_NESTABLE_ASYNC_END0("media", "appending",
                                      TRACE_ID_LOCAL(this));
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "delay", TRACE_ID_LOCAL(this),
                                        "type", "nextPieceDelay");
      return;
    }

    // 3. Set the updating attribute to false.
    updating_ = false;
    pending_append_data_.clear();
    pending_append_data_offset_ = 0;

    // 4. Queue a task to fire a simple event named update at this SourceBuffer
    //    object.
    ScheduleEvent(event_type_names::kUpdate);

    // 5. Queue a task to fire a simple event named updateend at this
    //    SourceBuffer object.
    ScheduleEvent(event_type_names::kUpdateend);
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "appending", TRACE_ID_LOCAL(this));
  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "SourceBuffer::appendBuffer",
                                  TRACE_ID_LOCAL(this));

  double media_time = GetMediaTime();
  DVLOG(3) << __func__ << " done. this=" << this << " media_time=" << media_time
           << " buffered="
           << WebTimeRangesToString(web_source_buffer_->Buffered());
}

void SourceBuffer::RemoveAsyncPart() {
  // Do the async remove operation only if attachment is usable and underlying
  // demuxer is protected from destruction (applicable especially for
  // MSE-in-Worker case).
  DCHECK(!IsRemoved());  // So must have |source_| and it must have attachment.
  if (!source_->RunUnlessElementGoneOrClosingUs(WTF::Bind(
          &SourceBuffer::RemoveAsyncPart_Locked, WrapPersistent(this)))) {
    // TODO(https://crbug.com/878133): Determine in specification what the
    // specific, app-visible, behavior should be for this case. This
    // implementation takes the safest route and does nothing. See similar case
    // in AppendBufferAsyncPart for reasoning.
    DVLOG(1) << __func__ << " this=" << this
             << ": Worker MediaSource attachment is closing";
  }
}

void SourceBuffer::RemoveAsyncPart_Locked(
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  DCHECK(source_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();
  DCHECK(updating_);
  DCHECK_GE(pending_remove_start_, 0);
  DCHECK_LT(pending_remove_start_, pending_remove_end_);

  // Section 3.2 remove() method steps
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-remove-void-double-start-double-end

  // 9. Run the coded frame removal algorithm with start and end as the start
  //    and end of the removal range.
  web_source_buffer_->Remove(pending_remove_start_, pending_remove_end_);

  // 10. Set the updating attribute to false.
  updating_ = false;
  pending_remove_start_ = -1;
  pending_remove_end_ = -1;

  // 11. Queue a task to fire a simple event named update at this SourceBuffer
  //     object.
  ScheduleEvent(event_type_names::kUpdate);

  // 12. Queue a task to fire a simple event named updateend at this
  //     SourceBuffer object.
  ScheduleEvent(event_type_names::kUpdateend);
}

void SourceBuffer::AppendError(
    MediaSourceAttachmentSupplement::ExclusiveKey pass_key) {
  DVLOG(3) << __func__ << " this=" << this;
  DCHECK(source_);
  source_->AssertAttachmentsMutexHeldIfCrossThreadForDebugging();

  // Section 3.5.3 Append Error Algorithm
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-append-error

  // 1. Run the reset parser state algorithm.
  web_source_buffer_->ResetParserState();

  // 2. Set the updating attribute to false.
  updating_ = false;

  // 3. Queue a task to fire a simple event named error at this SourceBuffer
  //    object.
  ScheduleEvent(event_type_names::kError);

  // 4. Queue a task to fire a simple event named updateend at this SourceBuffer
  //    object.
  ScheduleEvent(event_type_names::kUpdateend);

  // 5. If decode error is true, then run the end of stream algorithm with the
  // error parameter set to "decode".
  source_->EndOfStreamAlgorithm(WebMediaSource::kEndOfStreamStatusDecodeError,
                                pass_key);
}

void SourceBuffer::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  visitor->Trace(track_defaults_);
  visitor->Trace(async_event_queue_);
  visitor->Trace(audio_tracks_);
  visitor->Trace(video_tracks_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
