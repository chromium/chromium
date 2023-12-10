// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_NOTIFICATION_PRESENTER_H_
#define REMOTING_CLIENT_JNI_JNI_NOTIFICATION_PRESENTER_H_

#include <jni.h>

#include <optional>
#include "base/android/jni_weak_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/client/notification/notification_client.h"

namespace remoting {

// C++ counterpart for org.chromium.chromoting.jni.NotificationPresenter.
class JniNotificationPresenter final {
 public:
  explicit JniNotificationPresenter(
      const JavaObjectWeakGlobalRef& java_presenter);

  JniNotificationPresenter(const JniNotificationPresenter&) = delete;
  JniNotificationPresenter& operator=(const JniNotificationPresenter&) = delete;

  ~JniNotificationPresenter();

  void FetchNotification(JNIEnv* env,
                         const base::android::JavaParamRef<jstring>& username);
  void Destroy(JNIEnv* env);

 private:
  void OnNotificationFetched(std::optional<NotificationMessage> notification);

  JavaObjectWeakGlobalRef java_presenter_;
  NotificationClient notification_client_;
  scoped_refptr<base::SequencedTaskRunner> sequence_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_NOTIFICATION_PRESENTER_H_
