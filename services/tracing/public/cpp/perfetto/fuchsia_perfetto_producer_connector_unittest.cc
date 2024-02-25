// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/fuchsia_perfetto_producer_connector.h"

#include <fcntl.h>
#include <fidl/fuchsia.tracing.perfetto/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/fdio/fd.h>
#include <lib/fidl/cpp/binding.h>
#include <zircon/errors.h>
#include <zircon/types.h>

#include <memory>
#include <optional>

#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/koid.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace {

// Returns true if the kernel objects |object0| and |object1| are related to
// each other (i.e. they are peers in a connected socket pair).
bool AreObjectsConnected(const zx::object_base& object0,
                         const zx::object_base& object1) {
  return base::GetKoid(object0) == base::GetRelatedKoid(object1);
}

zx::handle GetHandleFromFd(int fd) {
  DCHECK(fd != -1);
  zx::handle handle;
  zx_status_t status = fdio_fd_clone(fd, handle.reset_and_get_address());
  ZX_CHECK(status == ZX_OK, status) << "fdio_fd_transfer";
  return handle;
}

class FakeProducerConnectorService
    : public fidl::Server<fuchsia_tracing_perfetto::ProducerConnector> {
 public:
  explicit FakeProducerConnectorService(
      fidl::ServerEnd<fuchsia_tracing_perfetto::ProducerConnector> server_end)
      : binding_(async_get_default_dispatcher(),
                 std::move(server_end),
                 this,
                 fidl::kIgnoreBindingClosure) {}
  ~FakeProducerConnectorService() override = default;

  FakeProducerConnectorService(const FakeProducerConnectorService&) = delete;
  void operator=(const FakeProducerConnectorService&) = delete;

  // If set, ConnectProducer() will return the application error
  // ZX_ERR_NO_RESOURCES.
  void set_should_fail(bool should_fail) { should_fail_ = should_fail; }

  // Returns the socket endpoint passed in to ConnectProducer().
  zx::socket TakeSocket() {
    DCHECK(socket_) << "ConnectProducer() was not called yet.";
    return std::move(socket_);
  }

  // Sends a file descriptor to the remote BufferReceiver.
  void SendBuffer(base::ScopedFD fd) {
    zx::channel channel;
    zx_status_t status =
        fdio_fd_clone(fd.get(), channel.reset_and_get_address());
    ASSERT_EQ(status, ZX_OK);
    buffer_receiver_
        ->ProvideBuffer(fidl::ClientEnd<fuchsia_io::File>(std::move(channel)))
        .Then([](fidl::Result<
                  fuchsia_tracing_perfetto::BufferReceiver::ProvideBuffer>&
                     result) { ASSERT_FALSE(result.is_error()); });
  }

  // Disconnects the client of the ProducerConnector.
  void Close(zx_status_t status) { binding_.Close(status); }

 private:
  // fuchsia_tracing_perfetto::ProducerConnector implementation.
  void ConnectProducer(ConnectProducerRequest& request,
                       ConnectProducerCompleter::Sync& completer) final {
    if (should_fail_) {
      completer.Reply(fit::error(ZX_ERR_NO_RESOURCES));
    } else {
      socket_ = std::move(request.producer_socket());
      ASSERT_TRUE(request.buffer().Which() ==
                  fuchsia_tracing_perfetto::TraceBuffer::Tag::kFromServer);
      ASSERT_TRUE(request.buffer().from_server().has_value());
      buffer_receiver_.Bind(std::move(request.buffer().from_server().value()),
                            async_get_default_dispatcher());
      completer.Reply(fit::ok());
    }
  }

  bool should_fail_ = false;
  zx::socket socket_;
  fidl::ServerBinding<fuchsia_tracing_perfetto::ProducerConnector> binding_;
  fidl::Client<fuchsia_tracing_perfetto::BufferReceiver> buffer_receiver_;
};

