// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_MOTION_EVENT_ACTION_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_MOTION_EVENT_ACTION_H_

namespace content {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.browser
// GENERATED_JAVA_PREFIX_TO_STRIP: MOTION_EVENT_ACTION_
enum MotionEventAction {
  MOTION_EVENT_ACTION_INVALID = -1,
  MOTION_EVENT_ACTION_START = 0,
  MOTION_EVENT_ACTION_MOVE = 1,
  MOTION_EVENT_ACTION_CANCEL = 2,
  MOTION_EVENT_ACTION_END = 3,
  MOTION_EVENT_ACTION_SCROLL = 4,
  MOTION_EVENT_ACTION_HOVER_ENTER = 5,
  MOTION_EVENT_ACTION_HOVER_EXIT = 6,
  MOTION_EVENT_ACTION_HOVER_MOVE = 7,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_MOTION_EVENT_ACTION_H_
