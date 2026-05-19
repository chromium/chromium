// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/android/content_test_jni/NavigationControllerUtil_jni.h"
#include "content/public/test/android/content_test_jni/NavigationEntrySimple_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToTypedJavaArrayOfObjects;

namespace content {

ScopedJavaLocalRef<jobjectArray>
JNI_NavigationControllerUtil_GetNavigationHistorySimple(
    JNIEnv* env,
    const JavaRef<jobject>& j_web_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents) {
    return nullptr;
  }

  content::NavigationController& controller = web_contents->GetController();

  std::vector<ScopedJavaLocalRef<jobject>> j_objs;
  int entry_count = controller.GetEntryCount();
  for (int i = 0; i < entry_count; ++i) {
    content::NavigationEntry* entry = controller.GetEntryAtIndex(i);
    if (entry) {
      j_objs.push_back(Java_NavigationEntrySimple_Constructor(
          env, ConvertUTF8ToJavaString(env, entry->GetURL().spec()),
          ConvertUTF8ToJavaString(env, entry->GetExtraHeaders())));
    }
  }

  // 4. Convert the vector of local references into a single typed Java array
  return ToTypedJavaArrayOfObjects(
      env, j_objs,
      org_chromium_content_1public_browser_test_util_NavigationEntrySimple_clazz(
          env));
}

}  // namespace content

DEFINE_JNI(NavigationControllerUtil)
