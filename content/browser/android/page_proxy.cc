// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/page_proxy.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/Page_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

PageProxy::PageProxy(PageImpl* cpp_page) {
  JNIEnv* env = AttachCurrentThread();
  java_page_ = JavaObjectWeakGlobalRef(
      env, Java_Page_Constructor(
               env, reinterpret_cast<intptr_t>(this),
               cpp_page->GetMainDocument().lifecycle_state() ==
                   RenderFrameHostImpl::LifecycleStateImpl::kPrerendering));
}

PageProxy::~PageProxy() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_page = java_page_.get(env);
  CHECK(!java_page.is_null());
  Java_Page_destroy(env, java_page);
}

void PageProxy::WillDeletePage(bool is_prerendering) {
  JNIEnv* env = AttachCurrentThread();
  Java_Page_willDeletePage(env, GetJavaPage(), is_prerendering);
}

base::android::ScopedJavaLocalRef<jobject> PageProxy::GetJavaPage() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_page = java_page_.get(env);
  CHECK(!java_page.is_null());
  return java_page;
}

}  // namespace content

DEFINE_JNI(Page)
