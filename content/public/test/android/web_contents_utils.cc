// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/android/content_test_jni/WebContentsUtils_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

// Reports all frame submissions to the browser process, even those that do not
// impact Browser UI.
void JNI_WebContentsUtils_ReportAllFrameSubmissions(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jboolean enabled) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  RenderFrameMetadataProviderImpl* provider =
      RenderWidgetHostImpl::From(web_contents->GetRenderViewHost()->GetWidget())
          ->render_frame_metadata_provider();
  provider->ReportAllFrameSubmissionsForTesting(enabled);
}

ScopedJavaLocalRef<jobject> JNI_WebContentsUtils_GetFocusedFrame(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  return static_cast<RenderFrameHostImpl*>(web_contents->GetFocusedFrame())
      ->GetJavaRenderFrameHost();
}

void JNI_WebContentsUtils_EvaluateJavaScriptWithUserGesture(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& script) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  RenderViewHost* rvh = web_contents->GetRenderViewHost();
  DCHECK(rvh);

  if (!rvh->IsRenderViewLive()) {
    if (!static_cast<WebContentsImpl*>(web_contents)
             ->CreateRenderViewForInitialEmptyDocument()) {
      LOG(ERROR)
          << "Failed to create RenderView in EvaluateJavaScriptWithUserGesture";
      return;
    }
  }

  web_contents->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      ConvertJavaStringToUTF16(env, script));
}

}  // namespace content
