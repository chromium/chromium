// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/network_service_async_socket.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_default.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_source.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_config_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/proxy_resolving_socket_factory_mojo.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/rtc_base/third_party/sigslot/sigslot.h"

namespace jingle_glue {

namespace {

// Data provider that handles reads/writes for NetworkServiceAsyncSocket.
class AsyncSocketDataProvider : public net::SocketDataProvider {
 public:
  AsyncSocketDataProvider() : has_pending_read_(false) {}

  ~AsyncSocketDataProvider() override {
    EXPECT_TRUE(writes_.empty());
    EXPECT_TRUE(reads_.empty());
  }

  // If there's no read, sets the "has pending read" flag.  Otherwise,
  // pops the next read.
  net::MockRead OnRead() override {
    if (reads_.empty()) {
      DCHECK(!has_pending_read_);
      has_pending_read_ = true;
      const net::MockRead pending_read(net::SYNCHRONOUS, net::ERR_IO_PENDING);
      return pending_read;
    }
    net::MockRead mock_read = reads_.front();
    reads_.pop_front();
    return mock_read;
  }

  void CancelPendingRead() override {
    DCHECK(has_pending_read_);
    has_pending_read_ = false;
  }

  // Simply pops the next write and, if applicable, compares it to
  // |data|.
  net::MockWriteResult OnWrite(const std::string& data) override {
    DCHECK(!writes_.empty());
    net::MockWrite mock_write = writes_.front();
    writes_.pop_front();
    if (mock_write.result != net::OK) {
      return net::MockWriteResult(mock_write.mode, mock_write.result);
    }
    std::string expected_data(mock_write.data, mock_write.data_len);
    EXPECT_EQ(expected_data, data);
    if (expected_data != data) {
      return net::MockWriteResult(net::SYNCHRONOUS, net::ERR_UNEXPECTED);
    }
    return net::MockWriteResult(mock_write.mode, data.size());
  }

  // We ignore resets so we can pre-load the socket data provider with
  // read/write events.
  void Reset() override {}

  // If there is a pending read, completes it with the given read.
  // Otherwise, queues up the given read.
  void AddRead(const net::MockRead& mock_read) {
    DCHECK_NE(mock_read.result, net::ERR_IO_PENDING);
    if (has_pending_read_) {
      has_pending_read_ = false;
      socket()->OnReadComplete(mock_read);
      return;
    }
    reads_.push_back(mock_read);
  }

  // Simply queues up the given write.
  void AddWrite(const net::MockWrite& mock_write) {
    writes_.push_back(mock_write);
  }

  bool AllReadDataConsumed() const override { return reads_.empty(); }

  bool AllWriteDataConsumed() const override { return writes_.empty(); }

 private:
  base::circular_deque<net::MockRead> reads_;
  bool has_pending_read_;

  base::circular_deque<net::MockWrite> writes_;

  DISALLOW_COPY_AND_ASSIGN(AsyncSocketDataProvider);
};

class MockProxyResolvingSocket : public network::mojom::ProxyResolvingSocket {
 public:
  enum Event {
    // These names are from the perspective of the client, not the socket.
    kRead,
    kWrite,
    kCloseReadPipe,
    kCloseWritePipe,
    kReadError,
    kReadErrorInvalid,
    kWriteError,
    kWriteErrorInvalid,
    kReadEofError,
    kCloseObserverPipe,
  };

  MockProxyResolvingSocket() {}
  ~MockProxyResolvingSocket() override {}

  void Connect(mojo::PendingRemote<network::mojom::SocketObserver> observer,
               network::mojom::ProxyResolvingSocketFactory::
                   CreateProxyResolvingSocketCallback callback) {
    mojo::DataPipe send_pipe;
    mojo::DataPipe receive_pipe;

    observer_.Bind(std::move(observer));
    receive_pipe_handle_ = std::move(receive_pipe.producer_handle);
    send_pipe_handle_ = std::move(send_pipe.consumer_handle);

    std::move(callback).Run(net::OK, base::nullopt, base::nullopt,
                            std::move(receive_pipe.consumer_handle),
                            std::move(send_pipe.producer_handle));
  }

  void RunEvents(std::vector<Event>&& events);

  // mojom::ProxyResolvingSocket implementation.
  void UpgradeToTLS(
      const net::HostPortPair& host_port_pair,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TLSClientSocket> receiver,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      network::mojom::ProxyResolvingSocket::UpgradeToTLSCallback callback)
      override {
    NOTREACHED();
  }

 private:
  mojo::Remote<network::mojom::SocketObserver> observer_;
  mojo::ScopedDataPipeProducerHandle receive_pipe_handle_;
  mojo::ScopedDataPipeConsumerHandle send_pipe_handle_;

