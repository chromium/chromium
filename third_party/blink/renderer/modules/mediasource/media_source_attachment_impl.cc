// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/media_source_attachment_impl.h"

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

MediaSourceAttachmentImpl::MediaSourceAttachmentImpl(MediaSource* media_source)
    : registered_media_source_(media_source) {
  // This kind of attachment only operates on the main thread.
  DCHECK(IsMainThread());

  DVLOG(1) << __func__ << " this=" << this << " media_source=" << media_source;

  // Verify that at construction time, refcounting of this object begins at
  // precisely 1.
  DCHECK(HasOneRef());
}

MediaSourceAttachmentImpl::~MediaSourceAttachmentImpl() = default;

void MediaSourceAttachmentImpl::Unregister() {
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

MediaSourceTracer* MediaSourceAttachmentImpl::StartAttachingToMediaElement(
    HTMLMediaElement* element) {
  if (!registered_media_source_)
    return nullptr;

  return registered_media_source_->StartAttachingToMediaElement(element);
}

void MediaSourceAttachmentImpl::CompleteAttachingToMediaElement(
    MediaSourceTracer* tracer,
    std::unique_ptr<WebMediaSource> web_media_source) {
  GetMediaSource(tracer)->CompleteAttachingToMediaElement(
      std::move(web_media_source));
}

void MediaSourceAttachmentImpl::Close(MediaSourceTracer* tracer) {
  GetMediaSource(tracer)->Close();
}

bool MediaSourceAttachmentImpl::IsClosed(MediaSourceTracer* tracer) const {
  return GetMediaSource(tracer)->IsClosed();
}

double MediaSourceAttachmentImpl::duration(MediaSourceTracer* tracer) const {
  return GetMediaSource(tracer)->duration();
}

WebTimeRanges MediaSourceAttachmentImpl::BufferedInternal(
    MediaSourceTracer* tracer) const {
  return GetMediaSource(tracer)->BufferedInternal();
}

WebTimeRanges MediaSourceAttachmentImpl::SeekableInternal(
    MediaSourceTracer* tracer) const {
  return GetMediaSource(tracer)->SeekableInternal();
}

TimeRanges* MediaSourceAttachmentImpl::Buffered(
    MediaSourceTracer* tracer) const {
  return GetMediaSource(tracer)->Buffered();
}

void MediaSourceAttachmentImpl::OnTrackChanged(MediaSourceTracer* tracer,
                                               TrackBase* track) {
  GetMediaSource(tracer)->OnTrackChanged(track);
}

}  // namespace blink
