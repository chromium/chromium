// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_TRACER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_TRACER_IMPL_H_

#include "third_party/blink/renderer/core/html/media/media_source_tracer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class HTMLMediaElement;
class MediaSource;

// Concrete MediaSourceTracer that enables an HTMLMediaElement and its attached
// MediaSource on the same (main) thread to trace into each other. This enables
// garbage collection to automatically detect and collect idle attachments of
// these objects that have no other strong references.
// A MediaSourceAttachmentImpl uses a MediaSourceTracerImpl as the authoritative
// reference holder for each side of the attachment.
class MediaSourceTracerImpl final : public MediaSourceTracer {
 public:
  MediaSourceTracerImpl(HTMLMediaElement* media_element,
                        MediaSource* media_source);
  ~MediaSourceTracerImpl() override = default;

  void Trace(Visitor* visitor) const override;

  bool IsCrossThreadForDebugging() const override { return false; }

  HTMLMediaElement* GetMediaElement() { return media_element_; }

  MediaSource* GetMediaSource() { return media_source_; }

 private:
  Member<HTMLMediaElement> media_element_;
  Member<MediaSource> media_source_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_TRACER_IMPL_H_
