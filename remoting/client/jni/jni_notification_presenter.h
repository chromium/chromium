// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_NOTIFICATION_PRESENTER_H_
#define REMOTING_CLIENT_JNI_JNI_NOTIFICATION_PRESENTER_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "remoting/client/notification/notification_client.h"

namespace remoting {

// C++ counterpart for org.chromium.chromoting.jni.NotificationPresenter.
class JniNotificationPresenter final {
 public:
  explicit JniNotificationPresenter(
      const JavaObjectWeakGlobalRef& java_presenter);
  ~JniNotificationPresenter();

  void FetchNotification(JNIEnv* env,
                         const base::android::JavaParamRef<jstring>& username);
  void Destroy(JNIEnv* env);

 private:
  void OnNotificationFetched(base::Optional<NotificationMessage> notification);

  JavaObjectWeakGlobalRef java_presenter_;
  NotificationClient notification_client_;
  scoped_refptr<base::SequencedTaskRunner> sequence_;

  DISALLOW_COPY_AND_ASSIGN(JniNotificationPresenter);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_NOTIFICATION_PRESENTER_H_