  DISALLOW_COPY_AND_ASSIGN(MockProxyResolvingSocket);
};

class MockProxyResolvingSocketFactory
    : public network::mojom::ProxyResolvingSocketFactory {
 public:
  explicit MockProxyResolvingSocketFactory() : socket_raw_(nullptr) {}
  ~MockProxyResolvingSocketFactory() override {}

  // mojom::ProxyResolvingSocketFactory implementation.
  void CreateProxyResolvingSocket(
      const GURL& url,
      network::mojom::ProxyResolvingSocketOptionsPtr options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocket> receiver,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      CreateProxyResolvingSocketCallback callback) override {
    auto socket = std::make_unique<MockProxyResolvingSocket>();
    socket_raw_ = socket.get();
    proxy_resolving_socket_receivers_.Add(std::move(socket),
                                          std::move(receiver));
    socket_raw_->Connect(std::move(observer), std::move(callback));
  }

  MockProxyResolvingSocket* socket() { return socket_raw_; }

 private:
  mojo::UniqueReceiverSet<network::mojom::ProxyResolvingSocket>
      proxy_resolving_socket_receivers_;

  // Owned by |proxy_resolving_socket_receivers_|.
  MockProxyResolvingSocket* socket_raw_;

  DISALLOW_COPY_AND_ASSIGN(MockProxyResolvingSocketFactory);
};

void MockProxyResolvingSocket::RunEvents(std::vector<Event>&& events) {
  for (Event ev : events) {
    switch (ev) {
      case kRead:
        mojo::BlockingCopyFromString("data", receive_pipe_handle_);
        break;
      case kWrite: {
        std::string written;
        while (true) {
          char read_buffer[1024];
          uint32_t read_size = sizeof(read_buffer);
          MojoResult result = send_pipe_handle_->ReadData(
              read_buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
          if (result != MOJO_RESULT_OK)
            break;
          written.append(read_buffer, read_size);
        }

        EXPECT_EQ("atad", written);
        break;
      }
      case kCloseReadPipe:
        receive_pipe_handle_.reset();
        break;
      case kCloseWritePipe:
        send_pipe_handle_.reset();
        break;
      case kReadError:
        observer_->OnReadError(net::ERR_OUT_OF_MEMORY);
        observer_.FlushForTesting();
        break;
      case kReadErrorInvalid:
        observer_->OnReadError(42);
        observer_.FlushForTesting();
        break;
      case kWriteError:
        observer_->OnWriteError(net::ERR_ACCESS_DENIED);
        observer_.FlushForTesting();
        break;
      case kWriteErrorInvalid:
        observer_->OnWriteError(net::ERR_IO_PENDING);
        observer_.FlushForTesting();
        break;
      case kReadEofError:
        observer_->OnReadError(0);
        observer_.FlushForTesting();
        break;
      case kCloseObserverPipe:
        observer_.reset();
        break;
    }
    // Make sure the event is actually delivered.
    base::RunLoop().RunUntilIdle();
  }
}

class NetworkServiceAsyncSocketTest : public testing::Test,
                                      public sigslot::has_slots<> {
 protected:
  explicit NetworkServiceAsyncSocketTest(bool use_mojo_level_mock = false)
      : ssl_socket_data_provider_(net::ASYNC, net::OK),
        use_mojo_level_mock_(use_mojo_level_mock),
        mock_proxy_resolving_socket_factory_(nullptr),
        addr_({"localhost", 35}) {
    // GTest death tests by default execute in a fork()ed but not exec()ed
    // process. On macOS, a CoreFoundation-backed MessageLoop will exit with a
    // __THE_PROCESS_HAS_FORKED_AND_YOU_CANNOT_USE_THIS_COREFOUNDATION_FUNCTIONALITY___YOU_MUST_EXEC__
    // when called. Use the threadsafe mode to avoid this problem.
    testing::GTEST_FLAG(death_test_style) = "threadsafe";
  }

  ~NetworkServiceAsyncSocketTest() override {}

  void SetUp() override {
    mock_client_socket_factory_ =
        std::make_unique<net::MockClientSocketFactory>();
    mock_client_socket_factory_->set_enable_read_if_ready(true);
    mock_client_socket_factory_->AddSocketDataProvider(
        &async_socket_data_provider_);
    mock_client_socket_factory_->AddSSLSocketDataProvider(
        &ssl_socket_data_provider_);

    test_url_request_context_ =
        std::make_unique<net::TestURLRequestContext>(true /* delay init */);
    test_url_request_context_->set_client_socket_factory(
        mock_client_socket_factory_.get());
    test_url_request_context_->Init();

    if (use_mojo_level_mock_) {
      auto mock_proxy_resolving_socket_factory =
          std::make_unique<MockProxyResolvingSocketFactory>();
      mock_proxy_resolving_socket_factory_ =
          mock_proxy_resolving_socket_factory.get();
      proxy_resolving_socket_factory_ =
          std::move(mock_proxy_resolving_socket_factory);
    } else {
      mock_proxy_resolving_socket_factory_ = nullptr;
      proxy_resolving_socket_factory_ =
          std::make_unique<network::ProxyResolvingSocketFactoryMojo>(
              test_url_request_context_.get());
    }

    ns_async_socket_.reset(new NetworkServiceAsyncSocket(
        base::BindRepeating(
            &NetworkServiceAsyncSocketTest::BindToProxyResolvingSocketFactory,
            base::Unretained(this)),
        false, /* use_fake_tls_handshake */
        14, 20, TRAFFIC_ANNOTATION_FOR_TESTS));

    ns_async_socket_->SignalConnected.connect(
        this, &NetworkServiceAsyncSocketTest::OnConnect);
    ns_async_socket_->SignalSSLConnected.connect(
        this, &NetworkServiceAsyncSocketTest::OnSSLConnect);
    ns_async_socket_->SignalClosed.connect(
        this, &NetworkServiceAsyncSocketTest::OnClose);
    ns_async_socket_->SignalRead.connect(
        this, &NetworkServiceAsyncSocketTest::OnRead);
    ns_async_socket_->SignalError.connect(
        this, &NetworkServiceAsyncSocketTest::OnError);
  }

  void TearDown() override {
    // Run any tasks that we forgot to pump.
    base::RunLoop().RunUntilIdle();
    ExpectClosed();
    ExpectNoSignal();
    ns_async_socket_.reset();
  }

  void BindToProxyResolvingSocketFactory(
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
          receiver) {
    proxy_resolving_socket_factory_receiver_ = std::make_unique<
        mojo::Receiver<network::mojom::ProxyResolvingSocketFactory>>(
        proxy_resolving_socket_factory_.get());
    proxy_resolving_socket_factory_receiver_->Bind(std::move(receiver));
  }

  enum Signal {
    SIGNAL_CONNECT,
    SIGNAL_SSL_CONNECT,
    SIGNAL_CLOSE,
    SIGNAL_READ,
    SIGNAL_ERROR,
  };

  // Helper struct that records the state at the time of a signal.

  struct SignalSocketState {
    SignalSocketState()
        : signal(SIGNAL_ERROR),
          state(NetworkServiceAsyncSocket::STATE_CLOSED),
          error(NetworkServiceAsyncSocket::ERROR_NONE),
          net_error(net::OK) {}

    SignalSocketState(Signal signal,
                      NetworkServiceAsyncSocket::State state,
                      NetworkServiceAsyncSocket::Error error,
                      net::Error net_error)
        : signal(signal), state(state), error(error), net_error(net_error) {}

    bool IsEqual(const SignalSocketState& other) const {
      return (signal == other.signal) && (state == other.state) &&
             (error == other.error) && (net_error == other.net_error);
    }

    static SignalSocketState FromAsyncSocket(Signal signal,
                                             jingle_xmpp::AsyncSocket* async_socket) {
      return SignalSocketState(
          signal, async_socket->state(), async_socket->error(),
          static_cast<net::Error>(async_socket->GetError()));
    }

    static SignalSocketState NoError(Signal signal,
                                     jingle_xmpp::AsyncSocket::State state) {
      return SignalSocketState(signal, state, jingle_xmpp::AsyncSocket::ERROR_NONE,
                               net::OK);
    }

    std::string ToString() const {
      return base::StrCat({"(", base::NumberToString(signal), ",",
                           base::NumberToString(state), ",",
                           base::NumberToString(error), ",",
                           base::NumberToString(net_error), ")"});
    }

    Signal signal;
    NetworkServiceAsyncSocket::State state;
    NetworkServiceAsyncSocket::Error error;
    net::Error net_error;
  };

  void CaptureSocketState(Signal signal) {
    signal_socket_states_.push_back(
        SignalSocketState::FromAsyncSocket(signal, ns_async_socket_.get()));
  }

  void OnConnect() { CaptureSocketState(SIGNAL_CONNECT); }

  void OnSSLConnect() { CaptureSocketState(SIGNAL_SSL_CONNECT); }

  void OnClose() { CaptureSocketState(SIGNAL_CLOSE); }

  void OnRead() { CaptureSocketState(SIGNAL_READ); }

  void OnError() { ADD_FAILURE(); }

  // State expect functions.

  void ExpectState(NetworkServiceAsyncSocket::State state,
                   NetworkServiceAsyncSocket::Error error,
                   net::Error net_error) {
    EXPECT_EQ(state, ns_async_socket_->state());
    EXPECT_EQ(error, ns_async_socket_->error());
    EXPECT_EQ(net_error, ns_async_socket_->GetError());
  }

  void ExpectNonErrorState(NetworkServiceAsyncSocket::State state) {
    ExpectState(state, NetworkServiceAsyncSocket::ERROR_NONE, net::OK);
  }

  void ExpectErrorState(NetworkServiceAsyncSocket::State state,
                        NetworkServiceAsyncSocket::Error error) {
    ExpectState(state, error, net::OK);
  }

  void ExpectClosed() {
    ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_CLOSED);
  }

  // Signal expect functions.

  void ExpectNoSignal() {
    if (!signal_socket_states_.empty()) {
      ADD_FAILURE() << signal_socket_states_.front().signal;
    }
  }

  void ExpectSignalSocketState(SignalSocketState expected_signal_socket_state) {
    if (signal_socket_states_.empty()) {
      ADD_FAILURE() << expected_signal_socket_state.signal;
      return;
    }
    EXPECT_TRUE(
        expected_signal_socket_state.IsEqual(signal_socket_states_.front()))
        << "Expected signal:" << expected_signal_socket_state.ToString()
        << " actual signal: " << signal_socket_states_.front().ToString();
    signal_socket_states_.pop_front();
  }

  void ExpectReadSignal() {
    ExpectSignalSocketState(SignalSocketState::NoError(
        SIGNAL_READ, NetworkServiceAsyncSocket::STATE_OPEN));
  }

  void ExpectSSLConnectSignal() {
    ExpectSignalSocketState(SignalSocketState::NoError(
        SIGNAL_SSL_CONNECT, NetworkServiceAsyncSocket::STATE_TLS_OPEN));
  }

  void ExpectSSLReadSignal() {
    ExpectSignalSocketState(SignalSocketState::NoError(
        SIGNAL_READ, NetworkServiceAsyncSocket::STATE_TLS_OPEN));
  }

  // Open/close utility functions.

  void DoOpenClosed() {
    ExpectClosed();
    async_socket_data_provider_.set_connect_data(
        net::MockConnect(net::SYNCHRONOUS, net::OK));
    EXPECT_TRUE(ns_async_socket_->Connect(addr_));
    ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_CONNECTING);

    base::RunLoop().RunUntilIdle();
    // We may not necessarily be open; may have been other events
    // queued up.
    ExpectSignalSocketState(SignalSocketState::NoError(
        SIGNAL_CONNECT, NetworkServiceAsyncSocket::STATE_OPEN));
  }

