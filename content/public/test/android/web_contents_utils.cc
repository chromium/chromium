// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/test/android/content_test_jni/WebContentsUtils_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {
void JavaScriptResultCallback(const ScopedJavaGlobalRef<jobject>& callback,
                              base::Value result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json;
  base::JSONWriter::Write(result, &json);
  ScopedJavaLocalRef<jstring> j_json = ConvertUTF8ToJavaString(env, json);
  Java_WebContentsUtils_onEvaluateJavaScriptResult(env, j_json, callback);
}
}  // namespace

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
    const JavaParamRef<jstring>& script,
    const base::android::JavaParamRef<jobject>& callback) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromJavaWebContents(jweb_contents));
  RenderViewHost* rvh = web_contents->GetRenderViewHost();
  DCHECK(rvh);

  if (!web_contents->GetWebContentsAndroid()
           ->InitializeRenderFrameForJavaScript())
    return;

  if (!callback) {
    // No callback requested.
    web_contents->GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(
            ConvertJavaStringToUTF16(env, script), base::NullCallback(),
            ISOLATED_WORLD_ID_GLOBAL);
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::OnceCallback below.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);

  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      ConvertJavaStringToUTF16(env, script),
      base::BindOnce(&JavaScriptResultCallback, std::move(j_callback)),
      ISOLATED_WORLD_ID_GLOBAL);
}

void JNI_WebContentsUtils_CrashTab(JNIEnv* env,
                                   const JavaParamRef<jobject>& jweb_contents) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromJavaWebContents(jweb_contents));
  web_contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      RESULT_CODE_KILLED);
}

void JNI_WebContentsUtils_NotifyCopyableViewInWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& done_callback) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromJavaWebContents(jweb_contents));

  NotifyCopyableViewInWebContents(
      web_contents, base::BindOnce(
                        [](const ScopedJavaGlobalRef<jobject>& inner_callback) {
                          base::android::RunRunnableAndroid(inner_callback);
                        },
                        ScopedJavaGlobalRef<jobject>(done_callback)));
}

}  // namespace content
