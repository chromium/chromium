// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/fuchsia_perfetto_producer_connector.h"

#include <fidl/fuchsia.tracing.perfetto/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/fdio/fd.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/zx/socket.h>
#include <lib/zx/vmo.h>
#include <perfetto/ext/tracing/core/shared_memory.h>

#include "base/files/scoped_file.h"
#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/message_loop/message_pump_type.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"

namespace tracing {

// Receives shared memory buffers over FIDL and sends them to a
// SharedMemoryTransport receiver.
// Service runs on a dedicated thread because the Perfetto thread can
// synchronously block while waiting for a FD to arrive, and we don't want it
// to interrupt the handling of FIDL messages.
class FuchsiaPerfettoProducerConnector::BufferReceiverImpl
    : public fidl::Server<fuchsia_tracing_perfetto::BufferReceiver> {
 public:
  BufferReceiverImpl(
      fidl::ServerEnd<fuchsia_tracing_perfetto::BufferReceiver> server_end,
      base::RepeatingCallback<void(base::ScopedFD)> on_fd_received)
      : binding_(async_get_default_dispatcher(),
                 std::move(server_end),
                 this,
                 fidl::kIgnoreBindingClosure),
        on_fd_received_(on_fd_received) {}
  ~BufferReceiverImpl() override = default;

  void ProvideBuffer(ProvideBufferRequest& request,
                     ProvideBufferCompleter::Sync& completer) final {
    if (!request.buffer()) {
      LOG(ERROR) << "Received invalid file handle.";
      on_fd_received_.Run({});
      completer.Reply(fit::error(ZX_ERR_BAD_HANDLE));
      return;
    }

    zx::channel file_handle = request.buffer().TakeChannel();
    base::ScopedFD shmem_fd;
    zx_status_t status = fdio_fd_create(
        file_handle.release(), base::ScopedFD::Receiver(shmem_fd).get());
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "fdio_fd_create";
      on_fd_received_.Run({});
      completer.Reply(fit::error(ZX_ERR_INVALID_ARGS));
      return;
    }
    on_fd_received_.Run(std::move(shmem_fd));

    completer.Reply(fit::ok());
  }

 private:
  fidl::ServerBinding<fuchsia_tracing_perfetto::BufferReceiver> binding_;

  // Called when a buffer is received from ProvideBuffer().
  base::RepeatingCallback<void(base::ScopedFD)> on_fd_received_;
};

FuchsiaPerfettoProducerConnector::FuchsiaPerfettoProducerConnector(
    scoped_refptr<base::TaskRunner> perfetto_task_runner)
    : buffer_receiver_thread_(
          std::make_unique<base::Thread>("BufferReceiverThread")),
      deletion_task_runner_(std::move(perfetto_task_runner)) {}

FuchsiaPerfettoProducerConnector::~FuchsiaPerfettoProducerConnector() {
  // Avoid UAF raciness by ensuring that the BufferReceiver is deleted on
  // the sequence that accesses it.
  deletion_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::SequenceBound<BufferReceiverImpl> receiver,
             std::unique_ptr<base::Thread>) {
            // Destroy |receiver| while its thread is alive, then allow the
            // thread to fall out of scope and stop.
            receiver.Reset();
          },
          std::move(buffer_receiver_), std::move(buffer_receiver_thread_)));
}

std::optional<perfetto::ipc::Client::ConnArgs>
FuchsiaPerfettoProducerConnector::Connect() {
  auto socket = ConnectSocket();
  if (!socket.is_valid()) {
    return std::nullopt;
  }
  perfetto::ipc::Client::ConnArgs conn_args(
      perfetto::base::ScopedSocketHandle(socket.release()));
  conn_args.receive_shmem_fd_cb_fuchsia = [this]() {
    return WaitForSharedMemoryFd();
  };
  return conn_args;
}

