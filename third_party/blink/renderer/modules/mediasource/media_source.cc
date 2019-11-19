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

#include "third_party/blink/renderer/modules/mediasource/media_source.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "media/base/logging_override_if_enabled.h"
#include "third_party/blink/public/platform/web_media_source.h"
#include "third_party/blink/public/platform/web_source_buffer.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/html/track/video_track_list.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_registry.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer_track_base_supplement.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

using blink::WebMediaSource;
using blink::WebSourceBuffer;

namespace blink {

namespace {
// These values are written to logs. New enum values can be added, but existing
// ones must never be renumbered or deleted and reused.
enum class MseExecutionContext {
  kWindow = 0,
  kDedicatedWorker = 1,
  kSharedWorker = 2,
  kMax = kSharedWorker
};
}  // namespace

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
    MediaSource::LogAndThrowDOMException(exception_state,
                                         DOMExceptionCode::kInvalidStateError,
                                         "The 'updating' attribute is true on "
                                         "one or more of this MediaSource's "
                                         "SourceBuffers.");
    return true;
  }

  return false;
}

const AtomicString& MediaSource::OpenKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, open, ("open"));
  return open;
}

const AtomicString& MediaSource::ClosedKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, closed, ("closed"));
  return closed;
}

const AtomicString& MediaSource::EndedKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, ended, ("ended"));
  return ended;
}

MediaSource* MediaSource::Create(ExecutionContext* context) {
  return MakeGarbageCollected<MediaSource>(context);
}

MediaSource::MediaSource(ExecutionContext* context)
    : ContextLifecycleObserver(context),
      ready_state_(ClosedKeyword()),
      async_event_queue_(
          MakeGarbageCollected<EventQueue>(context,
                                           TaskType::kMediaElementEvent)),
      attached_element_(nullptr),
      source_buffers_(
          MakeGarbageCollected<SourceBufferList>(GetExecutionContext(),
                                                 async_event_queue_.Get())),
      active_source_buffers_(
          MakeGarbageCollected<SourceBufferList>(GetExecutionContext(),
                                                 async_event_queue_.Get())),
      live_seekable_range_(MakeGarbageCollected<TimeRanges>()),
      added_to_registry_counter_(0) {
  DVLOG(1) << __func__ << " this=" << this;

  DCHECK(RuntimeEnabledFeatures::MediaSourceInWorkersEnabled() ||
         IsMainThread());

  MseExecutionContext type = MseExecutionContext::kWindow;
  if (!IsMainThread()) {
    if (context->IsDedicatedWorkerGlobalScope())
      type = MseExecutionContext::kDedicatedWorker;
    else if (context->IsSharedWorkerGlobalScope())
      type = MseExecutionContext::kSharedWorker;
    else
      CHECK(false) << "Invalid execution context for MSE usage";
  }
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, mse_execution_context_histogram,
      ("Media.MSE.ExecutionContext",
       static_cast<int>(MseExecutionContext::kMax) + 1));
  mse_execution_context_histogram.Count(static_cast<int>(type));

  // TODO(wolenetz): Actually enable experimental usage of MediaSource from
  // dedicated and shared worker contexts. See https://crbug.com/878133.
  CHECK(type == MseExecutionContext::kWindow)
      << "MSE is not yet supported from workers";
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
  if (type.IsEmpty()) {
    LogAndThrowTypeError(exception_state, "The type provided is empty");
    return nullptr;
  }

  // 2. If type contains a MIME type that is not supported ..., then throw a
  // NotSupportedError exception and abort these steps.
  //
  // TODO(wolenetz): Refactor and use a less-strict version of isTypeSupported
  // here. As part of that, CreateWebSourceBuffer in Chromium should inherit
  // relaxation of impl's StreamParserFactory (since it returns false if a
  // stream parser can't be constructed with |type|). See
  // https://crbug.com/535738.
  if (!isTypeSupported(type)) {
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

  // 5. Create a new SourceBuffer object and associated resources.
  ContentType content_type(type);
  String codecs = content_type.Parameter("codecs");
  std::unique_ptr<WebSourceBuffer> web_source_buffer =
      CreateWebSourceBuffer(content_type.GetType(), codecs, exception_state);

  if (!web_source_buffer) {
    DCHECK(exception_state.CodeAs<DOMExceptionCode>() ==
               DOMExceptionCode::kNotSupportedError ||
           exception_state.CodeAs<DOMExceptionCode>() ==
               DOMExceptionCode::kQuotaExceededError);
    // 2. If type contains a MIME type that is not supported ..., then throw a
    //    NotSupportedError exception and abort these steps.
    // 3. If the user agent can't handle any more SourceBuffer objects then
    //    throw a QuotaExceededError exception and abort these steps
    return nullptr;
  }

  bool generate_timestamps_flag =
      web_source_buffer->GetGenerateTimestampsFlag();

  SourceBuffer* buffer = SourceBuffer::Create(std::move(web_source_buffer),
                                              this, async_event_queue_.Get());
  // 8. Add the new object to sourceBuffers and queue a simple task to fire a
  //    simple event named addsourcebuffer at sourceBuffers.
  source_buffers_->Add(buffer);

  // Steps 6 and 7 (Set the SourceBuffer's mode attribute based on the byte
  // stream format's generate timestamps flag). We do this after adding to
  // sourceBuffers (step 8) to enable direct reuse of the setMode() logic here,
  // which depends on |buffer| being in |source_buffers_| in our
  // implementation.
  if (generate_timestamps_flag) {
    buffer->setMode(SourceBuffer::SequenceKeyword(), exception_state);
  } else {
    buffer->setMode(SourceBuffer::SegmentsKeyword(), exception_state);
  }

  // 9. Return the new object to the caller.
  DVLOG(3) << __func__ << " this=" << this << " type=" << type << " -> "
           << buffer;
  return buffer;
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
}

