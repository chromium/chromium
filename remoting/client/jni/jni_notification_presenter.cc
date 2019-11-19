// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_notification_presenter.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/android/jni_headers/NotificationPresenter_jni.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/notification/notification_message.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace remoting {

JniNotificationPresenter::JniNotificationPresenter(
    const JavaObjectWeakGlobalRef& java_presenter)
    : java_presenter_(java_presenter),
      notification_client_(
          ChromotingClientRuntime::GetInstance()->network_task_runner()),
      sequence_(base::SequencedTaskRunnerHandle::Get()) {}

JniNotificationPresenter::~JniNotificationPresenter() {
  DCHECK(sequence_->RunsTasksInCurrentSequence());
}

void JniNotificationPresenter::FetchNotification(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& username) {
  DCHECK(sequence_->RunsTasksInCurrentSequence());
  std::string username_str = ConvertJavaStringToUTF8(env, username);
  // Safe to use unretained: callback is dropped once client is deleted.
  notification_client_.GetNotification(
      username_str,
      base::BindOnce(&JniNotificationPresenter::OnNotificationFetched,
                     base::Unretained(this)));
}

void JniNotificationPresenter::Destroy(JNIEnv* env) {
  if (sequence_->RunsTasksInCurrentSequence()) {
    delete this;
  } else {
    sequence_->DeleteSoon(FROM_HERE, this);
  }
}

void JniNotificationPresenter::OnNotificationFetched(
    base::Optional<NotificationMessage> notification) {
  DCHECK(sequence_->RunsTasksInCurrentSequence());
  JNIEnv* env = base::android::AttachCurrentThread();
  auto java_presenter = java_presenter_.get(env);
  if (!notification) {
    Java_NotificationPresenter_onNoNotification(env, java_presenter);
    return;
  }
  auto j_message_id = ConvertUTF8ToJavaString(env, notification->message_id);
  auto j_message_text =
      ConvertUTF8ToJavaString(env, notification->message_text);
  auto j_link_text = ConvertUTF8ToJavaString(env, notification->link_text);
  auto j_link_url = ConvertUTF8ToJavaString(env, notification->link_url);
  auto j_allow_silence = notification->allow_silence;
  Java_NotificationPresenter_onNotificationFetched(
      env, java_presenter, j_message_id, j_message_text, j_link_text,
      j_link_url, j_allow_silence);
}

static jlong JNI_NotificationPresenter_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_presenter) {
  return reinterpret_cast<intptr_t>(new JniNotificationPresenter(
      JavaObjectWeakGlobalRef(env, java_presenter)));
}

}  // namespace remoting
