// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_TRIGGERING_EVENT_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_TRIGGERING_EVENT_INFO_H_

namespace blink {

// Extra info sometimes associated with a navigation.
enum class TriggeringEventInfo {
  kUnknown,

  // The navigation was not triggered via a JS Event.
  kNotFromEvent,

  // The navigation was triggered via a JS event with isTrusted() == true.
  kFromTrustedEvent,

  // The navigation was triggered via a JS event with isTrusted() == false.
  kFromUntrustedEvent,

  kMaxValue = kFromUntrustedEvent,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_TRIGGERING_EVENT_INFO_H_
