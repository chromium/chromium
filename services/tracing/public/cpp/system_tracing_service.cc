// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/system_tracing_service.h"
#include "third_party/perfetto/include/perfetto/ext/base/unix_socket.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/ipc/default_socket.h"  // nogncheck

namespace tracing {

namespace {

using ::perfetto::base::UnixSocket;

class UnixSocketEventListener : private UnixSocket::EventListener {
 public:
  using OpenProducerSocketCallback =
      SystemTracingService::OpenProducerSocketCallback;

  explicit UnixSocketEventListener(OpenProducerSocketCallback callback)
      : callback_(std::move(callback)),
        callback_sequence_(base::SequencedTaskRunnerHandle::Get()) {}
  ~UnixSocketEventListener() override = default;

  void Connect() {
    auto* task_runner = tracing::PerfettoTracedProcess::Get()->GetTaskRunner();
    if (!task_runner->RunsTasksOnCurrentThread()) {
      // The socket needs to be opened on the |task_runner| sequence.
      task_runner->PostTask([self = this]() { self->Connect(); });
      return;
    }

    std::string socket_name = perfetto::GetProducerSocket();
    socket_ = perfetto::base::UnixSocket::Connect(
        socket_name, this, task_runner, perfetto::base::SockFamily::kUnix,
        perfetto::base::SockType::kStream);
  }

 private:
  void RunCallback(base::File fd) {
    if (!callback_)
      return;

    callback_sequence_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](OpenProducerSocketCallback callback, base::File fd) {
                         std::move(callback).Run(std::move(fd));
                       },
                       std::move(callback_), std::move(fd)));
    // |callback_| and |fd| are moved in posting the task to
    // |callback_sequence_|. The task doesn't need to touch any data member of
    // |this|. It's safe to self-destruct.
    delete this;
  }

  void OnNewIncomingConnection(
      UnixSocket* self,
      std::unique_ptr<UnixSocket> new_connection) override {
    NOTREACHED();
  }

  // After Connect(), whether successful or not.
  void OnConnect(UnixSocket* self, bool connected) override {
    DCHECK(self == socket_.get());
    // Steal the FD of the underlying socket.
    base::File fd(self->ReleaseSocket().ReleaseFd().release());
    DCHECK(connected == fd.IsValid());
    RunCallback(std::move(fd));
  }

  void OnDisconnect(UnixSocket* self) override { RunCallback(base::File()); }

  void OnDataAvailable(UnixSocket* self) override {
    // Should be non-reachable, but just do nothing so we don't read any data
    // from the socket in case it ever reaches this callback.
  }

  std::unique_ptr<UnixSocket> socket_;
  OpenProducerSocketCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> callback_sequence_;
};

}  // Anonymous namespace

SystemTracingService::SystemTracingService() = default;
SystemTracingService::~SystemTracingService() = default;

mojo::PendingRemote<mojom::SystemTracingService>
SystemTracingService::BindAndPassPendingRemote() {
  DCHECK(!receiver_.is_bound());
  auto pending_remote = receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(base::BindOnce(
      &SystemTracingService::OnConnectionError, base::Unretained(this)));
  return pending_remote;
}

void SystemTracingService::OnConnectionError() {
  receiver_.reset();
}

void SystemTracingService::OpenProducerSocket(
    OpenProducerSocketCallback callback) {
  auto* connect_listener = new UnixSocketEventListener(std::move(callback));
  connect_listener->Connect();
  // |connect_listener| will self-destroy on connection successful.
}

}  // namespace tracing
