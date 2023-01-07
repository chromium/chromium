// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_bound_object_delegate.h"

namespace content {

GinJavaBoundObjectDelegate::GinJavaBoundObjectDelegate(
    scoped_refptr<GinJavaBoundObject> object)
    : object_(object) {
}

GinJavaBoundObjectDelegate::~GinJavaBoundObjectDelegate() {
}

base::android::ScopedJavaLocalRef<jobject>
GinJavaBoundObjectDelegate::GetLocalRef(JNIEnv* env) {
  return object_->GetLocalRef(env);
}

base::android::ScopedJavaLocalRef<jclass>
GinJavaBoundObjectDelegate::GetLocalClassRef(JNIEnv* env) {
  return object_->GetLocalClassRef(env);
}

const JavaMethod* GinJavaBoundObjectDelegate::FindMethod(
    const std::string& method_name,
    size_t num_parameters) {
  return object_->FindMethod(method_name, num_parameters);
}

bool GinJavaBoundObjectDelegate::IsObjectGetClassMethod(
    const JavaMethod* method) {
  return object_->IsObjectGetClassMethod(method);
}

const base::android::JavaRef<jclass>&
GinJavaBoundObjectDelegate::GetSafeAnnotationClass() {
  return object_->GetSafeAnnotationClass();
}

}  // namespace content