  void DoCloseOpened(SignalSocketState expected_signal_socket_state) {
    // We may be in an error state, so just compare state().
    EXPECT_EQ(NetworkServiceAsyncSocket::STATE_OPEN, ns_async_socket_->state());
    EXPECT_TRUE(ns_async_socket_->Close());
    ExpectSignalSocketState(expected_signal_socket_state);
    ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_CLOSED);
  }

  void DoCloseOpenedNoError() {
    DoCloseOpened(SignalSocketState::NoError(
        SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED));
  }

  void DoSSLOpenClosed() {
    const char kDummyData[] = "dummy_data";
    async_socket_data_provider_.AddRead(net::MockRead(kDummyData));
    DoOpenClosed();
    ExpectReadSignal();
    EXPECT_EQ(kDummyData, DrainRead(1));

    EXPECT_TRUE(ns_async_socket_->StartTls("fakedomain.com"));
    base::RunLoop().RunUntilIdle();
    ExpectSSLConnectSignal();
    ExpectNoSignal();
    ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_TLS_OPEN);
  }

  void DoSSLCloseOpened(SignalSocketState expected_signal_socket_state) {
    // We may be in an error state, so just compare state().
    EXPECT_EQ(NetworkServiceAsyncSocket::STATE_TLS_OPEN,
              ns_async_socket_->state());
    EXPECT_TRUE(ns_async_socket_->Close());
    ExpectSignalSocketState(expected_signal_socket_state);
    ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_CLOSED);
  }

  void DoSSLCloseOpenedNoError() {
    DoSSLCloseOpened(SignalSocketState::NoError(
        SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED));
  }

  // Read utility functions.

  std::string DrainRead(size_t buf_size) {
    std::string read;
    std::unique_ptr<char[]> buf(new char[buf_size]);
    size_t len_read;
    while (true) {
      bool success = ns_async_socket_->Read(buf.get(), buf_size, &len_read);
      if (!success) {
        ADD_FAILURE();
        break;
      }
      if (len_read == 0U) {
        break;
      }
      read.append(buf.get(), len_read);
    }
    return read;
  }

  // Need a message loop for both the socket and Mojo.
  base::test::SingleThreadTaskEnvironment task_environment_;

  AsyncSocketDataProvider async_socket_data_provider_;
  net::SSLSocketDataProvider ssl_socket_data_provider_;

  bool use_mojo_level_mock_;

  std::unique_ptr<net::MockClientSocketFactory> mock_client_socket_factory_;
  std::unique_ptr<net::TestURLRequestContext> test_url_request_context_;
  // Either null or owned by proxy_resolving_socket_factory_.
  MockProxyResolvingSocketFactory* mock_proxy_resolving_socket_factory_;
  std::unique_ptr<network::mojom::ProxyResolvingSocketFactory>
      proxy_resolving_socket_factory_;
  std::unique_ptr<mojo::Receiver<network::mojom::ProxyResolvingSocketFactory>>
      proxy_resolving_socket_factory_receiver_;

  std::unique_ptr<NetworkServiceAsyncSocket> ns_async_socket_;
  base::circular_deque<SignalSocketState> signal_socket_states_;
  const net::HostPortPair addr_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkServiceAsyncSocketTest);
};