class FuchsiaPerfettoProducerConnectorTest : public testing::Test {
 public:
  FuchsiaPerfettoProducerConnectorTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    connector_client_.emplace(task_environment_.GetMainThreadTaskRunner());
  }
  ~FuchsiaPerfettoProducerConnectorTest() override = default;

  void SetUp() override {
    base::Thread::Options thread_options(base::MessagePumpType::IO, 0);
    service_thread_.StartWithOptions(std::move(thread_options));
    auto endpoints =
        fidl::CreateEndpoints<fuchsia_tracing_perfetto::ProducerConnector>();
    ASSERT_TRUE(endpoints.is_ok());
    service_.emplace(service_thread_.task_runner(),
                     std::move(endpoints->server));
    connector_client_->SetProducerServiceForTest(std::move(endpoints->client));
  }

  void TearDown() override {
    // Tear down the connector and handle the resulting async deletion task.
    connector_client_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  bool Connect(base::ScopedFD* socket_fd,
               base::OnceCallback<int()>* receive_fd_cb) {
    auto conn_args = connector_client_->Connect();
    if (!conn_args) {
      return false;
    }

    *socket_fd = base::ScopedFD(conn_args->socket_fd.release());
    *receive_fd_cb =
        base::BindLambdaForTesting(conn_args->receive_shmem_fd_cb_fuchsia);
    return true;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::optional<FuchsiaPerfettoProducerConnector> connector_client_;

  // The fake service runs on a separate thread so that it can respond
  // when synchronous calls are made to it from the main thread.
  base::Thread service_thread_{"FakeProducerConnectorServiceThread"};
  base::SequenceBound<FakeProducerConnectorService> service_;
};

TEST_F(FuchsiaPerfettoProducerConnectorTest, Success) {
  const char kTestBufferContents[] = "foo";

  base::ScopedFD local_socket_fd;
  base::OnceCallback<int()> receive_fd_cb;
  ASSERT_TRUE(Connect(&local_socket_fd, &receive_fd_cb));
  ASSERT_TRUE(local_socket_fd.is_valid());

  zx::socket remote_socket;
  {
    base::RunLoop run_loop;
    service_.AsyncCall(&FakeProducerConnectorService::TakeSocket)
        .Then(base::BindLambdaForTesting(
            [&remote_socket, &run_loop](zx::socket socket) {
              remote_socket = std::move(socket);
              run_loop.Quit();
            }));
    run_loop.Run();
    ASSERT_TRUE(remote_socket);
    EXPECT_TRUE(AreObjectsConnected(GetHandleFromFd(local_socket_fd.get()),
                                    remote_socket));
  }

  // Verify that the buffer is being transferred properly by sending a file
  // across the FIDL boundary and verifying its contents.
  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::ScopedFD fd(open(temp_path.value().data(), O_RDWR | O_CREAT | O_TRUNC));
  ASSERT_TRUE(fd.is_valid());
  ASSERT_EQ(0, unlink(temp_path.value().data()));
  EXPECT_EQ(sizeof(kTestBufferContents),
            static_cast<size_t>(write(fd.get(), kTestBufferContents,
                                      sizeof(kTestBufferContents))));

  {
    base::RunLoop run_loop;
    service_.AsyncCall(&FakeProducerConnectorService::SendBuffer)
        .WithArgs(std::move(fd))
        .Then(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Take the FD from FuchsiaProducerConnector.
  base::ScopedFD received_fd(std::move(receive_fd_cb).Run());
  ASSERT_TRUE(received_fd.is_valid());

  // Ensure that the file is the same by examining its contents.
  char read_buf[sizeof(kTestBufferContents)];
  EXPECT_EQ(0, lseek(received_fd.get(), 0, SEEK_SET));
  EXPECT_EQ(sizeof(kTestBufferContents),
            static_cast<size_t>(read(received_fd.get(), read_buf,
                                     sizeof(kTestBufferContents))));
  EXPECT_EQ(0,
            memcmp(read_buf, kTestBufferContents, sizeof(kTestBufferContents)));
}

// Verify that errors in the service's application layer are handled properly.
TEST_F(FuchsiaPerfettoProducerConnectorTest, ApplicationError) {
  base::ScopedFD local_socket_fd;
  base::OnceCallback<int()> receive_fd_cb;

  service_.AsyncCall(&FakeProducerConnectorService::set_should_fail)
      .WithArgs(true);
  ASSERT_FALSE(Connect(&local_socket_fd, &receive_fd_cb));
}

// Ensure that the client handles the case where the system tracing service
// is not present, which will generally be true for production systems.
TEST_F(FuchsiaPerfettoProducerConnectorTest, SystemTracingServiceMissing) {
  base::ScopedFD local_socket_fd;
  base::OnceCallback<int()> receive_fd_cb;

  // Drop the handle at the remote end, which will produce a
  // ZX_ERR_PEER_CLOSED when the service is called by the connector.
  service_.AsyncCall(&FakeProducerConnectorService::Close)
      .WithArgs(ZX_ERR_UNAVAILABLE);
  ASSERT_FALSE(Connect(&local_socket_fd, &receive_fd_cb));
}

// Ensure that the client can gracefully recover if the system tracing service
// disconnects, e.g. due to a crash.
TEST_F(FuchsiaPerfettoProducerConnectorTest, SystemTracingServiceDisconnects) {
  base::ScopedFD local_socket_fd;
  base::OnceCallback<int()> receive_fd_cb;

  // Drop the handle at the remote end, which will produce a
  // ZX_ERR_PEER_CLOSED when the service is called by the connector.
  service_.AsyncCall(&FakeProducerConnectorService::Close)
      .WithArgs(ZX_ERR_PEER_CLOSED);
  ASSERT_FALSE(Connect(&local_socket_fd, &receive_fd_cb));
}

}  // namespace
}  // namespace tracing
