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
using base::android::JavaParamRef;

namespace content {

PageProxy::PageProxy(PageImpl* cpp_page) {
  JNIEnv* env = AttachCurrentThread();
  java_page_ = Java_Page_Constructor(
      env, cpp_page->GetMainDocument().lifecycle_state() ==
               RenderFrameHostImpl::LifecycleStateImpl::kPrerendering);
}

PageProxy::~PageProxy() = default;

void PageProxy::WillDeletePage(bool is_prerendering) {
  JNIEnv* env = AttachCurrentThread();
  Java_Page_willDeletePage(env, java_page_, is_prerendering);
}

}  // namespace content