TEST_F(NetworkServiceAsyncSocketTest, InitialState) {
  ExpectClosed();
  ExpectNoSignal();
}

TEST_F(NetworkServiceAsyncSocketTest, EmptyClose) {
  ExpectClosed();
  EXPECT_TRUE(ns_async_socket_->Close());
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketTest, ImmediateConnectAndClose) {
  DoOpenClosed();

  ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_OPEN);

  DoCloseOpenedNoError();
}

// After this, no need to test immediate successful connect and
// Close() so thoroughly.

TEST_F(NetworkServiceAsyncSocketTest, DoubleClose) {
  DoOpenClosed();

  EXPECT_TRUE(ns_async_socket_->Close());
  ExpectClosed();
  ExpectSignalSocketState(SignalSocketState::NoError(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED));

  EXPECT_TRUE(ns_async_socket_->Close());
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketTest, ZeroPortConnect) {
  const net::HostPortPair zero_port_addr({addr_.host(), 0});
  EXPECT_FALSE(ns_async_socket_->Connect(zero_port_addr));
  ExpectErrorState(NetworkServiceAsyncSocket::STATE_CLOSED,
                   NetworkServiceAsyncSocket::ERROR_DNS);

  EXPECT_TRUE(ns_async_socket_->Close());
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketTest, DoubleConnect) {
  EXPECT_DEBUG_DEATH(
      {
        DoOpenClosed();

        EXPECT_FALSE(ns_async_socket_->Connect(addr_));
        ExpectErrorState(NetworkServiceAsyncSocket::STATE_OPEN,
                         NetworkServiceAsyncSocket::ERROR_WRONGSTATE);

        DoCloseOpened(SignalSocketState(
            SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
            NetworkServiceAsyncSocket::ERROR_WRONGSTATE, net::OK));
      },
      "non-closed socket");
}

TEST_F(NetworkServiceAsyncSocketTest, ImmediateConnectCloseBeforeRead) {
  DoOpenClosed();

  EXPECT_TRUE(ns_async_socket_->Close());
  ExpectClosed();
  ExpectSignalSocketState(SignalSocketState::NoError(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED));

  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkServiceAsyncSocketTest, HangingConnect) {
  EXPECT_TRUE(ns_async_socket_->Connect(addr_));
  ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_CONNECTING);
  ExpectNoSignal();

  EXPECT_TRUE(ns_async_socket_->Close());
  ExpectClosed();
  ExpectSignalSocketState(SignalSocketState::NoError(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED));
}

TEST_F(NetworkServiceAsyncSocketTest, PendingConnect) {
  async_socket_data_provider_.set_connect_data(
      net::MockConnect(net::ASYNC, net::OK));
  EXPECT_TRUE(ns_async_socket_->Connect(addr_));
  ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_CONNECTING);
  ExpectNoSignal();

  base::RunLoop().RunUntilIdle();
  ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_OPEN);
  ExpectSignalSocketState(SignalSocketState::NoError(
      SIGNAL_CONNECT, NetworkServiceAsyncSocket::STATE_OPEN));
  ExpectNoSignal();

  base::RunLoop().RunUntilIdle();

  DoCloseOpenedNoError();
}

// After this no need to test successful pending connect so
// thoroughly.

