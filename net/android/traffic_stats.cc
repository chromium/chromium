// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/traffic_stats.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "net/net_jni_headers/AndroidTrafficStats_jni.h"

namespace net::android::traffic_stats {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
enum TrafficStatsError {
  // Value returned by AndroidTrafficStats APIs when a valid value is
  // unavailable.
  ERROR_NOT_SUPPORTED = 0,
};

bool GetTotalTxBytes(int64_t* bytes) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  *bytes = Java_AndroidTrafficStats_getTotalTxBytes(env);
  return *bytes != ERROR_NOT_SUPPORTED;
}

bool GetTotalRxBytes(int64_t* bytes) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  *bytes = Java_AndroidTrafficStats_getTotalRxBytes(env);
  return *bytes != ERROR_NOT_SUPPORTED;
}

bool GetCurrentUidTxBytes(int64_t* bytes) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  *bytes = Java_AndroidTrafficStats_getCurrentUidTxBytes(env);
  return *bytes != ERROR_NOT_SUPPORTED;
}

bool GetCurrentUidRxBytes(int64_t* bytes) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  *bytes = Java_AndroidTrafficStats_getCurrentUidRxBytes(env);
  return *bytes != ERROR_NOT_SUPPORTED;
}

}  // namespace net::android::traffic_stats
