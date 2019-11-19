// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/text_suggestion_host_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "content/browser/android/text_suggestion_host_mojo_impl_android.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/android/content_jni_headers/SuggestionInfo_jni.h"
#include "content/public/android/content_jni_headers/TextSuggestionHost_jni.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/gfx/android/view_configuration.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::JavaParamRef;
using base::android::MethodID;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace content {

namespace {

const size_t kMaxNumberOfSuggestions = 5;

}  // namespace

void TextSuggestionHostAndroid::Create(JNIEnv* env, WebContents* web_contents) {
  auto* text_suggestion_host = new TextSuggestionHostAndroid(env, web_contents);
  text_suggestion_host->Initialize();
}

TextSuggestionHostAndroid::TextSuggestionHostAndroid(JNIEnv* env,
                                                     WebContents* web_contents)
    : RenderWidgetHostConnector(web_contents),
      WebContentsObserver(web_contents),
      rwhva_(nullptr),
      suggestion_menu_timeout_(base::BindRepeating(
          &TextSuggestionHostAndroid::OnSuggestionMenuTimeout,
          base::Unretained(this))) {}

TextSuggestionHostAndroid::~TextSuggestionHostAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_text_suggestion_host_.get(env);
  if (!obj.is_null())
    Java_TextSuggestionHost_onNativeDestroyed(env, obj);
}

void TextSuggestionHostAndroid::UpdateRenderProcessConnection(
    RenderWidgetHostViewAndroid* old_rwhva,
    RenderWidgetHostViewAndroid* new_rwhva) {
  text_suggestion_backend_.reset();
  if (old_rwhva)
    old_rwhva->set_text_suggestion_host(nullptr);
  if (new_rwhva)
    new_rwhva->set_text_suggestion_host(this);
  rwhva_ = new_rwhva;
}

void TextSuggestionHostAndroid::ApplySpellCheckSuggestion(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    const base::android::JavaParamRef<jstring>& replacement) {
  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
      text_suggestion_backend = GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->ApplySpellCheckSuggestion(
      ConvertJavaStringToUTF8(env, replacement));
}

void TextSuggestionHostAndroid::ApplyTextSuggestion(
    JNIEnv*,
    const JavaParamRef<jobject>&,
    int marker_tag,
    int suggestion_index) {
  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
      text_suggestion_backend = GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->ApplyTextSuggestion(marker_tag, suggestion_index);
}

void TextSuggestionHostAndroid::DeleteActiveSuggestionRange(
    JNIEnv*,
    const JavaParamRef<jobject>&) {
  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
      text_suggestion_backend = GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->DeleteActiveSuggestionRange();
}

void TextSuggestionHostAndroid::OnNewWordAddedToDictionary(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    const base::android::JavaParamRef<jstring>& word) {
  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
      text_suggestion_backend = GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->OnNewWordAddedToDictionary(
      ConvertJavaStringToUTF8(env, word));
}

void TextSuggestionHostAndroid::OnSuggestionMenuClosed(
    JNIEnv*,
    const JavaParamRef<jobject>&) {
  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
      text_suggestion_backend = GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->OnSuggestionMenuClosed();
}

double TextSuggestionHostAndroid::DpToPxIfNeeded(double value) {
  WebContents* contents = RenderWidgetHostConnector::web_contents();
  // When --use-zoom-for-dsf is disabled, caret values are CSS scale
  // (i.e., not pixel scale). This code changes it to pixel scale.
  if (!IsUseZoomForDSFEnabled() && contents && contents->GetNativeView()) {
    return contents->GetNativeView()->GetDipScale() * value;
  }
  return value;
}

ScopedJavaLocalRef<jobject>
TextSuggestionHostAndroid::GetJavaTextSuggestionHost() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_text_suggestion_host_.get(env);
  if (obj.is_null()) {
    obj = Java_TextSuggestionHost_create(
        env, WebContentsObserver::web_contents()->GetJavaWebContents(),
        reinterpret_cast<intptr_t>(this));
    java_text_suggestion_host_ = JavaObjectWeakGlobalRef(env, obj);
  }
  return obj;
}

