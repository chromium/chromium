// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/text_suggestion_host_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "content/browser/android/text_suggestion_host_mojo_impl_android.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/gfx/android/view_configuration.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/SuggestionInfo_jni.h"
#include "content/public/android/content_jni_headers/TextSuggestionHost_jni.h"
#include "content/public/android/content_jni_headers/TextSuggestionPopupController_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace content {

namespace {

const size_t kMaxNumberOfSuggestions = 5;

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(TextSuggestionHostAndroid);

TextSuggestionHostAndroid::TextSuggestionHostAndroid(RenderFrameHost* rfh)
    : DocumentUserData<TextSuggestionHostAndroid>(rfh),
      suggestion_menu_timeout_(
          base::BindRepeating(
              &TextSuggestionHostAndroid::OnSuggestionMenuTimeout,
              base::Unretained(this)),
          GetUIThreadTaskRunner({BrowserTaskType::kUserInput})) {}

TextSuggestionHostAndroid::~TextSuggestionHostAndroid() {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (web_contents) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_TextSuggestionPopupController_onNativeTextSuggestionHostDestroyed(
        env, web_contents->GetJavaWebContents(),
        reinterpret_cast<intptr_t>(this));
  }
}

void TextSuggestionHostAndroid::ApplySpellCheckSuggestion(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& replacement) {
  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
      text_suggestion_backend = GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->ApplySpellCheckSuggestion(
      ConvertJavaStringToUTF8(env, replacement));
}

void TextSuggestionHostAndroid::ApplyTextSuggestion(
    JNIEnv*,
    int marker_tag,
    int suggestion_index) {
  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
      text_suggestion_backend = GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->ApplyTextSuggestion(marker_tag, suggestion_index);
}

void TextSuggestionHostAndroid::DeleteActiveSuggestionRange(JNIEnv*) {
  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
      text_suggestion_backend = GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->DeleteActiveSuggestionRange();
}

void TextSuggestionHostAndroid::OnNewWordAddedToDictionary(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& word) {
  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
      text_suggestion_backend = GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->OnNewWordAddedToDictionary(
      ConvertJavaStringToUTF8(env, word));
}

void TextSuggestionHostAndroid::OnSuggestionMenuClosed(JNIEnv*) {
  const mojo::Remote<blink::mojom::TextSuggestionBackend>&
      text_suggestion_backend = GetTextSuggestionBackend();
  if (!text_suggestion_backend)
    return;
  text_suggestion_backend->OnSuggestionMenuClosed();
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

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  Java_TextSuggestionPopupController_showSpellCheckSuggestionMenu(
      env, web_contents->GetJavaWebContents(), reinterpret_cast<intptr_t>(this),
      caret_x, caret_y, ConvertUTF8ToJavaString(env, marked_text),
      ToJavaArrayOfStrings(env, suggestion_strings));
}

void TextSuggestionHostAndroid::ShowTextSuggestionMenu(
    double caret_x,
    double caret_y,
    const std::string& marked_text,
    const std::vector<blink::mojom::TextSuggestionPtr>& suggestions) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();

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

  Java_TextSuggestionPopupController_showTextSuggestionMenu(
      env, web_contents->GetJavaWebContents(), reinterpret_cast<intptr_t>(this),
      caret_x, caret_y, ConvertUTF8ToJavaString(env, marked_text),
      jsuggestion_infos);
}

void TextSuggestionHostAndroid::StartSuggestionMenuTimer() {
  suggestion_menu_timeout_.Stop();
  suggestion_menu_timeout_.Start(
      base::Milliseconds(gfx::ViewConfiguration::GetDoubleTapTimeoutInMs()));
}

void TextSuggestionHostAndroid::HidePopups() {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TextSuggestionPopupController_hidePopups(
      env, web_contents->GetJavaWebContents());
}

void TextSuggestionHostAndroid::StopSuggestionMenuTimer() {
  suggestion_menu_timeout_.Stop();
}

void TextSuggestionHostAndroid::BindTextSuggestionHost(
    mojo::PendingReceiver<blink::mojom::TextSuggestionHost> receiver) {
  text_suggestion_impl_ =
      TextSuggestionHostMojoImplAndroid::Create(this, std::move(receiver));
}

const mojo::Remote<blink::mojom::TextSuggestionBackend>&
TextSuggestionHostAndroid::GetTextSuggestionBackend() {
  if (!text_suggestion_backend_) {
    render_frame_host().GetRemoteInterfaces()->GetInterface(
        text_suggestion_backend_.BindNewPipeAndPassReceiver());
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

DEFINE_JNI(SuggestionInfo)
DEFINE_JNI(TextSuggestionHost)
DEFINE_JNI(TextSuggestionPopupController)
