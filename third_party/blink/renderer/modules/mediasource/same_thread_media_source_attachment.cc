// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/same_thread_media_source_attachment.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/modules/mediasource/same_thread_media_source_tracer.h"

namespace {
// Downcasts |tracer| to the expected same-thread attachment's tracer type.
// Includes a debug-mode check that the tracer matches the expected attachment
// semantic.
blink::SameThreadMediaSourceTracer* GetTracerImpl(
    blink::MediaSourceTracer* tracer) {
  DCHECK(!tracer || !tracer->IsCrossThreadForDebugging());
  return static_cast<blink::SameThreadMediaSourceTracer*>(tracer);
}

blink::MediaSource* GetMediaSource(blink::MediaSourceTracer* tracer) {
  return GetTracerImpl(tracer)->GetMediaSource();
}

blink::HTMLMediaElement* GetMediaElement(blink::MediaSourceTracer* tracer) {
  return GetTracerImpl(tracer)->GetMediaElement();
}

}  // namespace

namespace blink {

SameThreadMediaSourceAttachment::SameThreadMediaSourceAttachment(
    MediaSource* media_source)
    : MediaSourceAttachmentSupplement(media_source),
      recent_element_time_(0.0),
      element_has_error_(false),
      element_context_destroyed_(false),
      media_source_context_destroyed_(false) {
  // This kind of attachment only operates on the main thread.
  DCHECK(IsMainThread());

  DVLOG(1) << __func__ << " this=" << this << " media_source=" << media_source;

  // Verify that at construction time, refcounting of this object begins at
  // precisely 1.
  DCHECK(HasOneRef());
}

SameThreadMediaSourceAttachment::~SameThreadMediaSourceAttachment() {
  DVLOG(1) << __func__ << " this=" << this;
}

void SameThreadMediaSourceAttachment::NotifyDurationChanged(
    MediaSourceTracer* tracer,
    double duration) {
  DVLOG(1) << __func__ << " this=" << this;

  VerifyCalledWhileContextsAliveForDebugging();

  HTMLMediaElement* element = GetMediaElement(tracer);

  bool request_seek = element->currentTime() > duration;
  element->DurationChanged(duration, request_seek);
}

double SameThreadMediaSourceAttachment::GetRecentMediaTime(
    MediaSourceTracer* tracer) {
  DVLOG(1) << __func__ << " this=" << this;

  VerifyCalledWhileContextsAliveForDebugging();

  HTMLMediaElement* element = GetMediaElement(tracer);
  double result = element->currentTime();

  DVLOG(2) << __func__ << " this=" << this
           << " -> recent time=" << recent_element_time_
           << ", actual currentTime=" << result;
  return result;
}

bool SameThreadMediaSourceAttachment::GetElementError(
    MediaSourceTracer* tracer) {
  DVLOG(1) << __func__ << " this=" << this;

  VerifyCalledWhileContextsAliveForDebugging();

  HTMLMediaElement* element = GetMediaElement(tracer);
  bool current_element_error_state = !!element->error();

  DCHECK_EQ(current_element_error_state, element_has_error_);

  return current_element_error_state;
}

MediaSourceTracer*
SameThreadMediaSourceAttachment::StartAttachingToMediaElement(
    HTMLMediaElement* element) {
  VerifyCalledWhileContextsAliveForDebugging();

  if (!registered_media_source_)
    return nullptr;

  return registered_media_source_->StartAttachingToMediaElement(
      WrapRefCounted(this), element);
}

void SameThreadMediaSourceAttachment::CompleteAttachingToMediaElement(
    MediaSourceTracer* tracer,
    std::unique_ptr<WebMediaSource> web_media_source) {
  VerifyCalledWhileContextsAliveForDebugging();

  GetMediaSource(tracer)->CompleteAttachingToMediaElement(
      std::move(web_media_source));
}

void SameThreadMediaSourceAttachment::Close(MediaSourceTracer* tracer) {
  // The media element may have already notified us that its context is
  // destroyed, so VerifyCalledWhileContextIsAliveForDebugging() is unusable in
  // this scope.

  GetMediaSource(tracer)->Close();
}

double SameThreadMediaSourceAttachment::duration(
    MediaSourceTracer* tracer) const {
  VerifyCalledWhileContextsAliveForDebugging();

  return GetMediaSource(tracer)->duration();
}

WebTimeRanges SameThreadMediaSourceAttachment::BufferedInternal(
    MediaSourceTracer* tracer) const {
  VerifyCalledWhileContextsAliveForDebugging();

  return GetMediaSource(tracer)->BufferedInternal();
}

WebTimeRanges SameThreadMediaSourceAttachment::SeekableInternal(
    MediaSourceTracer* tracer) const {
  VerifyCalledWhileContextsAliveForDebugging();

  return GetMediaSource(tracer)->SeekableInternal();
}

TimeRanges* SameThreadMediaSourceAttachment::Buffered(
    MediaSourceTracer* tracer) const {
  VerifyCalledWhileContextsAliveForDebugging();

  return GetMediaSource(tracer)->Buffered();
}

void SameThreadMediaSourceAttachment::OnTrackChanged(MediaSourceTracer* tracer,
                                                     TrackBase* track) {
  // In this same thread implementation, the MSE side of the attachment can loop
  // back into this from SourceBuffer's initialization segment received
  // algorithm notifying the element, which then calls this. Regardless, we are
  // not called as part of execution context teardown, so verification should be
  // stable here.
  VerifyCalledWhileContextsAliveForDebugging();

  GetMediaSource(tracer)->OnTrackChanged(track);
}

void SameThreadMediaSourceAttachment::OnElementTimeUpdate(double time) {
  DVLOG(3) << __func__ << " this=" << this << ", time=" << time;

  VerifyCalledWhileContextsAliveForDebugging();

  recent_element_time_ = time;
}

void SameThreadMediaSourceAttachment::OnElementError() {
  DVLOG(3) << __func__ << " this=" << this;

  VerifyCalledWhileContextsAliveForDebugging();

  DCHECK(!element_has_error_)
      << "At most one transition to element error per attachment is expected";

  element_has_error_ = true;
}

void SameThreadMediaSourceAttachment::OnElementContextDestroyed() {
  DVLOG(3) << __func__ << " this=" << this;

  // We should only be notified once.
  DCHECK(!element_context_destroyed_);

  element_context_destroyed_ = true;
}

void SameThreadMediaSourceAttachment::OnMediaSourceContextDestroyed() {
  DVLOG(3) << __func__ << " this=" << this;

  // We should only be notified once.
  DCHECK(!element_context_destroyed_);

  media_source_context_destroyed_ = true;
}

void SameThreadMediaSourceAttachment::
    VerifyCalledWhileContextsAliveForDebugging() const {
  DCHECK(!element_context_destroyed_);
  DCHECK(!media_source_context_destroyed_);
}

}  // namespace blink
