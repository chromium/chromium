// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_TOUCH_EVENT_DATA_H_
#define REMOTING_CLIENT_JNI_JNI_TOUCH_EVENT_DATA_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace remoting {

namespace protocol {
class TouchEventPoint;
}

class JniTouchEventData {
 public:
  JniTouchEventData();

  JniTouchEventData(const JniTouchEventData&) = delete;
  JniTouchEventData& operator=(const JniTouchEventData&) = delete;

  ~JniTouchEventData();

  // Copies touch point data from a Java object to a C++ object.
  static void CopyTouchPointData(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jobject>& java_object,
      protocol::TouchEventPoint* touch_event_point);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_TOUCH_EVENT_DATA_H_