TEST_F(NetworkServiceAsyncSocketTest, PendingConnectCloseBeforeRead) {
  async_socket_data_provider_.set_connect_data(
      net::MockConnect(net::ASYNC, net::OK));
  EXPECT_TRUE(ns_async_socket_->Connect(addr_));

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState::NoError(
      SIGNAL_CONNECT, NetworkServiceAsyncSocket::STATE_OPEN));

  DoCloseOpenedNoError();

  base::RunLoop().RunUntilIdle();
}

TEST_F(NetworkServiceAsyncSocketTest, PendingConnectError) {
  async_socket_data_provider_.set_connect_data(
      net::MockConnect(net::ASYNC, net::ERR_TIMED_OUT));
  EXPECT_TRUE(ns_async_socket_->Connect(addr_));

  base::RunLoop().RunUntilIdle();

  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_TIMED_OUT));
}

// After this we can assume Connect() and Close() work as expected.

TEST_F(NetworkServiceAsyncSocketTest, EmptyRead) {
  DoOpenClosed();

  char buf[4096];
  size_t len_read = 10000U;
  EXPECT_TRUE(ns_async_socket_->Read(buf, sizeof(buf), &len_read));
  EXPECT_EQ(0U, len_read);

  DoCloseOpenedNoError();
}

TEST_F(NetworkServiceAsyncSocketTest, WrongRead) {
  EXPECT_DEBUG_DEATH(
      {
        async_socket_data_provider_.set_connect_data(
            net::MockConnect(net::ASYNC, net::OK));
        EXPECT_TRUE(ns_async_socket_->Connect(addr_));
        ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_CONNECTING);
        ExpectNoSignal();

        char buf[4096];
        size_t len_read;
        EXPECT_FALSE(ns_async_socket_->Read(buf, sizeof(buf), &len_read));
        ExpectErrorState(NetworkServiceAsyncSocket::STATE_CONNECTING,
                         NetworkServiceAsyncSocket::ERROR_WRONGSTATE);
        EXPECT_TRUE(ns_async_socket_->Close());
        ExpectSignalSocketState(SignalSocketState(
            SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
            NetworkServiceAsyncSocket::ERROR_WRONGSTATE, net::OK));
      },
      "non-open");
}

TEST_F(NetworkServiceAsyncSocketTest, WrongReadClosed) {
  char buf[4096];
  size_t len_read;
  EXPECT_FALSE(ns_async_socket_->Read(buf, sizeof(buf), &len_read));
  ExpectErrorState(NetworkServiceAsyncSocket::STATE_CLOSED,
                   NetworkServiceAsyncSocket::ERROR_WRONGSTATE);
  EXPECT_TRUE(ns_async_socket_->Close());
}

const char kReadData[] = "mydatatoread";

TEST_F(NetworkServiceAsyncSocketTest, Read) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();

  ExpectReadSignal();
  ExpectNoSignal();

  EXPECT_EQ(kReadData, DrainRead(1));

  base::RunLoop().RunUntilIdle();

  DoCloseOpenedNoError();
}

TEST_F(NetworkServiceAsyncSocketTest, ReadTwice) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();

  ExpectReadSignal();
  ExpectNoSignal();

  EXPECT_EQ(kReadData, DrainRead(1));

  base::RunLoop().RunUntilIdle();

  const char kReadData2[] = "mydatatoread2";
  async_socket_data_provider_.AddRead(net::MockRead(kReadData2));
  base::RunLoop().RunUntilIdle();

  ExpectReadSignal();
  ExpectNoSignal();

  EXPECT_EQ(kReadData2, DrainRead(1));

  DoCloseOpenedNoError();
}

TEST_F(NetworkServiceAsyncSocketTest, ReadError) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();

  ExpectReadSignal();
  ExpectNoSignal();

  EXPECT_EQ(kReadData, DrainRead(1));

  base::RunLoop().RunUntilIdle();

  async_socket_data_provider_.AddRead(
      net::MockRead(net::SYNCHRONOUS, net::ERR_TIMED_OUT));
  base::RunLoop().RunUntilIdle();

  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_TIMED_OUT));
}

TEST_F(NetworkServiceAsyncSocketTest, ReadEOF) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();

  ExpectReadSignal();
  ExpectNoSignal();

  EXPECT_EQ(kReadData, DrainRead(1));

  base::RunLoop().RunUntilIdle();

  async_socket_data_provider_.AddRead(net::MockRead(net::SYNCHRONOUS, net::OK));
  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState::NoError(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED));
}

TEST_F(NetworkServiceAsyncSocketTest, ReadEmpty) {
  async_socket_data_provider_.AddRead(net::MockRead(""));
  DoOpenClosed();

  ExpectSignalSocketState(SignalSocketState::NoError(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED));
}

TEST_F(NetworkServiceAsyncSocketTest, PendingRead) {
  DoOpenClosed();

  ExpectNoSignal();

  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  base::RunLoop().RunUntilIdle();

  ExpectSignalSocketState(SignalSocketState::NoError(
      SIGNAL_READ, NetworkServiceAsyncSocket::STATE_OPEN));
  ExpectNoSignal();

  EXPECT_EQ(kReadData, DrainRead(1));

  base::RunLoop().RunUntilIdle();

  DoCloseOpenedNoError();
}

TEST_F(NetworkServiceAsyncSocketTest, PendingEmptyRead) {
  DoOpenClosed();

  ExpectNoSignal();

  async_socket_data_provider_.AddRead(net::MockRead(""));
  base::RunLoop().RunUntilIdle();

  ExpectSignalSocketState(SignalSocketState::NoError(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED));
}

TEST_F(NetworkServiceAsyncSocketTest, PendingReadError) {
  DoOpenClosed();

  ExpectNoSignal();

  async_socket_data_provider_.AddRead(
      net::MockRead(net::ASYNC, net::ERR_TIMED_OUT));
  base::RunLoop().RunUntilIdle();

  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_TIMED_OUT));
}

