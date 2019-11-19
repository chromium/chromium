// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/android_overlay_provider_impl.h"

#include "content/public/android/content_jni_headers/AndroidOverlayProviderImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace content {

// static
AndroidOverlayProvider* AndroidOverlayProvider::GetInstance() {
  static AndroidOverlayProvider* instance = nullptr;
  if (!instance)
    instance = new AndroidOverlayProviderImpl();

  return instance;
}

AndroidOverlayProviderImpl::AndroidOverlayProviderImpl() {}

AndroidOverlayProviderImpl::~AndroidOverlayProviderImpl() {}

bool AndroidOverlayProviderImpl::AreOverlaysSupported() {
  JNIEnv* env = AttachCurrentThread();

  return Java_AndroidOverlayProviderImpl_areOverlaysSupported(env);
}

}  // namespace content
