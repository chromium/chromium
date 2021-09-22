// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_EVENT_PAGE_SHOW_PERSISTED_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_EVENT_PAGE_SHOW_PERSISTED_H_

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// The type of pageshow events. These values must be synced with
// EventPageShowPersisted in enum.xml. Do not renumber these values.
enum class EventPageShowPersisted {
  // The pageshow event is recorded without persisted flag in renderer.
  kNoInRenderer = 0,

  // The pageshow event is recorded with persisted flag in renderer.
  kYesInRenderer = 1,

  // Browser triggers a pageshow event with persisted flag. The recorded count
  // should be almost the same as kYesInRenderer. See crbug.com/1234634.
  kYesInBrowser = 2,

  // There is not kNoInBrowser as we don't have to compare the counts of
  // pageshow events without persisted between browser and renderer so far.

  kMaxValue = kYesInBrowser,
};

BLINK_COMMON_EXPORT void RecordUMAEventPageShowPersisted(
    EventPageShowPersisted value);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_EVENT_PAGE_SHOW_PERSISTED_H_
