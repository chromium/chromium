// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/selection/smart_selection_client.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/supports_user_data.h"
#include "content/public/android/content_jni_headers/SmartSelectionClient_jni.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {
const void* const kSmartSelectionClientUDKey = &kSmartSelectionClientUDKey;

// This class deletes SmartSelectionClient when WebContents is destroyed.
class UserData : public base::SupportsUserData::Data {
 public:
  explicit UserData(SmartSelectionClient* client) : client_(client) {}

 private:
  std::unique_ptr<SmartSelectionClient> client_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(UserData);
};
}

jlong JNI_SmartSelectionClient_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents)
      << "A SmartSelectionClient should be created with a valid WebContents.";

  return reinterpret_cast<intptr_t>(
      new SmartSelectionClient(env, obj, web_contents));
}

SmartSelectionClient::SmartSelectionClient(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    WebContents* web_contents)
    : java_ref_(env, obj), web_contents_(web_contents) {
  DCHECK(!web_contents_->GetUserData(kSmartSelectionClientUDKey));
  web_contents_->SetUserData(kSmartSelectionClientUDKey,
                             std::make_unique<UserData>(this));
}

SmartSelectionClient::~SmartSelectionClient() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (!j_obj.is_null()) {
    Java_SmartSelectionClient_onNativeSideDestroyed(
        env, j_obj, reinterpret_cast<intptr_t>(this));
  }
}

void SmartSelectionClient::RequestSurroundingText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    int num_extra_characters,
    int callback_data) {
  RenderFrameHost* focused_frame = web_contents_->GetFocusedFrame();
  if (!focused_frame) {
    OnSurroundingTextReceived(callback_data, base::string16(), 0, 0);
    return;
  }

  focused_frame->RequestTextSurroundingSelection(
      base::BindOnce(&SmartSelectionClient::OnSurroundingTextReceived,
                     weak_ptr_factory_.GetWeakPtr(), callback_data),
      num_extra_characters);
}

void SmartSelectionClient::CancelAllRequests(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SmartSelectionClient::OnSurroundingTextReceived(int callback_data,
                                                     const base::string16& text,
                                                     uint32_t start,
                                                     uint32_t end) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    ScopedJavaLocalRef<jstring> j_text = ConvertUTF16ToJavaString(env, text);
    Java_SmartSelectionClient_onSurroundingTextReceived(env, obj, callback_data,
                                                        j_text, start, end);
  }
}

}  // namespace content
