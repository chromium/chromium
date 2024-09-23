// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/browser_context_handle.h"

#include "base/android/jni_android.h"
#include "content/public/browser/browser_context.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/BrowserContextHandleImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;

namespace content {

// static
BrowserContext* BrowserContextFromJavaHandle(const JavaRef<jobject>& jhandle) {
  if (!jhandle)
    return nullptr;

  return reinterpret_cast<BrowserContext*>(
      Java_BrowserContextHandleImpl_getNativeBrowserContextPointer(
          AttachCurrentThread(), jhandle));
}

}  // namespace content
