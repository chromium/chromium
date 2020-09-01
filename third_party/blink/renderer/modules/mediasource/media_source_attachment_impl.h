// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_ATTACHMENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_ATTACHMENT_IMPL_H_

#include <memory>
#include "third_party/blink/public/platform/web_time_range.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/core/html/media/media_source_tracer.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class HTMLMediaElement;
class TimeRanges;
class TrackBase;
class WebMediaSource;

// Concrete attachment that supports operation only on the main thread.
class MediaSourceAttachmentImpl final : public MediaSourceAttachment {
 public:
  // The only intended caller of this constructor is
  // URLMediaSource::createObjectUrl. The raw pointer is then adopted into a
  // scoped_refptr in MediaSourceRegistryImpl::RegisterURL.
  explicit MediaSourceAttachmentImpl(MediaSource* media_source);

  void Unregister() override;

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
  ~MediaSourceAttachmentImpl() override;

  // Cache of the registered MediaSource. Retains strong reference until
  // Unregister() is called.
  Persistent<MediaSource> registered_media_source_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourceAttachmentImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_ATTACHMENT_IMPL_H_
