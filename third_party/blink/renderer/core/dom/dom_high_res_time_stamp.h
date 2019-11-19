// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOM_HIGH_RES_TIME_STAMP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOM_HIGH_RES_TIME_STAMP_H_

#include "base/time/time.h"

namespace blink {

typedef double DOMHighResTimeStamp;

inline DOMHighResTimeStamp ConvertSecondsToDOMHighResTimeStamp(double seconds) {
  return static_cast<DOMHighResTimeStamp>(seconds * 1000.0);
}

inline double ConvertDOMHighResTimeStampToSeconds(
    DOMHighResTimeStamp milliseconds) {
  return milliseconds / 1000.0;
}

inline DOMHighResTimeStamp ConvertTimeTicksToDOMHighResTimeStamp(
    base::TimeTicks time) {
  return (time - base::TimeTicks()).InMillisecondsF();
}

inline DOMHighResTimeStamp ConvertTimeDeltaToDOMHighResTimeStamp(
    base::TimeDelta delta) {
  return delta.InMillisecondsF();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOM_HIGH_RES_TIME_STAMP_H_
