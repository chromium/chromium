// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/jni_reflect.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

#include "content/browser/reflection_jni_headers/AccessibleObject_jni.h"
#include "content/browser/reflection_jni_headers/Class_jni.h"
#include "content/browser/reflection_jni_headers/Method_jni.h"
#include "content/browser/reflection_jni_headers/Modifier_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

std::string GetClassName(JNIEnv* env, const JavaRef<jclass>& clazz) {
  return ConvertJavaStringToUTF8(JNI_Class::Java_Class_getName(env, clazz));
}

ScopedJavaLocalRef<jobjectArray> GetClassMethods(JNIEnv* env,
                                                 const JavaRef<jclass>& clazz) {
  return JNI_Class::Java_Class_getMethods(env, clazz);
}

std::string GetMethodName(JNIEnv* env, const JavaRef<jobject>& method) {
  return ConvertJavaStringToUTF8(JNI_Method::Java_Method_getName(env, method));
}

ScopedJavaLocalRef<jobjectArray> GetMethodParameterTypes(
    JNIEnv* env,
    const JavaRef<jobject>& method) {
  return JNI_Method::Java_Method_getParameterTypes(env, method);
}

ScopedJavaLocalRef<jclass> GetMethodReturnType(JNIEnv* env,
                                               const JavaRef<jobject>& method) {
  return JNI_Method::Java_Method_getReturnType(env, method);
}

ScopedJavaLocalRef<jclass> GetMethodDeclaringClass(
    JNIEnv* env,
    const JavaRef<jobject>& method) {
  return JNI_Method::Java_Method_getDeclaringClass(env, method);
}

bool IsMethodStatic(JNIEnv* env, const JavaRef<jobject>& method) {
  jint modifiers = JNI_Method::Java_Method_getModifiers(env, method);
  return JNI_Modifier::Java_Modifier_isStatic(env, modifiers);
}

bool IsAnnotationPresent(JNIEnv* env,
                         const JavaRef<jobject>& obj,
                         const JavaRef<jclass>& annotation_clazz) {
  return JNI_AccessibleObject::Java_AccessibleObject_isAnnotationPresent(
      env, obj, annotation_clazz);
}

}  // namespace content
