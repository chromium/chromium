// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BODY_ANDROID_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BODY_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace network {
class ResourceRequestBody;
}

namespace content {

// Returns an instance of org.chromium.content_public.common.ResourceRequestBody
// that contains serialized representation of the |native_object|.
CONTENT_EXPORT base::android::ScopedJavaLocalRef<jobject>
ConvertResourceRequestBodyToJavaObject(
    JNIEnv* env,
    const scoped_refptr<network::ResourceRequestBody>& native_object);

// Reconstructs the native C++ network::ResourceRequestBody object based on
// org.chromium.content_public.common.ResourceRequestBody (|java_object|) passed
// in as an argument.
CONTENT_EXPORT scoped_refptr<network::ResourceRequestBody>
ExtractResourceRequestBodyFromJavaObject(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_object);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BODY_ANDROID_H_