void FuchsiaPerfettoProducerConnector::SetProducerServiceForTest(
    fidl::ClientEnd<fuchsia_tracing_perfetto::ProducerConnector> client_end) {
  producer_connector_client_end_for_test_ = std::move(client_end);
}

base::ScopedFD FuchsiaPerfettoProducerConnector::ConnectSocket() {
  // Create a connected kernel socket pair. |remote_socket| will be sent
  // over FIDL.
  zx::socket client_socket, remote_socket;
  zx_status_t status = zx::socket::create(0, &client_socket, &remote_socket);
  ZX_CHECK(status == ZX_OK, status) << "zx_socket_create";

  auto receiver_endpoints =
      fidl::CreateEndpoints<fuchsia_tracing_perfetto::BufferReceiver>();
  ZX_CHECK(receiver_endpoints.is_ok(), receiver_endpoints.status_value());
  auto trace_buffer = fuchsia_tracing_perfetto::TraceBuffer::WithFromServer(
      std::move(receiver_endpoints->client));

  // Call the ProducerConnector FIDL service.
  // Call is synchronous so that the caller can perform error handling if the
  // system tracing service is unavailable.
  fidl::SyncClient<fuchsia_tracing_perfetto::ProducerConnector>
      producer_connector_sync;
  if (producer_connector_client_end_for_test_) {
    producer_connector_sync.Bind(
        std::move(producer_connector_client_end_for_test_));
  } else {
    auto producer_connector_client_end = base::fuchsia_component::Connect<
        fuchsia_tracing_perfetto::ProducerConnector>();
    if (producer_connector_client_end.is_error()) {
      LOG(WARNING) << base::FidlConnectionErrorMessage(
                          producer_connector_client_end)
                   << ", system tracing disabled";
      return {};
    }
    producer_connector_sync.Bind(
        std::move(producer_connector_client_end.value()));
  }

  auto result = producer_connector_sync->ConnectProducer({{
      .producer_socket = std::move(remote_socket),
      .buffer = std::move(trace_buffer),
  }});
  if (result.is_error()) {
    zx_status_t error_value =
        result.error_value().is_framework_error()
            ? result.error_value().framework_error().status()
            : result.error_value().domain_error();
    ZX_LOG(WARNING, error_value)
        << "Error calling ProducerConnector::ConnectProducer, system tracing "
           "disabled.";
    return {};
  }

  // Create a dedicated thread for handling BufferReceiver calls.
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  thread_options.joinable = true;
  buffer_receiver_thread_->StartWithOptions(std::move(thread_options));
  buffer_receiver_ = base::SequenceBound<BufferReceiverImpl>(
      buffer_receiver_thread_->task_runner(),
      std::move(receiver_endpoints->server),
      base::BindRepeating(
          &FuchsiaPerfettoProducerConnector::OnSharedMemoryFdReceived,
          base::Unretained(this)));

  base::ScopedFD socket_fd;
  status = fdio_fd_create(client_socket.release(),
                          base::ScopedFD::Receiver(socket_fd).get());
  ZX_CHECK(status == ZX_OK, status) << "fdio_fd_create";
  DCHECK(socket_fd.is_valid());
  return socket_fd;
}

int FuchsiaPerfettoProducerConnector::WaitForSharedMemoryFd() {
  constexpr base::TimeDelta kWaitForShmemTimeout = base::Seconds(5);
  base::ScopedAllowBaseSyncPrimitives allow_blocking;
  if (!fd_received_event_.TimedWait(kWaitForShmemTimeout)) {
    LOG(WARNING) << "Timed out while waiting for shared memory.";
    return -1;
  }
  return received_fd_.release();
}

void FuchsiaPerfettoProducerConnector::OnSharedMemoryFdReceived(
    base::ScopedFD fd) {
  DCHECK(!fd_received_event_.IsSignaled());
  received_fd_ = std::move(fd);
  fd_received_event_.Signal();
}

}  // namespace tracing
