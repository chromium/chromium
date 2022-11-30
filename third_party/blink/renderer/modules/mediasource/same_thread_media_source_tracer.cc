// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/same_thread_media_source_tracer.h"

#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"

namespace blink {

SameThreadMediaSourceTracer::SameThreadMediaSourceTracer(
    HTMLMediaElement* media_element,
    MediaSource* media_source)
    : media_element_(media_element), media_source_(media_source) {}

void SameThreadMediaSourceTracer::Trace(Visitor* visitor) const {
  visitor->Trace(media_element_);
  visitor->Trace(media_source_);
  MediaSourceTracer::Trace(visitor);
}

}  // namespace blink
