// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_ATTACHMENT_SUPPLEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_ATTACHMENT_SUPPLEMENT_H_

#include <memory>
#include "third_party/blink/public/platform/web_time_range.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/core/html/media/media_source_tracer.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class MediaSource;

// Modules-specific common extension of the core MediaSourceAttachment
// interface. Includes extra interface methods used by concrete attachments to
// communicate with the media element, as well as method implementations and
// members common to all concrete attachments.
class MediaSourceAttachmentSupplement : public MediaSourceAttachment {
 public:
  // Communicates a change in the media resource duration to the attached media
  // element. In a same-thread attachment, communicates this information
  // synchronously. In a cross-thread attachment, communicates asynchronously to
  // the media element. Same-thread synchronous notification here is primarily
  // to preserve compliance of API behavior when not using MSE-in-Worker
  // (setting MediaSource.duration should be synchronously in agreement with
  // subsequent retrieval of MediaElement.duration, all on the main thread).
  virtual void NotifyDurationChanged(MediaSourceTracer* tracer,
                                     double duration) = 0;
  // MediaSourceAttachment
  void Unregister() final;

 protected:
  explicit MediaSourceAttachmentSupplement(MediaSource* media_source);
  ~MediaSourceAttachmentSupplement() override;

  // Cache of the registered MediaSource. Retains strong reference from
  // construction of this object until Unregister() is called.
  Persistent<MediaSource> registered_media_source_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourceAttachmentSupplement);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_ATTACHMENT_SUPPLEMENT_H_
