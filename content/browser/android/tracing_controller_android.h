// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_TRACING_CONTROLLER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_TRACING_CONTROLLER_ANDROID_H_

#include <set>

#include "base/android/jni_weak_ref.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace content {

// This class implements the native methods of TracingControllerAndroid.java
class TracingControllerAndroid {
 public:
  TracingControllerAndroid(JNIEnv* env, jobject obj);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  bool StartTracing(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& categories,
                    const base::android::JavaParamRef<jstring>& trace_options);
  void StopTracing(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jstring>& jfilepath,
                   bool compressfile,
                   const base::android::JavaParamRef<jobject>& callback);
  bool GetKnownCategoriesAsync(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& callback);
  bool GetTraceBufferUsageAsync(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& callback);
  static void GenerateTracingFilePath(base::FilePath* file_path);

 private:
  ~TracingControllerAndroid();
  void OnTracingStopped(
      const base::android::ScopedJavaGlobalRef<jobject>& callback);
  void OnKnownCategoriesReceived(
      const base::android::ScopedJavaGlobalRef<jobject>& callback,
      const std::set<std::string>& categories_received);
  void OnTraceBufferUsageReceived(
      const base::android::ScopedJavaGlobalRef<jobject>& callback,
      float percent_full,
      size_t approximate_event_count);

  JavaObjectWeakGlobalRef weak_java_object_;
  base::WeakPtrFactory<TracingControllerAndroid> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TracingControllerAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_TRACING_CONTROLLER_ANDROID_H_
