// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_BROWSER_CONTEXT_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_BROWSER_CONTEXT_HANDLE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "content/common/content_export.h"

namespace content {

class BrowserContext;

// Returns a pointer to the native BrowserContext wrapped by the given Java
// BrowserContextHandle reference.
CONTENT_EXPORT content::BrowserContext* BrowserContextFromJavaHandle(
    const base::android::JavaRef<jobject>& jhandle);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_BROWSER_CONTEXT_HANDLE_H_
