// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_SAME_THREAD_MEDIA_SOURCE_ATTACHMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_SAME_THREAD_MEDIA_SOURCE_ATTACHMENT_H_

#include <memory>

#include "third_party/blink/public/platform/web_time_range.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_attachment_supplement.h"

namespace blink {

// Concrete attachment that supports operation only on the main thread.
class SameThreadMediaSourceAttachment final
    : public MediaSourceAttachmentSupplement {
 public:
  // The only intended caller of this constructor is
  // URLMediaSource::createObjectUrl. The raw pointer is then adopted into a
  // scoped_refptr in SameThreadMediaSourceRegistry::RegisterURL.
  explicit SameThreadMediaSourceAttachment(MediaSource* media_source);

  // MediaSourceAttachmentSupplement
  void NotifyDurationChanged(MediaSourceTracer* tracer, double duration) final;
  double GetRecentMediaTime(MediaSourceTracer* tracer) final;
  bool GetElementError(MediaSourceTracer* tracer) final;
  void OnMediaSourceContextDestroyed() final;

  // MediaSourceAttachment
  MediaSourceTracer* StartAttachingToMediaElement(HTMLMediaElement*,
                                                  bool* success) override;
  void CompleteAttachingToMediaElement(
      MediaSourceTracer* tracer,
      std::unique_ptr<WebMediaSource>) override;

  void Close(MediaSourceTracer* tracer) override;
  WebTimeRanges BufferedInternal(MediaSourceTracer* tracer) const override;
  WebTimeRanges SeekableInternal(MediaSourceTracer* tracer) const override;
  void OnTrackChanged(MediaSourceTracer* tracer, TrackBase*) override;

  void OnElementTimeUpdate(double time) final;
  void OnElementError() final;
  void OnElementContextDestroyed() final;

 private:
  ~SameThreadMediaSourceAttachment() override;

  // In this same thread implementation, if the media element context is
  // destroyed, then so should be the Media Source's context. This method
  // this precondition in debug mode.
  void VerifyCalledWhileContextsAliveForDebugging() const;

  // These are mostly used to verify correct behavior of the media element and
  // media source state pumping in debug builds. In a cross-thread attachment
  // implementation, state like this will be relied upon for servicing the
  // MediaSource API.
  double recent_element_time_;           // See OnElementTimeUpdate().
  bool element_has_error_;               // See OnElementError().
  bool element_context_destroyed_;       // See OnElementContextDestroyed().
  bool media_source_context_destroyed_;  // See OnMediaSourceContextDestroyed().

  DISALLOW_COPY_AND_ASSIGN(SameThreadMediaSourceAttachment);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_SAME_THREAD_MEDIA_SOURCE_ATTACHMENT_H_
