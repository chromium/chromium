// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/same_thread_media_source_attachment.h"

#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_tracer_impl.h"

namespace {
// Downcasts |tracer| to the expected same-thread attachment's tracer type.
// Includes a debug-mode check that the tracer matches the expected attachment
// semantic.
blink::MediaSourceTracerImpl* GetTracerImpl(blink::MediaSourceTracer* tracer) {
  DCHECK(!tracer || !tracer->IsCrossThreadForDebugging());
  return static_cast<blink::MediaSourceTracerImpl*>(tracer);
}

blink::MediaSource* GetMediaSource(blink::MediaSourceTracer* tracer) {
  return GetTracerImpl(tracer)->GetMediaSource();
}

}  // namespace

namespace blink {

SameThreadMediaSourceAttachment::SameThreadMediaSourceAttachment(
    MediaSource* media_source)
    : registered_media_source_(media_source) {
  // This kind of attachment only operates on the main thread.
  DCHECK(IsMainThread());

  DVLOG(1) << __func__ << " this=" << this << " media_source=" << media_source;

  // Verify that at construction time, refcounting of this object begins at
  // precisely 1.
  DCHECK(HasOneRef());
}

SameThreadMediaSourceAttachment::~SameThreadMediaSourceAttachment() = default;

void SameThreadMediaSourceAttachment::Unregister() {
  DVLOG(1) << __func__ << " this=" << this;

  // The only expected caller is a MediaSourceRegistryImpl on the main thread.
  DCHECK(IsMainThread());

  // Release our strong reference to the MediaSource. Note that revokeObjectURL
  // of the url associated with this attachment could commonly follow this path
  // while the MediaSource (and any attachment to an HTMLMediaElement) may still
  // be alive/active.
  DCHECK(registered_media_source_);
  registered_media_source_ = nullptr;
}

MediaSourceTracer*
SameThreadMediaSourceAttachment::StartAttachingToMediaElement(
    HTMLMediaElement* element) {
  if (!registered_media_source_)
    return nullptr;

  return registered_media_source_->StartAttachingToMediaElement(element);
}

void SameThreadMediaSourceAttachment::CompleteAttachingToMediaElement(
    MediaSourceTracer* tracer,
    std::unique_ptr<WebMediaSource> web_media_source) {
  GetMediaSource(tracer)->CompleteAttachingToMediaElement(
      std::move(web_media_source));
}

void SameThreadMediaSourceAttachment::Close(MediaSourceTracer* tracer) {
  GetMediaSource(tracer)->Close();
}

bool SameThreadMediaSourceAttachment::IsClosed(
    MediaSourceTracer* tracer) const {
  return GetMediaSource(tracer)->IsClosed();
}

double SameThreadMediaSourceAttachment::duration(
    MediaSourceTracer* tracer) const {
  return GetMediaSource(tracer)->duration();
}

WebTimeRanges SameThreadMediaSourceAttachment::BufferedInternal(
    MediaSourceTracer* tracer) const {
  return GetMediaSource(tracer)->BufferedInternal();
}

WebTimeRanges SameThreadMediaSourceAttachment::SeekableInternal(
    MediaSourceTracer* tracer) const {
  return GetMediaSource(tracer)->SeekableInternal();
}

TimeRanges* SameThreadMediaSourceAttachment::Buffered(
    MediaSourceTracer* tracer) const {
  return GetMediaSource(tracer)->Buffered();
}

void SameThreadMediaSourceAttachment::OnTrackChanged(MediaSourceTracer* tracer,
                                                     TrackBase* track) {
  GetMediaSource(tracer)->OnTrackChanged(track);
}

}  // namespace blink