void TextSuggestionHostAndroid::ShowSpellCheckSuggestionMenu(
    double caret_x,
    double caret_y,
    const std::string& marked_text,
    const std::vector<blink::mojom::SpellCheckSuggestionPtr>& suggestions) {
  std::vector<std::string> suggestion_strings;
  // Enforce kMaxNumberOfSuggestions here in case the renderer is hijacked and
  // tries to send bad input.
  for (size_t i = 0; i < suggestions.size() && i < kMaxNumberOfSuggestions; ++i)
    suggestion_strings.push_back(suggestions[i]->suggestion);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaTextSuggestionHost();
  if (obj.is_null())
    return;

  caret_x = DpToPxIfNeeded(caret_x);
  caret_y = DpToPxIfNeeded(caret_y);

  Java_TextSuggestionHost_showSpellCheckSuggestionMenu(
      env, obj, caret_x, caret_y, ConvertUTF8ToJavaString(env, marked_text),
      ToJavaArrayOfStrings(env, suggestion_strings));
}

void TextSuggestionHostAndroid::ShowTextSuggestionMenu(
    double caret_x,
    double caret_y,
    const std::string& marked_text,
    const std::vector<blink::mojom::TextSuggestionPtr>& suggestions) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaTextSuggestionHost();
  if (obj.is_null())
    return;

  // Enforce kMaxNumberOfSuggestions here in case the renderer is hijacked and
  // tries to send bad input.
  size_t suggestion_count =
      std::min(suggestions.size(), kMaxNumberOfSuggestions);
  ScopedJavaLocalRef<jobjectArray> jsuggestion_infos =
      Java_SuggestionInfo_createArray(env, suggestion_count);

  for (size_t i = 0; i < suggestion_count; ++i) {
    const blink::mojom::TextSuggestionPtr& suggestion_ptr = suggestions[i];
    Java_SuggestionInfo_createSuggestionInfoAndPutInArray(
        env, jsuggestion_infos, i, suggestion_ptr->marker_tag,
        suggestion_ptr->suggestion_index,
        ConvertUTF8ToJavaString(env, suggestion_ptr->prefix),
        ConvertUTF8ToJavaString(env, suggestion_ptr->suggestion),
        ConvertUTF8ToJavaString(env, suggestion_ptr->suffix));
  }

  caret_x = DpToPxIfNeeded(caret_x);
  caret_y = DpToPxIfNeeded(caret_y);

  Java_TextSuggestionHost_showTextSuggestionMenu(
      env, obj, caret_x, caret_y, ConvertUTF8ToJavaString(env, marked_text),
      jsuggestion_infos);
}

void TextSuggestionHostAndroid::StartSuggestionMenuTimer() {
  suggestion_menu_timeout_.Stop();
  suggestion_menu_timeout_.Start(base::TimeDelta::FromMilliseconds(
      gfx::ViewConfiguration::GetDoubleTapTimeoutInMs()));
}

void TextSuggestionHostAndroid::OnKeyEvent() {
  suggestion_menu_timeout_.Stop();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_text_suggestion_host_.get(env);
  if (obj.is_null())
    return;

  Java_TextSuggestionHost_hidePopups(env, obj);
}

void TextSuggestionHostAndroid::StopSuggestionMenuTimer() {
  suggestion_menu_timeout_.Stop();
}

RenderFrameHost* TextSuggestionHostAndroid::GetFocusedFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We get the focused frame from the WebContents of the page. Although
  // |rwhva_->GetFocusedWidget()| does a similar thing, there is no direct way
  // to get a RenderFrameHost from its RWH.
  if (!rwhva_)
    return nullptr;
  RenderWidgetHostImpl* rwh =
      RenderWidgetHostImpl::From(rwhva_->GetRenderWidgetHost());
  if (!rwh || !rwh->delegate())
    return nullptr;

  if (auto* contents = rwh->delegate()->GetAsWebContents())
    return contents->GetFocusedFrame();

  return nullptr;
}

const mojo::Remote<blink::mojom::TextSuggestionBackend>&
TextSuggestionHostAndroid::GetTextSuggestionBackend() {
  if (!text_suggestion_backend_) {
    if (RenderFrameHost* rfh = GetFocusedFrame()) {
      rfh->GetRemoteInterfaces()->GetInterface(
          text_suggestion_backend_.BindNewPipeAndPassReceiver());
    }
  }
  return text_suggestion_backend_;
}

void TextSuggestionHostAndroid::OnSuggestionMenuTimeout() {
  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
      text_suggestion_backend = GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->SuggestionMenuTimeoutCallback(
      kMaxNumberOfSuggestions);
}

}  // namespace content
