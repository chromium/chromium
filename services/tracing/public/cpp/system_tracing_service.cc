// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/memory/ref_counted.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "build/build_config.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/tracing/public/cpp/system_tracing_service.h"
#include "third_party/perfetto/include/perfetto/tracing/default_socket.h"

namespace tracing {

namespace {

class ProducerSocketConnector
    : public base::RefCountedThreadSafe<ProducerSocketConnector> {
 public:
  using OpenProducerSocketCallback =
      SystemTracingService::OpenProducerSocketCallback;

  explicit ProducerSocketConnector(OpenProducerSocketCallback callback)
      : callback_(std::move(callback)) {}

  void Connect() {
    // base::RetainedRef() move() the argument so we need to create 2
    // scoped_refptr instances for
    // base::ThreadPool::PostTaskAndReplyWithResult().
    scoped_refptr<ProducerSocketConnector> req = this;
    scoped_refptr<ProducerSocketConnector> res = this;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&ProducerSocketConnector::ConnectSocket,
                       base::RetainedRef(req)),
        base::BindOnce(&ProducerSocketConnector::OnConnectSocketResult,
                       base::RetainedRef(res)));
  }

  void SetOnConnectCallbackForTesting(  // IN-TEST
      SystemTracingService::OnConnectCallback on_connect_callback) {
    on_connect_callback_for_testing_ = std::move(on_connect_callback);
  }

 private:
  friend class base::RefCountedThreadSafe<ProducerSocketConnector>;
  ~ProducerSocketConnector() {
    // In case we haven't been notified (e.g. ThreadPool shutdow), run the
    // callback with an invalid FD.
    RunCallback(base::File());
  }

  base::expected<bool, int> ConnectSocket() {
    std::string producer_sock_name = perfetto::GetProducerSocket();
    socket_fd_.reset(socket(AF_UNIX, SOCK_STREAM, 0));
    if (socket_fd_.get() == -1) {
      return base::unexpected(errno);
    }

    struct sockaddr_un saddr;
    memset(&saddr, 0, sizeof(saddr));
    memcpy(saddr.sun_path, producer_sock_name.data(),
           producer_sock_name.size());
    saddr.sun_family = AF_UNIX;
    auto size =
        static_cast<socklen_t>(__builtin_offsetof(sockaddr_un, sun_path) +
                               producer_sock_name.size() + 1);

    auto connect_rv = HANDLE_EINTR(
        connect(socket_fd_.get(),
                reinterpret_cast<const struct sockaddr*>(&saddr), size));
    // connect(2) return 0 on success and -1 on error.
    if (connect_rv == -1) {
      return base::unexpected(errno);
    }
    return base::ok(connect_rv == 0);
  }

  void OnConnectSocketResult(base::expected<bool, int> res) {
    if (res.has_value()) {
      DCHECK(res.value());
      return OnConnect(true);
    }
    DVLOG(3) << "Producer socket connection error: errno: " << res.error()
             << ": " << strerror(res.error());
    OnConnect(false);
  }

  void RunCallback(base::File fd) {
    if (!callback_) {
      return;
    }
    std::move(callback_).Run(std::move(fd));
  }

  // After Connect(), whether successful or not.
  void OnConnect(bool connected) {
    // Run |on_connect_callback_for_testing_| first if set for testing.
    if (on_connect_callback_for_testing_)
      std::move(on_connect_callback_for_testing_).Run(connected);

    // If not connected, run the mojo callback with an invalid FD to let the
    // child retry.
    if (!connected)
      return RunCallback(base::File());

    // Steal the FD of the underlying socket.
    base::File fd(socket_fd_.release());
    DCHECK(connected == fd.IsValid());
    RunCallback(std::move(fd));
  }

  base::ScopedPlatformFile socket_fd_;
  // The Mojo callback.
  OpenProducerSocketCallback callback_;
  // The callback for testing the OnConnect() event.
  SystemTracingService::OnConnectCallback on_connect_callback_for_testing_;
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
  auto connector =
      base::MakeRefCounted<ProducerSocketConnector>(std::move(callback));
  connector->Connect();
  // |connector| will self-destroy on connection success or failure.
}

void SystemTracingService::OpenProducerSocketForTesting(
    OpenProducerSocketCallback callback,
    OnConnectCallback on_connect_callback) {
  auto connector =
      base::MakeRefCounted<ProducerSocketConnector>(std::move(callback));
  connector->SetOnConnectCallbackForTesting(  // IN-TEST
      std::move(on_connect_callback));
  connector->Connect();
  // |connector| will self-destroy on connection success or failure.
}

}  // namespace tracing
