// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_
#define CONTENT_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_

#include <jni.h>

#include "base/android/library_loader/library_loader_hooks.h"

namespace content {

// Do the intialization of content needed immediately after the native library
// has loaded.
// This is designed to be used as a hook function to be passed to
// base::android::SetLibraryLoadedHook
bool LibraryLoaded(JNIEnv* env,
                   jclass clazz,
                   base::android::LibraryProcessType library_process_type);

}  // namespace content

#endif  // CONTENT_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_
