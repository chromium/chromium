// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_SYSTEM_TRACING_SERVICE_H_
#define SERVICES_TRACING_PUBLIC_CPP_SYSTEM_TRACING_SERVICE_H_

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/tracing/public/mojom/system_tracing_service.mojom.h"
#include "third_party/perfetto/include/perfetto/ext/base/unix_socket.h"

namespace tracing {

// Mojo implementation between a child and the browser process for connecting to
// the Perfetto system tracing service daemon (traced) from within a sandboxed
// child process. This enables system-wide trace collection with
// traced that includes trace data from all Chrome processes.
//
// Example:
//   (In the browser process: bind the receiver and pass the pending remote
//    using the ChildProcess mojo interface)
//   auto system_tracing_service = std::make_unique<SystemTracingService>();
//   child_process->EnableSystemTracingService(
//       system_tracing_service->BindAndPassPendingRemote());
//
//   (In the child process, where a pending remote is received)
//   mojo::SharedRemote<SystemTracingService> remote;
//   remote.Bind(std::move(pending_remote));
//   (Request to open the producer socket in the browser process)
//   OpenProducerSocketCallback cb = ...;
//   remote->OpenProducerSocket(std::move(cb));
class COMPONENT_EXPORT(TRACING_CPP) SystemTracingService
    : public mojom::SystemTracingService {
 public:
  SystemTracingService();

  SystemTracingService(const SystemTracingService&) = delete;
  SystemTracingService& operator=(const SystemTracingService&) = delete;

  ~SystemTracingService() override;

  void OpenProducerSocket(OpenProducerSocketCallback callback) override;

  using OnConnectCallback = base::OnceCallback<void(bool)>;
  // Same as OpenProducerSocket() with an extra parameter for testing the socket
  // connection callback.
  void OpenProducerSocketForTesting(OpenProducerSocketCallback callback,
                                    OnConnectCallback on_connect_callback);

  mojo::PendingRemote<mojom::SystemTracingService> BindAndPassPendingRemote();

 private:
  void OnConnectionError();

  mojo::Receiver<mojom::SystemTracingService> receiver_{this};
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_SYSTEM_TRACING_SERVICE_H_