// After this we can assume non-SSL Read() works as expected.

TEST_F(NetworkServiceAsyncSocketTest, WrongWrite) {
  EXPECT_DEBUG_DEATH(
      {
        std::string data("foo");
        EXPECT_FALSE(ns_async_socket_->Write(data.data(), data.size()));
        ExpectErrorState(NetworkServiceAsyncSocket::STATE_CLOSED,
                         NetworkServiceAsyncSocket::ERROR_WRONGSTATE);
        EXPECT_TRUE(ns_async_socket_->Close());
      },
      "non-open");
}

const char kWriteData[] = "mydatatowrite";

TEST_F(NetworkServiceAsyncSocketTest, SyncWrite) {
  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::SYNCHRONOUS, kWriteData, 3));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::SYNCHRONOUS, kWriteData + 3, 5));
  async_socket_data_provider_.AddWrite(net::MockWrite(
      net::SYNCHRONOUS, kWriteData + 8, base::size(kWriteData) - 8));
  DoOpenClosed();

  EXPECT_TRUE(ns_async_socket_->Write(kWriteData, 3));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ns_async_socket_->Write(kWriteData + 3, 5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      ns_async_socket_->Write(kWriteData + 8, base::size(kWriteData) - 8));
  base::RunLoop().RunUntilIdle();

  ExpectNoSignal();

  DoCloseOpenedNoError();
}

TEST_F(NetworkServiceAsyncSocketTest, AsyncWrite) {
  DoOpenClosed();

  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::ASYNC, kWriteData, 3));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::ASYNC, kWriteData + 3, 5));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::ASYNC, kWriteData + 8, base::size(kWriteData) - 8));

  EXPECT_TRUE(ns_async_socket_->Write(kWriteData, 3));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ns_async_socket_->Write(kWriteData + 3, 5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      ns_async_socket_->Write(kWriteData + 8, base::size(kWriteData) - 8));
  base::RunLoop().RunUntilIdle();

  ExpectNoSignal();

  DoCloseOpenedNoError();
}

TEST_F(NetworkServiceAsyncSocketTest, AsyncWriteError) {
  DoOpenClosed();

  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::ASYNC, kWriteData, 3));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::ASYNC, kWriteData + 3, 5));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::ASYNC, net::ERR_TIMED_OUT));

  EXPECT_TRUE(ns_async_socket_->Write(kWriteData, 3));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ns_async_socket_->Write(kWriteData + 3, 5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      ns_async_socket_->Write(kWriteData + 8, base::size(kWriteData) - 8));
  base::RunLoop().RunUntilIdle();

  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_TIMED_OUT));
}

TEST_F(NetworkServiceAsyncSocketTest, LargeWrite) {
  EXPECT_DEBUG_DEATH(
      {
        DoOpenClosed();

        std::string large_data(100, 'x');
        EXPECT_FALSE(
            ns_async_socket_->Write(large_data.data(), large_data.size()));
        ExpectState(NetworkServiceAsyncSocket::STATE_OPEN,
                    NetworkServiceAsyncSocket::ERROR_WINSOCK,
                    net::ERR_INSUFFICIENT_RESOURCES);
        DoCloseOpened(SignalSocketState(
            SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
            NetworkServiceAsyncSocket::ERROR_WINSOCK,
            net::ERR_INSUFFICIENT_RESOURCES));
      },
      "exceed the max write buffer");
}

TEST_F(NetworkServiceAsyncSocketTest, LargeAccumulatedWrite) {
  EXPECT_DEBUG_DEATH(
      {
        DoOpenClosed();

        std::string data(15, 'x');
        EXPECT_TRUE(ns_async_socket_->Write(data.data(), data.size()));
        EXPECT_FALSE(ns_async_socket_->Write(data.data(), data.size()));
        ExpectState(NetworkServiceAsyncSocket::STATE_OPEN,
                    NetworkServiceAsyncSocket::ERROR_WINSOCK,
                    net::ERR_INSUFFICIENT_RESOURCES);
        DoCloseOpened(SignalSocketState(
            SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
            NetworkServiceAsyncSocket::ERROR_WINSOCK,
            net::ERR_INSUFFICIENT_RESOURCES));
      },
      "exceed the max write buffer");
}

// After this we can assume non-SSL I/O works as expected.

TEST_F(NetworkServiceAsyncSocketTest, HangingSSLConnect) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();
  ExpectReadSignal();

  EXPECT_TRUE(ns_async_socket_->StartTls("fakedomain.com"));
  ExpectNoSignal();

  ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_TLS_CONNECTING);
  EXPECT_TRUE(ns_async_socket_->Close());
  ExpectSignalSocketState(SignalSocketState::NoError(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED));
  ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_CLOSED);
}

TEST_F(NetworkServiceAsyncSocketTest, ImmediateSSLConnect) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();
  ExpectReadSignal();

  EXPECT_TRUE(ns_async_socket_->StartTls("fakedomain.com"));
  base::RunLoop().RunUntilIdle();
  ExpectSSLConnectSignal();
  ExpectNoSignal();
  ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_TLS_OPEN);

  DoSSLCloseOpenedNoError();
}

TEST_F(NetworkServiceAsyncSocketTest, DoubleSSLConnect) {
  EXPECT_DEBUG_DEATH(
      {
        async_socket_data_provider_.AddRead(net::MockRead(kReadData));
        DoOpenClosed();
        ExpectReadSignal();

        EXPECT_TRUE(ns_async_socket_->StartTls("fakedomain.com"));
        base::RunLoop().RunUntilIdle();
        ExpectSSLConnectSignal();
        ExpectNoSignal();
        ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_TLS_OPEN);

        EXPECT_FALSE(ns_async_socket_->StartTls("fakedomain.com"));

        DoSSLCloseOpened(SignalSocketState(
            SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
            NetworkServiceAsyncSocket::ERROR_WRONGSTATE, net::OK));
      },
      "wrong state");
}

