// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_REMOTE_SUPPORT_HOST_ASH_H_
#define REMOTING_HOST_CHROMEOS_REMOTE_SUPPORT_HOST_ASH_H_

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "remoting/host/it2me/it2me_native_messaging_host_ash.h"
#include "remoting/host/mojom/remote_support.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace remoting {

class It2MeNativeMessageHostAsh;
struct ChromeOsEnterpriseParams;

// This class represents a remote support host instance which can be connected
// to and controlled over an IPC channel. It wraps a remote support host
// implementation and enforces the single connection requirement. Method calls
// and destruction must occur on the same sequence as creation. This object's
// lifetime is tied to that of the wrapped host, meaning this instance will be
// destroyed when the IPC channel is disconnected.
class RemoteSupportHostAsh {
 public:
  explicit RemoteSupportHostAsh(base::OnceClosure cleanup_callback);
  RemoteSupportHostAsh(const RemoteSupportHostAsh&) = delete;
  RemoteSupportHostAsh& operator=(const RemoteSupportHostAsh&) = delete;
  ~RemoteSupportHostAsh();

  using StartSessionCallback =
      base::OnceCallback<void(mojom::StartSupportSessionResponsePtr response)>;

  // Returns a structure which includes the host version and supported features.
  static mojom::SupportHostDetailsPtr GetHostDetails();

  // Allows the caller to start a new remote support session.  |callback| is
  // called with the result.
  void StartSession(
      mojom::SupportSessionParamsPtr params,
      const absl::optional<ChromeOsEnterpriseParams>& enterprise_params,
      StartSessionCallback callback);

 private:
  void OnSessionDisconnected();

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<It2MeNativeMessageHostAsh> it2me_native_message_host_ash_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::OnceClosure cleanup_callback_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_REMOTE_SUPPORT_HOST_ASH_H_
