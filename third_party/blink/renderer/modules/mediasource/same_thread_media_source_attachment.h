// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_SAME_THREAD_MEDIA_SOURCE_ATTACHMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_SAME_THREAD_MEDIA_SOURCE_ATTACHMENT_H_

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
  void NotifyDurationChanged(MediaSourceTracer* tracer,
                             double duration) override;

  // MediaSourceAttachment
  MediaSourceTracer* StartAttachingToMediaElement(HTMLMediaElement*) override;
  void CompleteAttachingToMediaElement(
      MediaSourceTracer* tracer,
      std::unique_ptr<WebMediaSource>) override;

  void Close(MediaSourceTracer* tracer) override;
  bool IsClosed(MediaSourceTracer* tracer) const override;
  double duration(MediaSourceTracer* tracer) const override;
  WebTimeRanges BufferedInternal(MediaSourceTracer* tracer) const override;
  WebTimeRanges SeekableInternal(MediaSourceTracer* tracer) const override;
  TimeRanges* Buffered(MediaSourceTracer* tracer) const override;
  void OnTrackChanged(MediaSourceTracer* tracer, TrackBase*) override;

 private:
  ~SameThreadMediaSourceAttachment() override;

  DISALLOW_COPY_AND_ASSIGN(SameThreadMediaSourceAttachment);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_SAME_THREAD_MEDIA_SOURCE_ATTACHMENT_H_