TEST_F(NetworkServiceAsyncSocketTest, FailedSSLConnect) {
  ssl_socket_data_provider_.connect =
      net::MockConnect(net::ASYNC, net::ERR_CERT_COMMON_NAME_INVALID),

  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();
  ExpectReadSignal();

  EXPECT_TRUE(ns_async_socket_->StartTls("fakedomain.com"));
  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(
      SignalSocketState(SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
                        NetworkServiceAsyncSocket::ERROR_WINSOCK,
                        net::ERR_CERT_COMMON_NAME_INVALID));

  EXPECT_TRUE(ns_async_socket_->Close());
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketTest, ReadDuringSSLConnecting) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();
  ExpectReadSignal();
  EXPECT_EQ(kReadData, DrainRead(1));

  EXPECT_TRUE(ns_async_socket_->StartTls("fakedomain.com"));
  ExpectNoSignal();

  // Shouldn't do anything.
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));

  char buf[4096];
  size_t len_read = 10000U;
  EXPECT_TRUE(ns_async_socket_->Read(buf, sizeof(buf), &len_read));
  EXPECT_EQ(0U, len_read);

  base::RunLoop().RunUntilIdle();
  ExpectSSLConnectSignal();
  ExpectSSLReadSignal();
  ExpectNoSignal();
  ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_TLS_OPEN);

  len_read = 10000U;
  EXPECT_TRUE(ns_async_socket_->Read(buf, sizeof(buf), &len_read));
  EXPECT_EQ(kReadData, std::string(buf, len_read));

  DoSSLCloseOpenedNoError();
}

TEST_F(NetworkServiceAsyncSocketTest, WriteDuringSSLConnecting) {
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  DoOpenClosed();
  ExpectReadSignal();

  EXPECT_TRUE(ns_async_socket_->StartTls("fakedomain.com"));
  ExpectNoSignal();
  ExpectNonErrorState(NetworkServiceAsyncSocket::STATE_TLS_CONNECTING);

  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::ASYNC, kWriteData, 3));

  // Shouldn't do anything.
  EXPECT_TRUE(ns_async_socket_->Write(kWriteData, 3));

  // TODO(akalin): Figure out how to test that the write happens
  // *after* the SSL connect.

  base::RunLoop().RunUntilIdle();
  ExpectSSLConnectSignal();
  ExpectNoSignal();

  base::RunLoop().RunUntilIdle();

  DoSSLCloseOpenedNoError();
}

// After this we can assume SSL connect works as expected.

TEST_F(NetworkServiceAsyncSocketTest, SSLRead) {
  DoSSLOpenClosed();
  async_socket_data_provider_.AddRead(net::MockRead(kReadData));
  base::RunLoop().RunUntilIdle();

  ExpectSSLReadSignal();
  ExpectNoSignal();

  EXPECT_EQ(kReadData, DrainRead(1));

  base::RunLoop().RunUntilIdle();

  DoSSLCloseOpenedNoError();
}

TEST_F(NetworkServiceAsyncSocketTest, SSLSyncWrite) {
  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::SYNCHRONOUS, kWriteData, 3));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::SYNCHRONOUS, kWriteData + 3, 5));
  async_socket_data_provider_.AddWrite(net::MockWrite(
      net::SYNCHRONOUS, kWriteData + 8, base::size(kWriteData) - 8));
  DoSSLOpenClosed();

  EXPECT_TRUE(ns_async_socket_->Write(kWriteData, 3));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ns_async_socket_->Write(kWriteData + 3, 5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      ns_async_socket_->Write(kWriteData + 8, base::size(kWriteData) - 8));
  base::RunLoop().RunUntilIdle();

  ExpectNoSignal();

  DoSSLCloseOpenedNoError();
}

TEST_F(NetworkServiceAsyncSocketTest, SSLAsyncWrite) {
  DoSSLOpenClosed();

  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::ASYNC, kWriteData, 3));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::ASYNC, kWriteData + 3, 5));
  async_socket_data_provider_.AddWrite(
      net::MockWrite(net::ASYNC, kWriteData + 8, base::size(kWriteData) - 8));

  EXPECT_TRUE(ns_async_socket_->Write(kWriteData, 3));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ns_async_socket_->Write(kWriteData + 3, 5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      ns_async_socket_->Write(kWriteData + 8, base::size(kWriteData) - 8));
  base::RunLoop().RunUntilIdle();

  ExpectNoSignal();

  DoSSLCloseOpenedNoError();
}

class NetworkServiceAsyncSocketMojoTest : public NetworkServiceAsyncSocketTest {
 protected:
  NetworkServiceAsyncSocketMojoTest()
      : NetworkServiceAsyncSocketTest(true /* use_mojo_level_mock */) {}
  ~NetworkServiceAsyncSocketMojoTest() override {}
};

