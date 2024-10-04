// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/captioning_controller.h"

#include "base/android/jni_string.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/CaptioningController_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

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
  if (!obj.is_null()) {
    Java_CaptioningController_onDestroy(env, obj);
  }
}

void CaptioningController::PrimaryPageChanged(Page& page) {
  CaptioningController::RenderViewReady();
}

void CaptioningController::RenderViewReady() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
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
  auto web_prefs = web_contents()->GetOrCreateWebPreferences();
  web_prefs.text_tracks_enabled = textTracksEnabled;
  web_prefs.text_track_background_color =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackBackgroundColor));
  web_prefs.text_track_font_family =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackFontFamily));
  web_prefs.text_track_font_style =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackFontStyle));
  web_prefs.text_track_font_variant =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackFontVariant));
  web_prefs.text_track_text_color =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackTextColor));
  web_prefs.text_track_text_shadow =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackTextShadow));
  web_prefs.text_track_text_size =
      AddCSSImportant(ConvertJavaStringToUTF8(env, textTrackTextSize));
  web_contents()->SetWebPreferences(web_prefs);
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
