// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_activation_request.h"

#include "base/android/jni_android.h"
#include "net/net_jni_headers/NetworkActivationRequest_jni.h"

namespace net {
namespace android {

NetworkActivationRequest::NetworkActivationRequest(TransportType transport)
    : task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  weak_self_ = weak_ptr_factory_.GetWeakPtr();
  java_request_ = Java_NetworkActivationRequest_createMobileNetworkRequest(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

NetworkActivationRequest::~NetworkActivationRequest() {
  Java_NetworkActivationRequest_unregister(base::android::AttachCurrentThread(),
                                           java_request_);
}

void NetworkActivationRequest::NotifyAvailable(JNIEnv* env,
                                               NetworkHandle network) {
  // This state is safe to access unsynchronized because we know (a) it doesn't
  // change after `this` is constructed and (b) this method is never invoked
  // beyond the destructor of this object. Synchronization takes place Java-side
  // during request registration and unregistration.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NetworkActivationRequest::NotifyAvailableOnCorrectSequence,
          weak_self_, network));
}

void NetworkActivationRequest::NotifyAvailableOnCorrectSequence(
    NetworkHandle network) {
  activated_network_ = network;
}

}  // namespace android
}  // namespace net
