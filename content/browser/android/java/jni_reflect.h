// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_JNI_REFLECT_H_
#define CONTENT_BROWSER_ANDROID_JAVA_JNI_REFLECT_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"

namespace content {

// Return the class's name.
std::string GetClassName(JNIEnv* env,
                         const base::android::JavaRef<jclass>& clazz);

// Return an array of Method objects for the public methods of clazz.
base::android::ScopedJavaLocalRef<jobjectArray> GetClassMethods(
    JNIEnv* env,
    const base::android::JavaRef<jclass>& clazz);

// Return the method's name.
std::string GetMethodName(JNIEnv* env,
                          const base::android::JavaRef<jobject>& method);

// Return an array listing the method's parameter types.
base::android::ScopedJavaLocalRef<jobjectArray> GetMethodParameterTypes(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& method);

// Return the method's return type.
base::android::ScopedJavaLocalRef<jclass> GetMethodReturnType(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& method);

// Return the method's declaring class.
base::android::ScopedJavaLocalRef<jclass> GetMethodDeclaringClass(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& method);

// Check if the method is static.
bool IsMethodStatic(JNIEnv* env, const base::android::JavaRef<jobject>& method);

// Check if the annotation identified by annotation_clazz is present on the
// object obj.
bool IsAnnotationPresent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jclass>& annotation_clazz);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_JNI_REFLECT_H_
