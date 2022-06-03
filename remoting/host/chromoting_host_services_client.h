// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_SERVICES_CLIENT_H_
#define REMOTING_HOST_CHROMOTING_HOST_SERVICES_CLIENT_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"

namespace mojo {
class IsolatedConnection;
}  // namespace mojo

namespace remoting {

// Maintains connection to a ChromotingHostServices server, and provides the
// ChromotingHostServices interface. Note that each process should have only one
// ChromotingHostServicesClient instance. Making multiple connections to the
// ChromotingHostServices server is not supported.
class ChromotingHostServicesClient final {
 public:
  ChromotingHostServicesClient();
  ChromotingHostServicesClient(const ChromotingHostServicesClient&) = delete;
  ChromotingHostServicesClient& operator=(const ChromotingHostServicesClient&) =
      delete;
  ~ChromotingHostServicesClient();

  // Gets the ChromotingHostServices. Always null-check before using it, as
  // nullptr will be returned if the connection has failed to make.
  mojom::ChromotingHostServices* Get() const;

 private:
  // Attempts to connect to the IPC server if the connection has not been
  // established. Returns a boolean indicating whether there is a valid IPC
  // connection to the chromoting host.
  bool EnsureConnection();

  void OnDisconnected();

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::NamedPlatformChannel::ServerName server_name_;
  std::unique_ptr<mojo::IsolatedConnection> connection_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::ChromotingHostServices> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_SERVICES_CLIENT_H_
