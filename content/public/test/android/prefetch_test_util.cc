// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "url/android/gurl_android.h"

#include "content/public/test/android/content_test_jni/PrefetchTestUtil_jni.h"

namespace content {

void JNI_PrefetchTestUtil_WaitUntilPrefetchResponseCompleted(
    JNIEnv* env,
    GURL& url,
    const base::android::JavaParamRef<jobject>& callback) {
  PrefetchService::SetPrefetchResponseCompletedCallbackForTesting(
      base::BindRepeating(
          [](const GURL& url,
             const base::android::ScopedJavaGlobalRef<jobject>& callback,
             base::WeakPtr<PrefetchContainer> container) {
            PrefetchContainer::Key key{std::nullopt, url};
            if (key == container->key()) {
              base::android::RunRunnableAndroid(callback);
            }
          },
          url,
          base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

}  // namespace content
