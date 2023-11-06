// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_SCRIPT_TO_JAVA_TYPES_COERCION_H_
#define CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_SCRIPT_TO_JAVA_TYPES_COERCION_H_

#include <map>

#include "base/android/jni_weak_ref.h"
#include "base/values.h"
#include "content/browser/android/java/gin_java_bound_object.h"
#include "content/browser/android/java/java_type.h"
#include "content/common/android/gin_java_bridge_errors.h"

namespace content {

typedef std::map<GinJavaBoundObject::ObjectID, JavaObjectWeakGlobalRef>
    ObjectRefs;

jvalue CoerceJavaScriptValueToJavaValue(JNIEnv* env,
                                        const base::Value& value,
                                        const JavaType& target_type,
                                        bool coerce_to_string,
                                        const ObjectRefs& object_refs,
                                        mojom::GinJavaBridgeError* error);

void ReleaseJavaValueIfRequired(JNIEnv* env,
                                jvalue* value,
                                const JavaType& type);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_SCRIPT_TO_JAVA_TYPES_COERCION_H_
