// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/background_sync_network_observer_android.h"

#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/BackgroundSyncNetworkObserver_jni.h"

using base::android::JavaParamRef;

namespace content {

// static
scoped_refptr<BackgroundSyncNetworkObserverAndroid::Observer>
BackgroundSyncNetworkObserverAndroid::Observer::Create(
    base::RepeatingCallback<void(network::mojom::ConnectionType)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<BackgroundSyncNetworkObserverAndroid::Observer> observer(
      new BackgroundSyncNetworkObserverAndroid::Observer(callback));
  return observer;
}

void BackgroundSyncNetworkObserverAndroid::Observer::Init() {
  TRACE_EVENT0("startup",
               "BackgroundSyncNetworkObserverAndroid::Observer::Init");
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Attach a Java BackgroundSyncNetworkObserver object. Its lifetime will be
  // scoped to the lifetime of this object.
  JNIEnv* env = base::android::AttachCurrentThread();
  j_observer_ = Java_BackgroundSyncNetworkObserver_createObserver(
      env, reinterpret_cast<jlong>(this));
}

BackgroundSyncNetworkObserverAndroid::Observer::~Observer() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BackgroundSyncNetworkObserver_removeObserver(
      env, j_observer_, reinterpret_cast<jlong>(this));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BackgroundSyncNetworkObserverAndroid::Observer::
    NotifyConnectionTypeChanged(JNIEnv* env,
                                const JavaParamRef<jobject>& jcaller,
                                jint new_connection_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback_.Run(
      static_cast<network::mojom::ConnectionType>(new_connection_type));
}

BackgroundSyncNetworkObserverAndroid::Observer::Observer(
    base::RepeatingCallback<void(network::mojom::ConnectionType)> callback)
    : callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BackgroundSyncNetworkObserverAndroid::BackgroundSyncNetworkObserverAndroid(
    base::RepeatingClosure network_changed_callback)
    : BackgroundSyncNetworkObserver(std::move(network_changed_callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BackgroundSyncNetworkObserverAndroid::~BackgroundSyncNetworkObserverAndroid() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BackgroundSyncNetworkObserverAndroid::Init() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_ = Observer::Create(base::BindRepeating(
      &BackgroundSyncNetworkObserverAndroid::OnConnectionChanged,
      weak_ptr_factory_.GetWeakPtr()));
  observer_->Init();
}

void BackgroundSyncNetworkObserverAndroid::RegisterWithNetworkConnectionTracker(
    network::NetworkConnectionTracker* network_connection_tracker) {}

}  // namespace content
