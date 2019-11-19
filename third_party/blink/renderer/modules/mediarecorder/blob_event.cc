// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/blob_event.h"

#include <cmath>

#include "third_party/blink/renderer/modules/mediarecorder/blob_event_init.h"

namespace blink {

// static
BlobEvent* BlobEvent::Create(const AtomicString& type,
                             const BlobEventInit* initializer) {
  return MakeGarbageCollected<BlobEvent>(type, initializer);
}

// static
BlobEvent* BlobEvent::Create(const AtomicString& type,
                             Blob* blob,
                             double timecode) {
  return MakeGarbageCollected<BlobEvent>(type, blob, timecode);
}

const AtomicString& BlobEvent::InterfaceName() const {
  return event_interface_names::kBlobEvent;
}

void BlobEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(blob_);
  Event::Trace(visitor);
}

BlobEvent::BlobEvent(const AtomicString& type, const BlobEventInit* initializer)
    : Event(type, initializer),
      blob_(initializer->data()),
      timecode_(initializer->hasTimecode() ? initializer->timecode() : NAN) {}

BlobEvent::BlobEvent(const AtomicString& type, Blob* blob, double timecode)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      blob_(blob),
      timecode_(timecode) {}

}  // namespace blink
