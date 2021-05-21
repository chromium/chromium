// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_NETWORK_ACTIVATION_REQUEST_H_
#define NET_ANDROID_NETWORK_ACTIVATION_REQUEST_H_

#include "base/android/jni_android.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
namespace android {

// NetworkActivationRequest asks Android to activate a network connection which
// fits a specified set of constraints. The system may choose to fulfill the
// request with an already-established network connection, or it may activate a
// new connection specifically to satisfy this request. In the latter case the
// connection may be deactivated upon destruction of this object.
class NET_EXPORT_PRIVATE NetworkActivationRequest {
 public:
  using NetworkHandle = NetworkChangeNotifier::NetworkHandle;

  enum class TransportType {
    // Requests a network connection which uses a mobile network for transport.
    kMobile,
  };

  // Requests an Internet-connected network which satisfies the given
  // `transport` constraint.
  explicit NetworkActivationRequest(TransportType transport);
  NetworkActivationRequest(const NetworkActivationRequest&) = delete;
  NetworkActivationRequest& operator=(const NetworkActivationRequest&) = delete;
  ~NetworkActivationRequest();

  // Exposes a handle to the network currently activated by the system on behalf
  // of this request, if any.
  const absl::optional<NetworkHandle>& activated_network() const {
    return activated_network_;
  }

  // Called from Java via JNI. May be called from any thread, but the ability
  // to call it is managed synchronously at construction and destruction of this
  // NetworkActivationRequest.
  void NotifyAvailable(JNIEnv* env, NetworkHandle network);

 private:
  void NotifyAvailableOnCorrectSequence(NetworkHandle network);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtr<NetworkActivationRequest> weak_self_;
  base::android::ScopedJavaGlobalRef<jobject> java_request_;
  absl::optional<NetworkHandle> activated_network_;
  base::WeakPtrFactory<NetworkActivationRequest> weak_ptr_factory_{this};
};

}  // namespace android
}  // namespace net

#endif  // NET_ANDROID_NETWORK_ACTIVATION_REQUEST_H_
