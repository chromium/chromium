// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_jni_onload.h"

#include "base/android/base_jni_onload.h"
#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "content/app/android/library_loader_hooks.h"

namespace content {
namespace android {

bool OnJNIOnLoadInit() {
  if (!base::android::OnJNIOnLoadInit())
    return false;

  base::android::SetLibraryLoadedHook(&content::LibraryLoaded);
  return true;
}

}  // namespace android
}  // namespace content
