// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_onload.h"
#include "base/android/jni_android.h"
#include "mojo/core/embedder/embedder.h"

JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);

  if (!base::android::OnJNIOnLoadInit())
    return -1;

  mojo::core::Init();
  return JNI_VERSION_1_4;
}