void MediaSource::OnReadyStateChange(const AtomicString& old_state,
                                     const AtomicString& new_state) {
  if (IsOpen()) {
    ScheduleEvent(event_type_names::kSourceopen);
    return;
  }

  if (old_state == OpenKeyword() && new_state == EndedKeyword()) {
    ScheduleEvent(event_type_names::kSourceended);
    return;
  }

  DCHECK(IsClosed());

  active_source_buffers_->Clear();

  // Clear SourceBuffer references to this object.
  for (unsigned i = 0; i < source_buffers_->length(); ++i)
    source_buffers_->item(i)->RemovedFromMediaSource();
  source_buffers_->Clear();

  attached_element_.Clear();

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

bool MediaSource::isTypeSupported(const String& type) {
  // Section 2.2 isTypeSupported() method steps.
  // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-MediaSource-isTypeSupported-boolean-DOMString-type
  // 1. If type is an empty string, then return false.
  if (type.IsEmpty()) {
    DVLOG(1) << __func__ << "(" << type << ") -> false (empty input)";
    return false;
  }

  ContentType content_type(type);
  String codecs = content_type.Parameter("codecs");

  // 2. If type does not contain a valid MIME type string, then return false.
  if (content_type.GetType().IsEmpty()) {
    DVLOG(1) << __func__ << "(" << type << ") -> false (invalid mime type)";
    return false;
  }

  // Note: MediaSource.isTypeSupported() returning true implies that
  // HTMLMediaElement.canPlayType() will return "maybe" or "probably" since it
  // does not make sense for a MediaSource to support a type the
  // HTMLMediaElement knows it cannot play.
  if (HTMLMediaElement::GetSupportsType(content_type) ==
      MIMETypeRegistry::kIsNotSupported) {
    DVLOG(1) << __func__ << "(" << type
             << ") -> false (not supported by HTMLMediaElement)";
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
  // false, complying with the non-normative guidance being incubated for the
  // MSE vNext codec switching feature at
  // https://github.com/WICG/media-source/tree/codec-switching.
  // TODO(wolenetz): Relaxed codec specificity following similar non-normative
  // guidance will soon be allowed for addSourceBuffer and changeType methods,
  // but this strict codec specificity is and will be retained for
  // isTypeSupported. See https://crbug.com/535738
  bool result = MIMETypeRegistry::kIsSupported ==
                MIMETypeRegistry::SupportsMediaSourceMIMEType(
                    content_type.GetType(), codecs);
  DVLOG(2) << __func__ << "(" << type << ") -> " << (result ? "true" : "false");
  return result;
}

const AtomicString& MediaSource::InterfaceName() const {
  return event_target_names::kMediaSource;
}

ExecutionContext* MediaSource::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void MediaSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(async_event_queue_);
  visitor->Trace(attached_element_);
  visitor->Trace(source_buffers_);
  visitor->Trace(active_source_buffers_);
  visitor->Trace(live_seekable_range_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void MediaSource::SetWebMediaSourceAndOpen(
    std::unique_ptr<WebMediaSource> web_media_source) {
  TRACE_EVENT_ASYNC_END0("media", "MediaSource::attachToElement", this);
  DCHECK(web_media_source);
  DCHECK(!web_media_source_);
  DCHECK(attached_element_);
  web_media_source_ = std::move(web_media_source);
  SetReadyState(OpenKeyword());
}

void MediaSource::AddedToRegistry() {
  ++added_to_registry_counter_;
  // Ensure there's no counter overflow.
  CHECK_GT(added_to_registry_counter_, 0);
}

void MediaSource::RemovedFromRegistry() {
  DCHECK_GT(added_to_registry_counter_, 0);
  --added_to_registry_counter_;
}

double MediaSource::duration() const {
  return IsClosed() ? std::numeric_limits<float>::quiet_NaN()
                    : web_media_source_->Duration();
}

WebTimeRanges MediaSource::BufferedInternal() const {
  // Implements MediaSource algorithm for HTMLMediaElement.buffered.
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#htmlmediaelement-extensions
  Vector<WebTimeRanges> ranges(active_source_buffers_->length());
  for (unsigned i = 0; i < active_source_buffers_->length(); ++i)
    ranges[i] = active_source_buffers_->item(i)->buffered();

  WebTimeRanges intersection_ranges;

  // 1. If activeSourceBuffers.length equals 0 then return an empty TimeRanges
  //    object and abort these steps.
  if (ranges.IsEmpty())
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
  bool ended = readyState() == EndedKeyword();
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

TimeRanges* MediaSource::Buffered() const {
  return MakeGarbageCollected<TimeRanges>(BufferedInternal());
}

WebTimeRanges MediaSource::SeekableInternal() const {
  DCHECK(attached_element_)
      << "Seekable should only be used when attached to HTMLMediaElement";

  // Implements MediaSource algorithm for HTMLMediaElement.seekable.
  // http://w3c.github.io/media-source/#htmlmediaelement-extensions
  WebTimeRanges ranges;

  double source_duration = duration();
  // If duration equals NaN: Return an empty TimeRanges object.
  if (std::isnan(source_duration))
    return ranges;

  // If duration equals positive Infinity:
  if (source_duration == std::numeric_limits<double>::infinity()) {
    WebTimeRanges buffered = BufferedInternal();

    // 1. If live seekable range is not empty:
    if (live_seekable_range_->length() != 0) {
      // 1.1. Let union ranges be the union of live seekable range and the
      //      HTMLMediaElement.buffered attribute.
      // 1.2. Return a single range with a start time equal to the
      //      earliest start time in union ranges and an end time equal to
      //      the highest end time in union ranges and abort these steps.
      if (buffered.empty()) {
        ranges.emplace_back(live_seekable_range_->start(0, ASSERT_NO_EXCEPTION),
                            live_seekable_range_->end(0, ASSERT_NO_EXCEPTION));
        return ranges;
      }

      ranges.emplace_back(
          std::min(live_seekable_range_->start(0, ASSERT_NO_EXCEPTION),
                   buffered.front().start),
          std::max(live_seekable_range_->end(0, ASSERT_NO_EXCEPTION),
                   buffered.back().end));
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
  DCHECK(HTMLMediaElement::MediaTracksEnabledInternally());
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
  DurationChangeAlgorithm(duration, exception_state);
}

void MediaSource::DurationChangeAlgorithm(double new_duration,
                                          ExceptionState& exception_state) {
  // http://w3c.github.io/media-source/#duration-change-algorithm
  // 1. If the current value of duration is equal to new duration, then return.
  if (new_duration == duration())
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
          exception_state, DOMExceptionCode::kInvalidStateError,
          "Setting duration below highest presentation timestamp of any "
          "buffered coded frames is disallowed. Instead, first do asynchronous "
          "remove(newDuration, oldDuration) on all sourceBuffers, where "
          "newDuration < oldDuration.");
      return;
    }

    Deprecation::CountDeprecation(
        attached_element_->GetDocument(),
        WebFeature::kMediaSourceDurationTruncatingBuffered);
    // See also deprecated remove(new duration, old duration) behavior below.
  }

  // 3. Set old duration to the current value of duration.
  double old_duration = duration();
  DCHECK_LE(highest_buffered_presentation_timestamp,
            std::isnan(old_duration) ? 0 : old_duration);

  // 4. Update duration to new duration.
  bool request_seek = attached_element_->currentTime() > new_duration;
  web_media_source_->SetDuration(new_duration);

  if (!RuntimeEnabledFeatures::MediaSourceNewAbortAndDurationEnabled() &&
      new_duration < old_duration) {
    // Deprecated behavior: if the new duration is less than old duration,
    // then call remove(new duration, old duration) on all all objects in
    // sourceBuffers.
    for (unsigned i = 0; i < source_buffers_->length(); ++i)
      source_buffers_->item(i)->remove(new_duration, old_duration,
                                       ASSERT_NO_EXCEPTION);
  }

  // 5. If a user agent is unable to partially render audio frames or text cues
  //    that start before and end after the duration, then run the following
  //    steps:
  //    NOTE: Currently we assume that the media engine is able to render
  //    partial frames/cues. If a media engine gets added that doesn't support
  //    this, then we'll need to add logic to handle the substeps.

  // 6. Update the media controller duration to new duration and run the
  //    HTMLMediaElement duration change algorithm.
  attached_element_->DurationChanged(new_duration, request_seek);
}

void MediaSource::SetReadyState(const AtomicString& state) {
  DCHECK(state == OpenKeyword() || state == ClosedKeyword() ||
         state == EndedKeyword());

  AtomicString old_state = readyState();
  DVLOG(3) << __func__ << " this=" << this << " : " << old_state << " -> "
           << state;

  if (state == ClosedKeyword()) {
    web_media_source_.reset();
  }

  if (old_state == state)
    return;

  ready_state_ = state;

  OnReadyStateChange(old_state, state);
}

void MediaSource::endOfStream(const AtomicString& error,
                              ExceptionState& exception_state) {
  DEFINE_STATIC_LOCAL(const AtomicString, network, ("network"));
  DEFINE_STATIC_LOCAL(const AtomicString, decode, ("decode"));

  DVLOG(3) << __func__ << " this=" << this << " : error=" << error;

  // https://www.w3.org/TR/media-source/#dom-mediasource-endofstream
  // 1. If the readyState attribute is not in the "open" state then throw an
  //    InvalidStateError exception and abort these steps.
  // 2. If the updating attribute equals true on any SourceBuffer in
  //    sourceBuffers, then throw an InvalidStateError exception and abort these
  //    steps.
  if (ThrowExceptionIfClosedOrUpdating(IsOpen(), IsUpdating(), exception_state))
    return;

  // 3. Run the end of stream algorithm with the error parameter set to error.
  if (error == network)
    EndOfStreamAlgorithm(WebMediaSource::kEndOfStreamStatusNetworkError);
  else if (error == decode)
    EndOfStreamAlgorithm(WebMediaSource::kEndOfStreamStatusDecodeError);
  else  // "" is allowed internally but not by IDL bindings.
    EndOfStreamAlgorithm(WebMediaSource::kEndOfStreamStatusNoError);
}

void MediaSource::endOfStream(ExceptionState& exception_state) {
  endOfStream("", exception_state);
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

  // 4. Set live seekable range to be a new normalized TimeRanges object
  //    containing a single range whose start position is start and end
  //    position is end.
  live_seekable_range_ = MakeGarbageCollected<TimeRanges>(start, end);
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

  // 3. If live seekable range contains a range, then set live seekable range
  //    to be a new empty TimeRanges object.
  if (live_seekable_range_->length() != 0)
    live_seekable_range_ = MakeGarbageCollected<TimeRanges>();
}

bool MediaSource::IsOpen() const {
  return readyState() == OpenKeyword();
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

HTMLMediaElement* MediaSource::MediaElement() const {
  return attached_element_.Get();
}

void MediaSource::EndOfStreamAlgorithm(
    const WebMediaSource::EndOfStreamStatus eos_status) {
  // https://www.w3.org/TR/media-source/#end-of-stream-algorithm
  // 1. Change the readyState attribute value to "ended".
  // 2. Queue a task to fire a simple event named sourceended at the
  //    MediaSource.
  SetReadyState(EndedKeyword());

  // 3. Do various steps based on |eos_status|.
  web_media_source_->MarkEndOfStream(eos_status);

  if (eos_status == WebMediaSource::kEndOfStreamStatusNoError) {
    // The implementation may not have immediately informed the
    // |attached_element_| of the potentially reduced duration. Prevent
    // app-visible duration race by synchronously running the duration change
    // algorithm. The MSE spec supports this:
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
    double new_duration = duration();
    bool request_seek = attached_element_->currentTime() > new_duration;
    attached_element_->DurationChanged(new_duration, request_seek);
  }
}

bool MediaSource::IsClosed() const {
  return readyState() == ClosedKeyword();
}

void MediaSource::Close() {
  SetReadyState(ClosedKeyword());
}

bool MediaSource::AttachToElement(HTMLMediaElement* element) {
  if (attached_element_)
    return false;

  DCHECK(IsClosed());

  TRACE_EVENT_ASYNC_BEGIN0("media", "MediaSource::attachToElement", this);
  attached_element_ = element;
  return true;
}

void MediaSource::OpenIfInEndedState() {
  if (ready_state_ != EndedKeyword())
    return;

  SetReadyState(OpenKeyword());
  web_media_source_->UnmarkEndOfStream();
}

bool MediaSource::HasPendingActivity() const {
  // Note that an unrevoked MediaSource objectUrl for an otherwise inactive,
  // unreferenced HTMLME with MSE still attached will prevent GC of the whole
  // group of objects. This is unfortunate, because it's conceivable that the
  // app may actually still have a "reference" to the underlying MediaSource if
  // it has the objectUrl in a string somewhere, for example. This is yet
  // further motivation for apps to properly revokeObjectUrl and for the MSE
  // spec, implementations and API users to transition to using HTMLME srcObject
  // for MSE attachment instead of objectUrl.
  return async_event_queue_->HasPendingEvents() ||
         added_to_registry_counter_ > 0;
}

void MediaSource::ContextDestroyed(ExecutionContext*) {
  if (!IsClosed())
    SetReadyState(ClosedKeyword());
  web_media_source_.reset();
}

std::unique_ptr<WebSourceBuffer> MediaSource::CreateWebSourceBuffer(
    const String& type,
    const String& codecs,
    ExceptionState& exception_state) {
  WebSourceBuffer* web_source_buffer = nullptr;

  switch (
      web_media_source_->AddSourceBuffer(type, codecs, &web_source_buffer)) {
    case WebMediaSource::kAddStatusOk:
      return base::WrapUnique(web_source_buffer);
    case WebMediaSource::kAddStatusNotSupported:
      DCHECK(!web_source_buffer);
      // 2.2
      // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
      // Step 2: If type contains a MIME type ... that is not supported with the
      // types specified for the other SourceBuffer objects in sourceBuffers,
      // then throw a NotSupportedError exception and abort these steps.
      LogAndThrowDOMException(
          exception_state, DOMExceptionCode::kNotSupportedError,
          "The type provided ('" + type + "') is not supported.");
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

  NOTREACHED();
  return nullptr;
}

void MediaSource::ScheduleEvent(const AtomicString& event_name) {
  DCHECK(async_event_queue_);

  Event* event = Event::Create(event_name);
  event->SetTarget(this);

  async_event_queue_->EnqueueEvent(FROM_HERE, *event);
}

URLRegistry& MediaSource::Registry() const {
  return MediaSourceRegistry::Registry();
}

}  // namespace blink
