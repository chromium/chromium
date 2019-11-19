// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "mojo/public/java/system/system_impl_java_jni_headers/WatcherImpl_jni.h"

namespace mojo {
namespace android {

using base::android::JavaParamRef;

namespace {

class WatcherImpl {
 public:
  WatcherImpl()
      : watcher_(FROM_HERE,
                 SimpleWatcher::ArmingPolicy::AUTOMATIC,
                 base::SequencedTaskRunnerHandle::Get()) {}

  ~WatcherImpl() = default;

  jint Start(JNIEnv* env,
             const JavaParamRef<jobject>& jcaller,
             jint mojo_handle,
             jint signals) {
    java_watcher_.Reset(env, jcaller);

    auto ready_callback = base::BindRepeating(&WatcherImpl::OnHandleReady,
                                              base::Unretained(this));

    MojoResult result =
        watcher_.Watch(mojo::Handle(static_cast<MojoHandle>(mojo_handle)),
                       static_cast<MojoHandleSignals>(signals), ready_callback);
    if (result != MOJO_RESULT_OK)
      java_watcher_.Reset();

    return result;
  }

  void Cancel() {
    java_watcher_.Reset();
    watcher_.Cancel();
  }

 private:
  void OnHandleReady(MojoResult result) {
    DCHECK(!java_watcher_.is_null());

    base::android::ScopedJavaGlobalRef<jobject> java_watcher_preserver;
    if (result == MOJO_RESULT_CANCELLED)
      java_watcher_preserver = std::move(java_watcher_);

    Java_WatcherImpl_onHandleReady(
        base::android::AttachCurrentThread(),
        java_watcher_.is_null() ? java_watcher_preserver : java_watcher_,
        result);
  }

  SimpleWatcher watcher_;
  base::android::ScopedJavaGlobalRef<jobject> java_watcher_;

  DISALLOW_COPY_AND_ASSIGN(WatcherImpl);
};

}  // namespace

static jlong JNI_WatcherImpl_CreateWatcher(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  return reinterpret_cast<jlong>(new WatcherImpl);
}

static jint JNI_WatcherImpl_Start(JNIEnv* env,
                                  const JavaParamRef<jobject>& jcaller,
                                  jlong watcher_ptr,
                                  jint mojo_handle,
                                  jint signals) {
  auto* watcher = reinterpret_cast<WatcherImpl*>(watcher_ptr);
  return watcher->Start(env, jcaller, mojo_handle, signals);
}

static void JNI_WatcherImpl_Cancel(JNIEnv* env,
                                   const JavaParamRef<jobject>& jcaller,
                                   jlong watcher_ptr) {
  reinterpret_cast<WatcherImpl*>(watcher_ptr)->Cancel();
}

static void JNI_WatcherImpl_Delete(JNIEnv* env,
                                   const JavaParamRef<jobject>& jcaller,
                                   jlong watcher_ptr) {
  delete reinterpret_cast<WatcherImpl*>(watcher_ptr);
}

}  // namespace android
}  // namespace mojo
