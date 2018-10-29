// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/background_sync_network_observer_android.h"

#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "jni/BackgroundSyncNetworkObserver_jni.h"

using base::android::JavaParamRef;

namespace content {

// static
scoped_refptr<BackgroundSyncNetworkObserverAndroid::Observer>
BackgroundSyncNetworkObserverAndroid::Observer::Create(
    base::Callback<void(network::mojom::ConnectionType)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scoped_refptr<BackgroundSyncNetworkObserverAndroid::Observer> observer(
      new BackgroundSyncNetworkObserverAndroid::Observer(callback));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::Bind(&BackgroundSyncNetworkObserverAndroid::Observer::Init,
                 observer));
  return observer;
}

void BackgroundSyncNetworkObserverAndroid::Observer::Init() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Attach a Java BackgroundSyncNetworkObserver object. Its lifetime will be
  // scoped to the lifetime of this object.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> obj(
      Java_BackgroundSyncNetworkObserver_createObserver(
          env, reinterpret_cast<jlong>(this)));
  j_observer_.Reset(obj);
}

BackgroundSyncNetworkObserverAndroid::Observer::~Observer() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BackgroundSyncNetworkObserver_removeObserver(
      env, j_observer_, reinterpret_cast<jlong>(this));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  j_observer_.Release();
}

void BackgroundSyncNetworkObserverAndroid::Observer::
    NotifyConnectionTypeChanged(JNIEnv* env,
                                const JavaParamRef<jobject>& jcaller,
                                jint new_connection_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::Bind(callback_, static_cast<network::mojom::ConnectionType>(
                                new_connection_type)));
}

BackgroundSyncNetworkObserverAndroid::Observer::Observer(
    base::Callback<void(network::mojom::ConnectionType)> callback)
    : callback_(callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

BackgroundSyncNetworkObserverAndroid::BackgroundSyncNetworkObserverAndroid(
    const base::Closure& network_changed_callback)
    : BackgroundSyncNetworkObserver(network_changed_callback),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  observer_ = Observer::Create(
      base::Bind(&BackgroundSyncNetworkObserverAndroid::OnConnectionChanged,
                 weak_ptr_factory_.GetWeakPtr()));
}

BackgroundSyncNetworkObserverAndroid::~BackgroundSyncNetworkObserverAndroid() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundSyncNetworkObserverAndroid::RegisterWithNetworkConnectionTracker(
    network::NetworkConnectionTracker* network_connection_tracker) {}

}  // namespace content
