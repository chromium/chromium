// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_position.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/media_session/public/cpp/android/media_session_jni_headers/MediaPosition_jni.h"

using base::android::ScopedJavaLocalRef;

namespace media_session {

base::android::ScopedJavaLocalRef<jobject> MediaPosition::CreateJavaObject(
    JNIEnv* env) const {
  return Java_MediaPosition_create(
      env,
      (duration_.is_max() || duration_.is_zero()) ? -1
                                                  : duration_.InMilliseconds(),
      position_.InMilliseconds(), playback_rate_,
      // Pass raw uptime (TimeTicks) to match
      // android.os.SystemClock.uptimeMillis(). Converting to wall clock (Epoch)
      // here is unsafe because the offset between Uptime and Wall Time changes
      // during deep sleep, causing the media position to jump forward
      // incorrectly on wake.
      last_updated_time_.since_origin().InMilliseconds());
}

}  // namespace media_session

DEFINE_JNI(MediaPosition)
