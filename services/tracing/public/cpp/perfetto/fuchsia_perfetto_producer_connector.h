// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_FUCHSIA_PERFETTO_PRODUCER_CONNECTOR_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_FUCHSIA_PERFETTO_PRODUCER_CONNECTOR_H_

#include <fidl/fuchsia.tracing.perfetto/cpp/fidl.h>
#include <perfetto/ext/ipc/client.h>
#include <perfetto/ext/tracing/core/shared_memory.h>

#include <optional>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "base/tracing/perfetto_task_runner.h"

namespace tracing {

// Connects Perfetto sockets and shared memory channels on Fuchsia.
class COMPONENT_EXPORT(TRACING_CPP) FuchsiaPerfettoProducerConnector {
 public:
  explicit FuchsiaPerfettoProducerConnector(
      scoped_refptr<base::TaskRunner> perfetto_task_runner);
  ~FuchsiaPerfettoProducerConnector();

  // Returns a ConnArgs object with a socket connected to the system tracing
  // service if system tracing is provided by the platform.
  std::optional<perfetto::ipc::Client::ConnArgs> Connect();

  // Injects a ProducerConnector handle.
  void SetProducerServiceForTest(
      fidl::ClientEnd<fuchsia_tracing_perfetto::ProducerConnector> client_end);

 private:
  class BufferReceiverImpl;

  // Establishes a connection with the Perfetto system tracing service.
  // If successful, returns a file descriptor to the connected socket,
  // and a SharedMemoryTransport which will be used to propagate shared
  // memory buffers from the system tracing service to the client.
  // Returns an unset file descriptor and a null pointer if the system
  // tracing service could not be used.
  base::ScopedFD ConnectSocket();

  // Blocks until the tracing service provides a shared memory file descriptor.
  // Returns an invalid (-1) FD on timeout or error.
  int WaitForSharedMemoryFd();

  // Called when the BufferReceiver service has received a buffer from
  // the system tracing service.
  void OnSharedMemoryFdReceived(base::ScopedFD fd);

  fidl::ClientEnd<fuchsia_tracing_perfetto::ProducerConnector>
      producer_connector_client_end_for_test_;

  // Event used to synchronously wait until a file descriptor is received
  // from the system tracing service.
  base::WaitableEvent fd_received_event_{
      base::WaitableEvent::ResetPolicy::AUTOMATIC};

  // The file descriptor handled from |buffer_receiver_|.
  // Can be written on |buffer_receiver_thread_|, and read from any task runner.
  // Can only be read after |fd_received_event_| is signaled.
  base::ScopedFD received_fd_;

  // Implementation of the BufferReceiver FIDL service, used to receive
  // shared memory buffers from the system tracing service.
  // It runs on its own thread so that it will continue to operate even when
  // Perfetto is synchronously waiting for a buffer to arrive.
  // Deletion of |buffer_receiver_| will take place on |deletion_task_runner_|,
  // so as to prevent any use-after-free bugs if there are pending tasks which
  // reference BufferReceiver's callback.
  std::unique_ptr<base::Thread> buffer_receiver_thread_;
  base::SequenceBound<BufferReceiverImpl> buffer_receiver_;
  scoped_refptr<base::TaskRunner> deletion_task_runner_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_FUCHSIA_PERFETTO_PRODUCER_CONNECTOR_H_
