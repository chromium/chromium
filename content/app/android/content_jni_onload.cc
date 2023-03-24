// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_jni_onload.h"

#include <vector>

#include "base/android/base_jni_onload.h"
#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/i18n/icu_util.h"
#include "base/trace_event/trace_event.h"
#include "content/app/android/library_loader_hooks.h"

namespace content {
namespace android {

bool OnJNIOnLoadInit() {
  if (!base::android::OnJNIOnLoadInit())
    return false;

  base::android::SetLibraryLoadedHook(&content::LibraryLoaded);

#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
  // Initialize ICU early so that it can be used by JNI calls before
  // ContentMain() is called.
  TRACE_EVENT0("startup", "InitializeICU");
  CHECK(base::i18n::InitializeICU());
#endif
  return true;
}

}  // namespace android
}  // namespace content
