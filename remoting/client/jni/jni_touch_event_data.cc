// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_touch_event_data.h"

#include "remoting/android/jni_headers/TouchEventData_jni.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

JniTouchEventData::JniTouchEventData() {}

JniTouchEventData::~JniTouchEventData() {}

// static
void JniTouchEventData::CopyTouchPointData(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& java_object,
    protocol::TouchEventPoint* touch_event_point) {
  touch_event_point->set_id(
      Java_TouchEventData_getTouchPointId(env, java_object));

  touch_event_point->set_x(
      Java_TouchEventData_getTouchPointX(env, java_object));

  touch_event_point->set_y(
      Java_TouchEventData_getTouchPointY(env, java_object));

  touch_event_point->set_radius_x(
      Java_TouchEventData_getTouchPointRadiusX(env, java_object));

  touch_event_point->set_radius_y(
      Java_TouchEventData_getTouchPointRadiusY(env, java_object));

  touch_event_point->set_angle(
      Java_TouchEventData_getTouchPointAngle(env, java_object));

  touch_event_point->set_pressure(
      Java_TouchEventData_getTouchPointPressure(env, java_object));
}

}  // namespace remoting
