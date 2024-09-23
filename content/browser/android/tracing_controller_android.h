// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_TRACING_CONTROLLER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_TRACING_CONTROLLER_ANDROID_H_

#include <set>

#include "base/android/jni_weak_ref.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"

namespace content {

// This class implements the native methods of TracingControllerAndroid.java
class TracingControllerAndroid {
 public:
  TracingControllerAndroid(JNIEnv* env, const jni_zero::JavaRef<jobject>& obj);

  TracingControllerAndroid(const TracingControllerAndroid&) = delete;
  TracingControllerAndroid& operator=(const TracingControllerAndroid&) = delete;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  bool StartTracing(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& categories,
                    const base::android::JavaParamRef<jstring>& trace_options,
                    bool use_protobuf);
  void StopTracing(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jstring>& jfilepath,
                   bool compress_file,
                   bool use_protobuf,
                   const base::android::JavaParamRef<jobject>& callback);
  bool GetKnownCategoriesAsync(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& callback);
  bool GetTraceBufferUsageAsync(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& callback);

  // Locate the appropriate directory to write the trace to and use it to
  // generate the path. |basename| might be empty, then TracingControllerAndroid
  // will generate an appropriate one as well.
  static base::FilePath GenerateTracingFilePath(const std::string& basename);

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
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_TRACING_CONTROLLER_ANDROID_H_
