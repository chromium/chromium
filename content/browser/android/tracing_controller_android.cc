// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/tracing_controller_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/android/content_jni_headers/TracingControllerAndroidImpl_jni.h"
#include "content/public/browser/tracing_controller.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;

namespace content {

static jlong JNI_TracingControllerAndroidImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  TracingControllerAndroid* profiler = new TracingControllerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(profiler);
}

TracingControllerAndroid::TracingControllerAndroid(JNIEnv* env, jobject obj)
    : weak_java_object_(env, obj) {}

TracingControllerAndroid::~TracingControllerAndroid() {}

void TracingControllerAndroid::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  delete this;
}

bool TracingControllerAndroid::StartTracing(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jcategories,
    const JavaParamRef<jstring>& jtraceoptions) {
  std::string categories =
      base::android::ConvertJavaStringToUTF8(env, jcategories);
  std::string options =
      base::android::ConvertJavaStringToUTF8(env, jtraceoptions);

  // This log is required by adb_profile_chrome.py.
  LOG(WARNING) << "Logging performance trace to file";

  return TracingController::GetInstance()->StartTracing(
      base::trace_event::TraceConfig(categories, options),
      TracingController::StartTracingDoneCallback());
}

void TracingControllerAndroid::StopTracing(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jfilepath,
    bool compressfile,
    const base::android::JavaParamRef<jobject>& callback) {
  base::FilePath file_path(
      base::android::ConvertJavaStringToUTF8(env, jfilepath));
  ScopedJavaGlobalRef<jobject> global_callback(env, callback);
  auto endpoint = TracingController::CreateFileEndpoint(
      file_path,
      base::BindRepeating(&TracingControllerAndroid::OnTracingStopped,
                          weak_factory_.GetWeakPtr(), global_callback));
  if (compressfile) {
    endpoint =
        TracingControllerImpl::CreateCompressedStringEndpoint(endpoint, true);
  }
  if (!TracingController::GetInstance()->StopTracing(endpoint)) {
    LOG(ERROR) << "EndTracingAsync failed, forcing an immediate stop";
    OnTracingStopped(global_callback);
  }
}

void TracingControllerAndroid::GenerateTracingFilePath(
    base::FilePath* file_path) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jfilename =
      Java_TracingControllerAndroidImpl_generateTracingFilePath(env);
  *file_path = base::FilePath(
      base::android::ConvertJavaStringToUTF8(env, jfilename.obj()));
}

void TracingControllerAndroid::OnTracingStopped(
    const base::android::ScopedJavaGlobalRef<jobject>& callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_object_.get(env);
  if (obj.obj())
    Java_TracingControllerAndroidImpl_onTracingStopped(env, obj, callback);
}

bool TracingControllerAndroid::GetKnownCategoriesAsync(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& callback) {
  ScopedJavaGlobalRef<jobject> global_callback(env, callback);
  return TracingController::GetInstance()->GetCategories(
      base::BindOnce(&TracingControllerAndroid::OnKnownCategoriesReceived,
                     weak_factory_.GetWeakPtr(), global_callback));
}

void TracingControllerAndroid::OnKnownCategoriesReceived(
    const ScopedJavaGlobalRef<jobject>& callback,
    const std::set<std::string>& categories_received) {
  base::ListValue category_list;
  for (const std::string& category : categories_received)
    category_list.AppendString(category);
  std::string received_category_list;
  base::JSONWriter::Write(category_list, &received_category_list);

  // This log is required by adb_profile_chrome.py.
  // TODO(crbug.com/898816): Replace (users of) this with DevTools' Tracing API.
  LOG(WARNING) << "{\"traceCategoriesList\": " << received_category_list << "}";

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_object_.get(env);
  if (obj.obj()) {
    std::vector<std::string> category_vector(categories_received.begin(),
                                             categories_received.end());
    base::android::ScopedJavaLocalRef<jobjectArray> jcategories =
        base::android::ToJavaArrayOfStrings(env, category_vector);
    Java_TracingControllerAndroidImpl_onKnownCategoriesReceived(
        env, obj, jcategories, callback);
  }
}

static ScopedJavaLocalRef<jstring>
JNI_TracingControllerAndroidImpl_GetDefaultCategories(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  base::trace_event::TraceConfig trace_config;
  return base::android::ConvertUTF8ToJavaString(
      env, trace_config.ToCategoryFilterString());
}

bool TracingControllerAndroid::GetTraceBufferUsageAsync(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& callback) {
  ScopedJavaGlobalRef<jobject> global_callback(env, callback);
  return TracingController::GetInstance()->GetTraceBufferUsage(
      base::BindOnce(&TracingControllerAndroid::OnTraceBufferUsageReceived,
                     weak_factory_.GetWeakPtr(), global_callback));
}

void TracingControllerAndroid::OnTraceBufferUsageReceived(
    const ScopedJavaGlobalRef<jobject>& callback,
    float percent_full,
    size_t approximate_event_count) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_object_.get(env);
  if (obj.obj()) {
    Java_TracingControllerAndroidImpl_onTraceBufferUsageReceived(
        env, obj, percent_full, approximate_event_count, callback);
  }
}

}  // namespace content
