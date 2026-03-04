// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/selection/smart_selection_client.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/supports_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/SmartSelectionClient_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {
const void* const kSmartSelectionClientUDKey = &kSmartSelectionClientUDKey;

// This class deletes SmartSelectionClient when WebContents is destroyed.
class UserData : public base::SupportsUserData::Data {
 public:
  UserData() = delete;

  explicit UserData(SmartSelectionClient* client) : client_(client) {}

  UserData(const UserData&) = delete;
  UserData& operator=(const UserData&) = delete;

 private:
  std::unique_ptr<SmartSelectionClient> client_;
};
}

static int64_t JNI_SmartSelectionClient_Init(JNIEnv* env,
                                             WebContents* web_contents) {
  CHECK(web_contents)
      << "A SmartSelectionClient should be created with a valid WebContents.";

  if (auto* user_data = web_contents->GetUserData(kSmartSelectionClientUDKey)) {
    return reinterpret_cast<intptr_t>(user_data);
  }

  return reinterpret_cast<intptr_t>(new SmartSelectionClient(web_contents));
}

SmartSelectionClient::SmartSelectionClient(WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(!web_contents_->GetUserData(kSmartSelectionClientUDKey));
  web_contents_->SetUserData(kSmartSelectionClientUDKey,
                             std::make_unique<UserData>(this));
}

SmartSelectionClient::~SmartSelectionClient() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = GetJavaObject(env);
  if (!j_obj.is_null()) {
    Java_SmartSelectionClient_onNativeSideDestroyed(
        env, j_obj, reinterpret_cast<intptr_t>(this));
  }
}

void SmartSelectionClient::RequestSurroundingText(
    JNIEnv* env,
    int num_extra_characters,
    int callback_data) {
  RenderFrameHost* focused_frame = web_contents_->GetFocusedFrame();
  if (!focused_frame) {
    OnSurroundingTextReceived(callback_data, std::u16string(), 0, 0);
    return;
  }

  focused_frame->RequestTextSurroundingSelection(
      base::BindOnce(&SmartSelectionClient::OnSurroundingTextReceived,
                     weak_ptr_factory_.GetWeakPtr(), callback_data),
      num_extra_characters);
}

void SmartSelectionClient::CancelAllRequests(JNIEnv* env) {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SmartSelectionClient::OnSurroundingTextReceived(int callback_data,
                                                     const std::u16string& text,
                                                     uint32_t start,
                                                     uint32_t end) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaObject(env);
  if (!obj.is_null()) {
    ScopedJavaLocalRef<jstring> j_text = ConvertUTF16ToJavaString(env, text);
    Java_SmartSelectionClient_onSurroundingTextReceived(env, obj, callback_data,
                                                        j_text, start, end);
  }
}

base::android::ScopedJavaLocalRef<jobject> SmartSelectionClient::GetJavaObject(
    JNIEnv* env) {
  return Java_SmartSelectionClient_getFromWebContents(env, web_contents_);
}

}  // namespace content

DEFINE_JNI(SmartSelectionClient)
