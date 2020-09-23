// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/cross_thread_media_source_attachment.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"

namespace blink {

CrossThreadMediaSourceAttachment::CrossThreadMediaSourceAttachment(
    MediaSource* media_source,
    util::PassKey<URLMediaSource> /* passkey */)
    : registered_media_source_(media_source) {
  // This kind of attachment can only be constructed by the worker thread.
  DCHECK(!IsMainThread());

  DVLOG(1) << __func__ << " this=" << this << " media_source=" << media_source;

  // Verify that at construction time, refcounting of this object begins at
  // precisely 1.
  DCHECK(HasOneRef());
}

CrossThreadMediaSourceAttachment::~CrossThreadMediaSourceAttachment() {
  DVLOG(1) << __func__ << " this=" << this;
}

void CrossThreadMediaSourceAttachment::NotifyDurationChanged(
    MediaSourceTracer* tracer,
    double duration) {
  // Called only by the MSE API on worker thread.
  DCHECK(!IsMainThread());

  DVLOG(1) << __func__ << " this=" << this;

  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
}

double CrossThreadMediaSourceAttachment::GetRecentMediaTime(
    MediaSourceTracer* tracer) {
  // Called only by the MSE API on worker thread.
  DCHECK(!IsMainThread());

  DVLOG(1) << __func__ << " this=" << this;

  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
  return 0.0;
}

bool CrossThreadMediaSourceAttachment::GetElementError(
    MediaSourceTracer* tracer) {
  // Called only by the MSE API on worker thread.
  DCHECK(!IsMainThread());

  DVLOG(1) << __func__ << " this=" << this;

  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
  return true;
}

void CrossThreadMediaSourceAttachment::Unregister() {
  DVLOG(1) << __func__ << " this=" << this
           << ", IsMainThread=" << IsMainThread();

  // The only expected caller is a MediaSourceRegistryImpl on the main thread
  // (or possibly on the worker thread, if MediaSourceInWorkers is enabled).
  DCHECK(IsMainThread() ||
         RuntimeEnabledFeatures::MediaSourceInWorkersEnabled());

  // Release our strong reference to the MediaSource. Note that revokeObjectURL
  // of the url associated with this attachment could commonly follow this path
  // while the MediaSource (and any attachment to an HTMLMediaElement) may still
  // be alive/active. Also note that |registered_media_source_| could be
  // incorrectly cleared already if its owner's execution context destruction
  // has completed without notifying us, hence careful locking in
  // MediaSourceRegistryImpl around this scenario, and allowance for us to be
  // called on the worker context. Locking there instead of cross-thread posting
  // to the main thread to reach us enables stability in cases where worker's
  // context destruction or explicit object URL revocation from worker context
  // races attempted usage of the object URL (or |registered_media_source_|
  // here).
  DCHECK(registered_media_source_);
  registered_media_source_ = nullptr;
}

MediaSourceTracer*
CrossThreadMediaSourceAttachment::StartAttachingToMediaElement(
    HTMLMediaElement* element,
    bool* success) {
  // Called only by the media element on main thread.
  DCHECK(IsMainThread());

  DCHECK(success);

  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
  *success = false;
  return nullptr;
}

void CrossThreadMediaSourceAttachment::CompleteAttachingToMediaElement(
    MediaSourceTracer* tracer,
    std::unique_ptr<WebMediaSource> web_media_source) {
  // Called only by the media element on main thread.
  DCHECK(IsMainThread());

  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
}

void CrossThreadMediaSourceAttachment::Close(MediaSourceTracer* tracer) {
  // Called only by the media element on main thread.
  DCHECK(IsMainThread());

  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
}

WebTimeRanges CrossThreadMediaSourceAttachment::BufferedInternal(
    MediaSourceTracer* tracer) const {
  // Called only by the media element on main thread.
  DCHECK(IsMainThread());

  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
  return {};
}

WebTimeRanges CrossThreadMediaSourceAttachment::SeekableInternal(
    MediaSourceTracer* tracer) const {
  // Called only by the media element on main thread.
  DCHECK(IsMainThread());

  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
  return {};
}

void CrossThreadMediaSourceAttachment::OnTrackChanged(MediaSourceTracer* tracer,
                                                      TrackBase* track) {
  // Called only by the media element on main thread.
  DCHECK(IsMainThread());

  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
}

void CrossThreadMediaSourceAttachment::OnElementTimeUpdate(double time) {
  // Called only by the media element on main thread.
  DCHECK(IsMainThread());

  DVLOG(3) << __func__ << " this=" << this << ", time=" << time;
  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
}

void CrossThreadMediaSourceAttachment::OnElementError() {
  // Called only by the media element on main thread.
  DCHECK(IsMainThread());

  DVLOG(3) << __func__ << " this=" << this;
  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
}

void CrossThreadMediaSourceAttachment::OnElementContextDestroyed() {
  // Called only by the media element on main thread.
  DCHECK(IsMainThread());

  DVLOG(3) << __func__ << " this=" << this;
  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
}

void CrossThreadMediaSourceAttachment::OnMediaSourceContextDestroyed() {
  // Called only by the MSE API on worker thread.
  DCHECK(!IsMainThread());

  DVLOG(3) << __func__ << " this=" << this;
  // TODO(https://crbug.com/878133): Implement cross-thread behavior for this.
  NOTIMPLEMENTED();
}

}  // namespace blink
