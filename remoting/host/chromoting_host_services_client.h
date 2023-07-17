// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_SERVICES_CLIENT_H_
#define REMOTING_HOST_CHROMOTING_HOST_SERVICES_CLIENT_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/chromoting_host_services_provider.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"

namespace base {
class Environment;
}  // namespace base

namespace remoting {

// Maintains connection to a ChromotingHostServices server, and provides the
// ChromotingHostServices interface. Note that each process should have only one
// ChromotingHostServicesClient instance. Making multiple connections to the
// ChromotingHostServices server is not supported.
class ChromotingHostServicesClient final
    : public ChromotingHostServicesProvider {
 public:
  ChromotingHostServicesClient();
  ChromotingHostServicesClient(const ChromotingHostServicesClient&) = delete;
  ChromotingHostServicesClient& operator=(const ChromotingHostServicesClient&) =
      delete;
  ~ChromotingHostServicesClient() override;

  // Configures the current process to allow it to communicate with the
  // ChromotingHostServices server. Must be called once before using any
  // instance of ChromotingHostServicesClient.
  // Returns a boolean that indicates whether the initialization succeeded.
  static bool Initialize();

  // Gets the ChromotingSessionServices. Always null-check before using it, as
  // nullptr will be returned if the connection could not be established.
  // Note that when the session is not remoted, you will still get a callable
  // interface, but all outgoing IPCs will be silently dropped, and any pending
  // receivers/remotes/message pipes sent will be closed.
  mojom::ChromotingSessionServices* GetSessionServices() const override;

 private:
  friend class ChromotingHostServicesClientTest;

  using ConnectToServerCallback = base::RepeatingCallback<
      mojo::PendingRemote<mojom::ChromotingHostServices>()>;

#if BUILDFLAG(IS_LINUX)
  static constexpr char kChromeRemoteDesktopSessionEnvVar[] =
      "CHROME_REMOTE_DESKTOP_SESSION";
#endif

  ChromotingHostServicesClient(std::unique_ptr<base::Environment> environment,
                               ConnectToServerCallback connect_to_server);

  // Attempts to connect to the IPC server if the connection has not been
  // established. Returns a boolean indicating whether there is a valid IPC
  // connection to the chromoting host.
  bool EnsureConnection();

  bool EnsureSessionServicesBinding();

  void OnDisconnected();
  void OnSessionDisconnected();

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<base::Environment> environment_;
  ConnectToServerCallback connect_to_server_;
  mojo::Remote<mojom::ChromotingHostServices> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::ChromotingSessionServices> session_services_remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::OnceClosure on_session_disconnected_callback_for_testing_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_SERVICES_CLIENT_H_
