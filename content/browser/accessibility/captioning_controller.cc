// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/captioning_controller.h"

#include "base/android/jni_string.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/android/content_jni_headers/CaptioningController_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

int GetRenderProcessIdFromRenderViewHost(RenderViewHost* host) {
  DCHECK(host);
  RenderProcessHost* render_process = host->GetProcess();
  DCHECK(render_process);
  if (render_process->IsInitializedAndNotDead())
    return render_process->GetProcess().Handle();
  return 0;
}

// Adds !important to all captions styles. They should always override any
// styles added by the video author or by a user stylesheet. This is because in
// Chrome, there is an option to turn off captions styles, so any time the
// captions are on, the styles should take priority.
std::string AddCSSImportant(std::string css_string) {
  return css_string + " !important";
}

}  // namespace

CaptioningController::CaptioningController(JNIEnv* env,
                                           const JavaRef<jobject>& obj,
                                           WebContents* web_contents)
    : WebContentsObserver(web_contents), java_ref_(env, obj) {}

CaptioningController::~CaptioningController() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_CaptioningController_onDestroy(env, obj);
}

void CaptioningController::RenderViewReady() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_CaptioningController_onRenderProcessChange(env, obj);
}

void CaptioningController::RenderViewHostChanged(RenderViewHost* old_host,
                                                 RenderViewHost* new_host) {
  int old_pid = 0;
  if (old_host) {
    old_pid = GetRenderProcessIdFromRenderViewHost(old_host);
  }
  int new_pid =
      GetRenderProcessIdFromRenderViewHost(web_contents()->GetRenderViewHost());
  if (new_pid != old_pid) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (!obj.is_null())
      Java_CaptioningController_onRenderProcessChange(env, obj);
  }
}

void CaptioningController::WebContentsDestroyed() {
  delete this;
}

void CaptioningController::SetTextTrackSettings(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean textTracksEnabled,
    const JavaParamRef<jstring>& textTrackBackgroundColor,
    const JavaParamRef<jstring>& textTrackFontFamily,
    const JavaParamRef<jstring>& textTrackFontStyle,
    const JavaParamRef<jstring>& textTrackFontVariant,
    const JavaParamRef<jstring>& textTrackTextColor,
    const JavaParamRef<jstring>& textTrackTextShadow,
    const JavaParamRef<jstring>& textTrackTextSize) {
  FrameMsg_TextTrackSettings_Params params;
  params.text_tracks_enabled = textTracksEnabled;
  params.text_track_background_color =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackBackgroundColor));
  params.text_track_font_family =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackFontFamily));
  params.text_track_font_style =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackFontStyle));
  params.text_track_font_variant =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackFontVariant));
  params.text_track_text_color =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackTextColor));
  params.text_track_text_shadow =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackTextShadow));
  params.text_track_text_size =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackTextSize));
  static_cast<WebContentsImpl*>(web_contents())
      ->GetMainFrame()
      ->SetTextTrackSettings(params);
}

jlong JNI_CaptioningController_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromJavaWebContents(jweb_contents));
  CHECK(web_contents);
  return reinterpret_cast<intptr_t>(
      new CaptioningController(env, obj, web_contents));
}

}  // namespace content
