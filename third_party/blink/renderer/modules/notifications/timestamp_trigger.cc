// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/notifications/timestamp_trigger.h"

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

TimestampTrigger* TimestampTrigger::Create(const DOMTimeStamp& timestamp) {
  return MakeGarbageCollected<TimestampTrigger>(timestamp);
}

TimestampTrigger::TimestampTrigger(const DOMTimeStamp& timestamp)
    : timestamp_(timestamp) {}

}  // namespace blink