TEST_F(NetworkServiceAsyncSocketMojoTest, ReadEOF1) {
  DoOpenClosed();
  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kRead,
       MockProxyResolvingSocket::kCloseReadPipe,
       MockProxyResolvingSocket::kReadEofError,
       MockProxyResolvingSocket::kCloseObserverPipe});

  ExpectReadSignal();
  ExpectNoSignal();
  EXPECT_EQ("data", DrainRead(1));

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState::NoError(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED));
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketMojoTest, ReadEOF2) {
  DoOpenClosed();
  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kReadEofError,
       MockProxyResolvingSocket::kCloseObserverPipe,
       MockProxyResolvingSocket::kRead,
       MockProxyResolvingSocket::kCloseReadPipe});

  ExpectReadSignal();
  ExpectNoSignal();
  EXPECT_EQ("data", DrainRead(1));

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState::NoError(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED));
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketMojoTest, ReadError1) {
  DoOpenClosed();
  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kRead,
       MockProxyResolvingSocket::kCloseReadPipe,
       MockProxyResolvingSocket::kReadError,
       MockProxyResolvingSocket::kCloseObserverPipe});

  ExpectReadSignal();
  ExpectNoSignal();
  EXPECT_EQ("data", DrainRead(1));

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_OUT_OF_MEMORY));
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketMojoTest, ReadError2) {
  DoOpenClosed();
  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kReadError,
       MockProxyResolvingSocket::kCloseObserverPipe,
       MockProxyResolvingSocket::kRead,
       MockProxyResolvingSocket::kCloseReadPipe});

  ExpectReadSignal();
  ExpectNoSignal();
  EXPECT_EQ("data", DrainRead(1));

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_OUT_OF_MEMORY));
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketMojoTest, ReadErrorDouble) {
  DoOpenClosed();
  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kReadError,
       MockProxyResolvingSocket::kReadError,
       MockProxyResolvingSocket::kCloseObserverPipe,
       MockProxyResolvingSocket::kRead,
       MockProxyResolvingSocket::kCloseReadPipe});

  ExpectReadSignal();
  ExpectNoSignal();
  EXPECT_EQ("data", DrainRead(1));

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_OUT_OF_MEMORY));
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketMojoTest, ReadErrorDoubleInvalid) {
  DoOpenClosed();
  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kReadError,
       MockProxyResolvingSocket::kReadErrorInvalid,
       MockProxyResolvingSocket::kCloseObserverPipe,
       MockProxyResolvingSocket::kRead,
       MockProxyResolvingSocket::kCloseReadPipe});

  ExpectReadSignal();
  ExpectNoSignal();
  EXPECT_EQ("data", DrainRead(1));

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_OUT_OF_MEMORY));
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketMojoTest, ReadErrorDoubleInvalid2) {
  DoOpenClosed();
  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kReadErrorInvalid,
       MockProxyResolvingSocket::kReadError,
       MockProxyResolvingSocket::kCloseObserverPipe,
       MockProxyResolvingSocket::kRead,
       MockProxyResolvingSocket::kCloseReadPipe});

  ExpectReadSignal();
  ExpectNoSignal();
  EXPECT_EQ("data", DrainRead(1));

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_FAILED));
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketMojoTest, ReadErrorClosedObserverPipe) {
  DoOpenClosed();
  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kRead,
       MockProxyResolvingSocket::kCloseObserverPipe});
  // Can't run kCloseReadPipe since it'll already be closed.

  ExpectReadSignal();
  // Since this is a misbehaving network service process scenario, no attempt
  // to recover the data is made.
  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_FAILED));
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketMojoTest, WriteError1) {
  DoOpenClosed();
  ExpectNoSignal();
  ns_async_socket_->Write("atad", 4);
  base::RunLoop().RunUntilIdle();

  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kWrite,
       MockProxyResolvingSocket::kCloseWritePipe,
       MockProxyResolvingSocket::kWriteError});
  // Cannot close the observer pipe here at the end since the other size
  // would have closed it already.

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_ACCESS_DENIED));
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketMojoTest, WriteError2) {
  DoOpenClosed();
  ExpectNoSignal();
  ns_async_socket_->Write("atad", 4);
  base::RunLoop().RunUntilIdle();

  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kWriteError,
       MockProxyResolvingSocket::kCloseObserverPipe,
       MockProxyResolvingSocket::kWrite,
       MockProxyResolvingSocket::kCloseWritePipe});

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_ACCESS_DENIED));
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketMojoTest, WriteErrorDouble) {
  DoOpenClosed();
  ExpectNoSignal();
  ns_async_socket_->Write("atad", 4);
  base::RunLoop().RunUntilIdle();

  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kWriteError,
       MockProxyResolvingSocket::kWriteError,
       MockProxyResolvingSocket::kCloseObserverPipe,
       MockProxyResolvingSocket::kWrite,
       MockProxyResolvingSocket::kCloseWritePipe});

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_ACCESS_DENIED));
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketMojoTest, WriteErrorDoubleInvalid) {
  DoOpenClosed();
  ExpectNoSignal();
  ns_async_socket_->Write("atad", 4);
  base::RunLoop().RunUntilIdle();

  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kWriteError,
       MockProxyResolvingSocket::kWriteErrorInvalid,
       MockProxyResolvingSocket::kCloseObserverPipe,
       MockProxyResolvingSocket::kWrite,
       MockProxyResolvingSocket::kCloseWritePipe});

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_ACCESS_DENIED));
  ExpectClosed();
}

TEST_F(NetworkServiceAsyncSocketMojoTest, WriteErrorDoubleInvalid2) {
  DoOpenClosed();
  ExpectNoSignal();
  ns_async_socket_->Write("atad", 4);
  base::RunLoop().RunUntilIdle();

  mock_proxy_resolving_socket_factory_->socket()->RunEvents(
      {MockProxyResolvingSocket::kWriteErrorInvalid,
       MockProxyResolvingSocket::kWriteError,
       MockProxyResolvingSocket::kCloseObserverPipe,
       MockProxyResolvingSocket::kWrite,
       MockProxyResolvingSocket::kCloseWritePipe});

  base::RunLoop().RunUntilIdle();
  ExpectSignalSocketState(SignalSocketState(
      SIGNAL_CLOSE, NetworkServiceAsyncSocket::STATE_CLOSED,
      NetworkServiceAsyncSocket::ERROR_WINSOCK, net::ERR_FAILED));
  ExpectClosed();
}

}  // namespace

}  // namespace jingle_glue
