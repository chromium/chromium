// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/websocket_channel_impl.h"

#include <stdint.h>
#include <string.h>

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/storage_access_api/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_client.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::PrintToString;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::Unused;

namespace blink {

typedef StrictMock<testing::MockFunction<void(int)>> Checkpoint;

class MockWebSocketChannelClient
    : public GarbageCollected<MockWebSocketChannelClient>,
      public WebSocketChannelClient {
 public:
  static MockWebSocketChannelClient* Create() {
    return MakeGarbageCollected<StrictMock<MockWebSocketChannelClient>>();
  }

  MockWebSocketChannelClient() = default;

  ~MockWebSocketChannelClient() override = default;

  MOCK_METHOD2(DidConnect, void(const String&, const String&));
  MOCK_METHOD1(DidReceiveTextMessage, void(const String&));
  void DidReceiveBinaryMessage(
      const Vector<base::span<const char>>& data) override {
    Vector<char> flatten;
    for (const auto& span : data) {
      flatten.AppendSpan(span);
    }
    DidReceiveBinaryMessageMock(flatten);
  }
  MOCK_METHOD1(DidReceiveBinaryMessageMock, void(const Vector<char>&));
  MOCK_METHOD0(DidError, void());
  MOCK_METHOD1(DidConsumeBufferedAmount, void(uint64_t));
  MOCK_METHOD0(DidStartClosingHandshake, void());
  MOCK_METHOD3(DidClose,
               void(ClosingHandshakeCompletionStatus, uint16_t, const String&));

  void Trace(Visitor* visitor) const override {
    WebSocketChannelClient::Trace(visitor);
  }
};

class MockWebSocketHandshakeThrottle : public WebSocketHandshakeThrottle {
 public:
  MockWebSocketHandshakeThrottle() = default;
  ~MockWebSocketHandshakeThrottle() override { Destructor(); }

  MOCK_METHOD4(ThrottleHandshake,
               void(const WebURL&,
                    const WebSecurityOrigin&,
                    const WebSecurityOrigin&,
                    WebSocketHandshakeThrottle::OnCompletion));

  // This method is used to allow us to require that the destructor is called at
  // a particular time.
  MOCK_METHOD0(Destructor, void());
};

// The base class sets up the page.
class WebSocketChannelImplTestBase : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();

    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::WebSocketConnector::Name_,
        WTF::BindRepeating(
            &WebSocketChannelImplTestBase::BindWebSocketConnector,
            GetWeakPtr()));

    const KURL page_url("http://example.com/");
    NavigateTo(page_url);
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::WebSocketConnector::Name_, {});

    PageTestBase::TearDown();
  }

  // These need to be implemented in the subclass.
  virtual base::WeakPtr<WebSocketChannelImplTestBase> GetWeakPtr() = 0;
  virtual void BindWebSocketConnector(mojo::ScopedMessagePipeHandle handle) = 0;

 private:
  Persistent<EmptyLocalFrameClient> local_frame_client_;
};

class WebSocketChannelImplTest : public WebSocketChannelImplTestBase {
 public:
  using WebSocketMessageType = network::mojom::WebSocketMessageType;
  class TestWebSocket final : public network::mojom::blink::WebSocket {
   public:
    struct DataFrame final {
      DataFrame(WebSocketMessageType type, uint64_t data_length)
          : type(type), data_length(data_length) {}
      WebSocketMessageType type;
      uint64_t data_length;

      bool operator==(const DataFrame& that) const {
        return std::tie(type, data_length) ==
               std::tie(that.type, that.data_length);
      }
    };

    explicit TestWebSocket(
        mojo::PendingReceiver<network::mojom::blink::WebSocket>
            pending_receiver)
        : receiver_(this, std::move(pending_receiver)) {}

    void SendMessage(WebSocketMessageType type, uint64_t data_length) override {
      pending_send_data_frames_.push_back(DataFrame(type, data_length));
      return;
    }
    void StartReceiving() override {
      DCHECK(!is_start_receiving_called_);
      is_start_receiving_called_ = true;
    }
    void StartClosingHandshake(uint16_t code, const String& reason) override {
      DCHECK(!is_start_closing_handshake_called_);
      is_start_closing_handshake_called_ = true;
      closing_code_ = code;
      closing_reason_ = reason;
    }

    const Vector<DataFrame>& GetDataFrames() const {
      return pending_send_data_frames_;
    }
    void ClearDataFrames() { pending_send_data_frames_.clear(); }
    bool IsStartReceivingCalled() const { return is_start_receiving_called_; }
    bool IsStartClosingHandshakeCalled() const {
      return is_start_closing_handshake_called_;
    }
    uint16_t GetClosingCode() const { return closing_code_; }
    const String& GetClosingReason() const { return closing_reason_; }

   private:
    Vector<DataFrame> pending_send_data_frames_;
    bool is_start_receiving_called_ = false;
    bool is_start_closing_handshake_called_ = false;
    uint16_t closing_code_ = 0;
    String closing_reason_;

    mojo::Receiver<network::mojom::blink::WebSocket> receiver_;
  };
  using DataFrames = Vector<TestWebSocket::DataFrame>;

  class WebSocketConnector final : public mojom::blink::WebSocketConnector {
   public:
    struct ConnectArgs {
      ConnectArgs(
          const KURL& url,
          const Vector<String>& protocols,
          const net::SiteForCookies& site_for_cookies,
          const String& user_agent,
          mojo::PendingRemote<network::mojom::blink::WebSocketHandshakeClient>
              handshake_client)
          : url(url),
            protocols(protocols),
            site_for_cookies(site_for_cookies),
            user_agent(user_agent),
            handshake_client(std::move(handshake_client)) {}

      KURL url;
      Vector<String> protocols;
      net::SiteForCookies site_for_cookies;
      String user_agent;
      mojo::PendingRemote<network::mojom::blink::WebSocketHandshakeClient>
          handshake_client;
    };

    void Connect(
        const KURL& url,
        const Vector<String>& requested_protocols,
        const net::SiteForCookies& site_for_cookies,
        const String& user_agent,
        net::StorageAccessApiStatus storage_access_api_status,
        mojo::PendingRemote<network::mojom::blink::WebSocketHandshakeClient>
            handshake_client,
        const std::optional<base::UnguessableToken>& throttling_profile_id)
        override {
      connect_args_.push_back(ConnectArgs(url, requested_protocols,
                                          site_for_cookies, user_agent,
                                          std::move(handshake_client)));
    }

    const Vector<ConnectArgs>& GetConnectArgs() const { return connect_args_; }
    Vector<ConnectArgs> TakeConnectArgs() { return std::move(connect_args_); }

    void Bind(
        mojo::PendingReceiver<mojom::blink::WebSocketConnector> receiver) {
      receiver_set_.Add(this, std::move(receiver));
    }

   private:
    mojo::ReceiverSet<mojom::blink::WebSocketConnector> receiver_set_;
    Vector<ConnectArgs> connect_args_;
  };

  explicit WebSocketChannelImplTest(
      std::unique_ptr<MockWebSocketHandshakeThrottle> handshake_throttle =
          nullptr)
      : channel_client_(MockWebSocketChannelClient::Create()),
        handshake_throttle_(std::move(handshake_throttle)),
        raw_handshake_throttle_(handshake_throttle_.get()),
        sum_of_consumed_buffered_amount_(0),
        weak_ptr_factory_(this) {
    ON_CALL(*ChannelClient(), DidConsumeBufferedAmount(_))
        .WillByDefault(
            Invoke(this, &WebSocketChannelImplTest::DidConsumeBufferedAmount));
  }

  ~WebSocketChannelImplTest() override { Channel()->Disconnect(); }

  base::WeakPtr<WebSocketChannelImplTestBase> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void BindWebSocketConnector(mojo::ScopedMessagePipeHandle handle) override {
    connector_.Bind(mojo::PendingReceiver<mojom::blink::WebSocketConnector>(
        std::move(handle)));
  }

  MojoResult CreateDataPipe(uint32_t capacity,
                            mojo::ScopedDataPipeProducerHandle* writable,
                            mojo::ScopedDataPipeConsumerHandle* readable) {
    const MojoCreateDataPipeOptions data_pipe_options{
        sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
        capacity};
    return mojo::CreateDataPipe(&data_pipe_options, *writable, *readable);
  }

  std::unique_ptr<TestWebSocket> EstablishConnection(
      network::mojom::blink::WebSocketHandshakeClient* handshake_client,
      const String& selected_protocol,
      const String& extensions,
      mojo::ScopedDataPipeConsumerHandle readable,
      mojo::ScopedDataPipeProducerHandle writable,
      mojo::Remote<network::mojom::blink::WebSocketClient>* client) {
    mojo::PendingRemote<network::mojom::blink::WebSocketClient> client_remote;
    mojo::PendingRemote<network::mojom::blink::WebSocket> websocket_to_pass;
    auto websocket = std::make_unique<TestWebSocket>(
        websocket_to_pass.InitWithNewPipeAndPassReceiver());

    auto response = network::mojom::blink::WebSocketHandshakeResponse::New();
    response->http_version = network::mojom::blink::HttpVersion::New();
    response->status_text = "";
    response->headers_text = "";
    response->selected_protocol = selected_protocol;
    response->extensions = extensions;
    handshake_client->OnConnectionEstablished(
        std::move(websocket_to_pass),
        client_remote.InitWithNewPipeAndPassReceiver(), std::move(response),
        std::move(readable), std::move(writable));
    client->Bind(std::move(client_remote));
    return websocket;
  }

  void SetUp() override {
    WebSocketChannelImplTestBase::SetUp();
    channel_ = WebSocketChannelImpl::CreateForTesting(
        GetFrame().DomWindow(), channel_client_.Get(), CaptureSourceLocation(),
        std::move(handshake_throttle_));
  }

  MockWebSocketChannelClient* ChannelClient() { return channel_client_.Get(); }

  WebSocketChannelImpl* Channel() { return channel_.Get(); }

  void DidConsumeBufferedAmount(uint64_t a) {
    sum_of_consumed_buffered_amount_ += a;
  }

  static Vector<uint8_t> AsVector(const char* data, size_t size) {
    Vector<uint8_t> v;
    v.Append(reinterpret_cast<const uint8_t*>(data),
             static_cast<wtf_size_t>(size));
    return v;
  }
  static Vector<uint8_t> AsVector(const char* data) {
    return AsVector(data, strlen(data));
  }

  Vector<uint8_t> ReadDataFromDataPipe(
      mojo::ScopedDataPipeConsumerHandle& readable,
      size_t bytes_to_read) {
    base::span<const uint8_t> buffer;
    const MojoResult begin_result =
        readable->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);

    DCHECK_EQ(begin_result, MOJO_RESULT_OK);
    if (buffer.size() < bytes_to_read) {
      ADD_FAILURE() << "ReadDataFromDataPipe expected " << bytes_to_read
                    << " bytes but only received " << buffer.size() << " bytes";
      return Vector<uint8_t>();
    }
    buffer = buffer.first(bytes_to_read);

    Vector<uint8_t> data_to_pass;
    data_to_pass.AppendRange(buffer.begin(), buffer.end());

    const MojoResult end_result = readable->EndReadData(buffer.size());
    DCHECK_EQ(end_result, MOJO_RESULT_OK);

    return data_to_pass;
  }

  // Returns nullptr if something bad happens.
  std::unique_ptr<TestWebSocket> Connect(
      uint32_t capacity,
      mojo::ScopedDataPipeProducerHandle* writable,
      mojo::ScopedDataPipeConsumerHandle* readable,
      mojo::Remote<network::mojom::blink::WebSocketClient>* client) {
    if (!Channel()->Connect(KURL("ws://localhost/"), "")) {
      ADD_FAILURE() << "WebSocketChannelImpl::Connect returns false.";
      return nullptr;
    }
    test::RunPendingTasks();
    auto connect_args = connector_.TakeConnectArgs();

    if (connect_args.size() != 1) {
      ADD_FAILURE() << "|connect_args.size()| is " << connect_args.size();
      return nullptr;
    }
    mojo::Remote<network::mojom::blink::WebSocketHandshakeClient>
        handshake_client(std::move(connect_args[0].handshake_client));

    mojo::ScopedDataPipeConsumerHandle remote_readable;
    if (CreateDataPipe(capacity, writable, &remote_readable) !=
        MOJO_RESULT_OK) {
      ADD_FAILURE() << "Failed to create a datapipe.";
      return nullptr;
    }

    mojo::ScopedDataPipeProducerHandle remote_writable;
    if (CreateDataPipe(capacity, &remote_writable, readable) !=
        MOJO_RESULT_OK) {
      ADD_FAILURE() << "Failed to create a datapipe.";
      return nullptr;
    }
    auto websocket = EstablishConnection(handshake_client.get(), "", "",
                                         std::move(remote_readable),
                                         std::move(remote_writable), client);
    test::RunPendingTasks();
    return websocket;
  }

  WebSocketConnector connector_;
  Persistent<MockWebSocketChannelClient> channel_client_;
  std::unique_ptr<MockWebSocketHandshakeThrottle> handshake_throttle_;
  const raw_ptr<MockWebSocketHandshakeThrottle, DanglingUntriaged>
      raw_handshake_throttle_;
  Persistent<WebSocketChannelImpl> channel_;
  uint64_t sum_of_consumed_buffered_amount_;

  base::WeakPtrFactory<WebSocketChannelImplTest> weak_ptr_factory_;
};

class CallTrackingClosure {
 public:
  CallTrackingClosure() = default;

  CallTrackingClosure(const CallTrackingClosure&) = delete;
  CallTrackingClosure& operator=(const CallTrackingClosure&) = delete;

  base::OnceClosure Closure() {
    // This use of base::Unretained is safe because nothing can call the
    // callback once the test has finished.
    return WTF::BindOnce(&CallTrackingClosure::Called, base::Unretained(this));
  }

  bool WasCalled() const { return was_called_; }

 private:
  void Called() { was_called_ = true; }

  bool was_called_ = false;
};

std::ostream& operator<<(
    std::ostream& o,
    const WebSocketChannelImplTest::TestWebSocket::DataFrame& f) {
  return o << " type = " << f.type << ", data = (...)";
}

TEST_F(WebSocketChannelImplTest, ConnectSuccess) {
  Checkpoint checkpoint;

  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidConnect(String("a"), String("b")));
  }

  // Make sure that SiteForCookies() is set to the given value.
  EXPECT_TRUE(net::SiteForCookies::FromUrl(GURL("http://example.com/"))
                  .IsEquivalent(GetDocument().SiteForCookies()));

  ASSERT_TRUE(Channel()->Connect(KURL("ws://localhost/"), "x"));
  EXPECT_TRUE(connector_.GetConnectArgs().empty());

  test::RunPendingTasks();
  auto connect_args = connector_.TakeConnectArgs();

  ASSERT_EQ(1u, connect_args.size());
  EXPECT_EQ(connect_args[0].url, KURL("ws://localhost/"));
  EXPECT_TRUE(connect_args[0].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(GURL("http://example.com/"))));

  EXPECT_EQ(connect_args[0].protocols, Vector<String>({"x"}));

  mojo::Remote<network::mojom::blink::WebSocketHandshakeClient>
      handshake_client(std::move(connect_args[0].handshake_client));

  mojo::ScopedDataPipeProducerHandle incoming_writable;
  mojo::ScopedDataPipeConsumerHandle incoming_readable;
  ASSERT_EQ(CreateDataPipe(32, &incoming_writable, &incoming_readable),
            MOJO_RESULT_OK);

  mojo::ScopedDataPipeProducerHandle outgoing_writable;
  mojo::ScopedDataPipeConsumerHandle outgoing_readable;
  ASSERT_EQ(CreateDataPipe(32, &outgoing_writable, &outgoing_readable),
            MOJO_RESULT_OK);

  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = EstablishConnection(handshake_client.get(), "a", "b",
                                       std::move(incoming_readable),
                                       std::move(outgoing_writable), &client);

  checkpoint.Call(1);
  test::RunPendingTasks();

  EXPECT_TRUE(websocket->IsStartReceivingCalled());
}

TEST_F(WebSocketChannelImplTest, MojoConnectionErrorDuringHandshake) {
  Checkpoint checkpoint;

  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(
        *ChannelClient(),
        DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete,
                 WebSocketChannel::kCloseEventCodeAbnormalClosure, String()));
  }

  ASSERT_TRUE(Channel()->Connect(KURL("ws://localhost/"), "x"));
  EXPECT_TRUE(connector_.GetConnectArgs().empty());

  test::RunPendingTasks();
  auto connect_args = connector_.TakeConnectArgs();

  ASSERT_EQ(1u, connect_args.size());

  checkpoint.Call(1);
  // This destroys the PendingReceiver, which will be detected as a mojo
  // connection error.
  connect_args.clear();
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, SendText) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  Channel()->Send("foo", base::OnceClosure());
  Channel()->Send("bar", base::OnceClosure());
  Channel()->Send("baz", base::OnceClosure());

  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetDataFrames(),
            (DataFrames{{WebSocketMessageType::TEXT, strlen("foo")},
                        {WebSocketMessageType::TEXT, strlen("bar")},
                        {WebSocketMessageType::TEXT, strlen("baz")}}));
}

TEST_F(WebSocketChannelImplTest, SendBinaryInVector) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  DOMArrayBuffer* foo_buffer =
      DOMArrayBuffer::Create(base::byte_span_from_cstring("foo"));
  Channel()->Send(*foo_buffer, 0, 3, base::OnceClosure());
  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetDataFrames(),
            (DataFrames{{WebSocketMessageType::BINARY, strlen("foo")}}));

  ASSERT_EQ(AsVector("foo"), ReadDataFromDataPipe(readable, 3u));
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferPartial) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  DOMArrayBuffer* foobar_buffer =
      DOMArrayBuffer::Create(base::byte_span_from_cstring("foobar"));
  DOMArrayBuffer* qbazux_buffer =
      DOMArrayBuffer::Create(base::byte_span_from_cstring("qbazux"));
  Channel()->Send(*foobar_buffer, 0, 3, base::OnceClosure());
  Channel()->Send(*foobar_buffer, 3, 3, base::OnceClosure());
  Channel()->Send(*qbazux_buffer, 1, 3, base::OnceClosure());
  Channel()->Send(*qbazux_buffer, 2, 1, base::OnceClosure());

  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetDataFrames(),
            (DataFrames{
                {WebSocketMessageType::BINARY, strlen("foo")},
                {WebSocketMessageType::BINARY, strlen("bar")},
                {WebSocketMessageType::BINARY, strlen("baz")},
                {WebSocketMessageType::BINARY, strlen("a")},
            }));

  ASSERT_EQ(AsVector("foo"), ReadDataFromDataPipe(readable, 3u));
  ASSERT_EQ(AsVector("bar"), ReadDataFromDataPipe(readable, 3u));
  ASSERT_EQ(AsVector("baz"), ReadDataFromDataPipe(readable, 3u));
  ASSERT_EQ(AsVector("a"), ReadDataFromDataPipe(readable, 1u));
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferWithNullBytes) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  // Used to CHECK() string was not truncated at first NUL.
  constexpr size_t kLengthOfEachMessage = 3;
  {
    auto byte_span = base::byte_span_from_cstring("\0ar");
    CHECK_EQ(kLengthOfEachMessage, byte_span.size());
    DOMArrayBuffer* b = DOMArrayBuffer::Create(byte_span);
    Channel()->Send(*b, 0, 3, base::OnceClosure());
  }
  {
    auto byte_span = base::byte_span_from_cstring("b\0z");
    CHECK_EQ(kLengthOfEachMessage, byte_span.size());
    DOMArrayBuffer* b = DOMArrayBuffer::Create(byte_span);
    Channel()->Send(*b, 0, 3, base::OnceClosure());
  }
  {
    auto byte_span = base::byte_span_from_cstring("qu\0");
    CHECK_EQ(kLengthOfEachMessage, byte_span.size());
    DOMArrayBuffer* b = DOMArrayBuffer::Create(byte_span);
    Channel()->Send(*b, 0, 3, base::OnceClosure());
  }
  {
    auto byte_span = base::byte_span_from_cstring("\0\0\0");
    CHECK_EQ(kLengthOfEachMessage, byte_span.size());
    DOMArrayBuffer* b = DOMArrayBuffer::Create(byte_span);
    Channel()->Send(*b, 0, 3, base::OnceClosure());
  }

  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetDataFrames(),
            (DataFrames{
                {WebSocketMessageType::BINARY, kLengthOfEachMessage},
                {WebSocketMessageType::BINARY, kLengthOfEachMessage},
                {WebSocketMessageType::BINARY, kLengthOfEachMessage},
                {WebSocketMessageType::BINARY, kLengthOfEachMessage},
            }));

  ASSERT_EQ(AsVector("\0ar", kLengthOfEachMessage),
            ReadDataFromDataPipe(readable, 3u));
  ASSERT_EQ(AsVector("b\0z", kLengthOfEachMessage),
            ReadDataFromDataPipe(readable, 3u));
  ASSERT_EQ(AsVector("qu\0", kLengthOfEachMessage),
            ReadDataFromDataPipe(readable, 3u));
  ASSERT_EQ(AsVector("\0\0\0", kLengthOfEachMessage),
            ReadDataFromDataPipe(readable, 3u));
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferNonLatin1UTF8) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  DOMArrayBuffer* b =
      DOMArrayBuffer::Create(base::byte_span_from_cstring("\xe7\x8b\x90"));
  Channel()->Send(*b, 0, 3, base::OnceClosure());

  test::RunPendingTasks();

  EXPECT_EQ(
      websocket->GetDataFrames(),
      (DataFrames{{WebSocketMessageType::BINARY, strlen("\xe7\x8b\x90")}}));

  ASSERT_EQ(AsVector("\xe7\x8b\x90"), ReadDataFromDataPipe(readable, 3u));
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferNonUTF8) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  DOMArrayBuffer* b =
      DOMArrayBuffer::Create(base::byte_span_from_cstring("\x80\xff\xe7"));
  Channel()->Send(*b, 0, 3, base::OnceClosure());

  test::RunPendingTasks();

  EXPECT_EQ(
      websocket->GetDataFrames(),
      (DataFrames{{WebSocketMessageType::BINARY, strlen("\x80\xff\xe7")}}));

  ASSERT_EQ(AsVector("\x80\xff\xe7"), ReadDataFromDataPipe(readable, 3u));
}

TEST_F(WebSocketChannelImplTest, SendTextSync) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  test::RunPendingTasks();
  CallTrackingClosure closure;
  EXPECT_EQ(WebSocketChannel::SendResult::kSentSynchronously,
            Channel()->Send("hello", closure.Closure()));
  EXPECT_FALSE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplTest, SendTextAsyncDueToQueueing) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  // The size of message matches the capacity of the datapipe
  constexpr int kMessageSize = 4 * 1024;

  // Ideally we'd use a Blob to block the queue in this test, but setting up a
  // working blob environment in a unit-test is complicated, so just block
  // behind a larger string instead.
  std::string long_message(kMessageSize, 'a');

  Channel()->Send(long_message, base::OnceClosure());
  CallTrackingClosure closure;
  EXPECT_EQ(WebSocketChannel::SendResult::kCallbackWillBeCalled,
            Channel()->Send(long_message, closure.Closure()));

  ReadDataFromDataPipe(readable, kMessageSize);
  test::RunPendingTasks();

  ReadDataFromDataPipe(readable, kMessageSize);

  EXPECT_TRUE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplTest, SendTextAsyncDueToMessageSize) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  // The size of message is greater than the capacity of the datapipe
  constexpr int kMessageSize = 5 * 1024;
  std::string long_message(kMessageSize, 'a');

  CallTrackingClosure closure;
  EXPECT_EQ(WebSocketChannel::SendResult::kCallbackWillBeCalled,
            Channel()->Send(long_message, closure.Closure()));

  ReadDataFromDataPipe(readable, 4 * 1024);
  test::RunPendingTasks();

  EXPECT_TRUE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferSync) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  test::RunPendingTasks();

  CallTrackingClosure closure;
  const auto* b = DOMArrayBuffer::Create(base::byte_span_from_cstring("hello"));
  EXPECT_EQ(WebSocketChannel::SendResult::kSentSynchronously,
            Channel()->Send(*b, 0, 5, closure.Closure()));

  test::RunPendingTasks();

  EXPECT_FALSE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferAsyncDueToQueueing) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  // The size of message matches the capacity of the datapipe
  constexpr int kMessageSize = 1024;
  std::string long_message(kMessageSize, 'a');

  CallTrackingClosure closure;
  const auto* b = DOMArrayBuffer::Create(base::as_byte_span(long_message));
  Channel()->Send(*b, 0, kMessageSize, base::OnceClosure());
  EXPECT_EQ(WebSocketChannel::SendResult::kCallbackWillBeCalled,
            Channel()->Send(*b, 0, kMessageSize, closure.Closure()));

  ReadDataFromDataPipe(readable, kMessageSize);
  test::RunPendingTasks();

  ReadDataFromDataPipe(readable, kMessageSize);

  EXPECT_TRUE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferAsyncDueToMessageSize) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  // The size of message is greater than the capacity of the datapipe
  constexpr int kMessageSize = 2 * 1024;
  std::string long_message(kMessageSize, 'a');

  CallTrackingClosure closure;
  const auto* b = DOMArrayBuffer::Create(base::as_byte_span(long_message));
  EXPECT_EQ(WebSocketChannel::SendResult::kCallbackWillBeCalled,
            Channel()->Send(*b, 0, kMessageSize, closure.Closure()));

  ReadDataFromDataPipe(readable, 1024);
  test::RunPendingTasks();

  EXPECT_TRUE(closure.WasCalled());
}

// FIXME: Add tests for WebSocketChannel::send(scoped_refptr<BlobDataHandle>)

TEST_F(WebSocketChannelImplTest, ReceiveText) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("FOO")));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("BAR")));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  size_t actually_written_bytes = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      writable->WriteData(base::byte_span_from_cstring("FOOBAR"),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 6u);

  client->OnDataFrame(true, WebSocketMessageType::TEXT, 3);
  client->OnDataFrame(true, WebSocketMessageType::TEXT, 3);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveTextContinuation) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("BAZ")));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  size_t actually_written_bytes = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      writable->WriteData(base::byte_span_from_cstring("BAZ"),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 3u);

  client->OnDataFrame(false, WebSocketMessageType::TEXT, 1);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 1);
  client->OnDataFrame(true, WebSocketMessageType::CONTINUATION, 1);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveTextNonLatin1) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    UChar non_latin1_string[] = {0x72d0, 0x0914, 0x0000};
    EXPECT_CALL(*ChannelClient(),
                DidReceiveTextMessage(String(non_latin1_string)));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  size_t actually_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData(
                base::byte_span_from_cstring("\xe7\x8b\x90\xe0\xa4\x94"),
                MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 6u);

  client->OnDataFrame(true, WebSocketMessageType::TEXT, 6);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveTextNonLatin1Continuation) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    UChar non_latin1_string[] = {0x72d0, 0x0914, 0x0000};
    EXPECT_CALL(*ChannelClient(),
                DidReceiveTextMessage(String(non_latin1_string)));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  size_t actually_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData(
                base::byte_span_from_cstring("\xe7\x8b\x90\xe0\xa4\x94"),
                MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 6u);

  client->OnDataFrame(false, WebSocketMessageType::TEXT, 2);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 2);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 1);
  client->OnDataFrame(true, WebSocketMessageType::CONTINUATION, 1);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveBinary) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'F', 'O', 'O'})));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  size_t actually_written_bytes = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      writable->WriteData(base::byte_span_from_cstring("FOO"),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 3u);

  client->OnDataFrame(true, WebSocketMessageType::BINARY, 3);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveBinaryContinuation) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'B', 'A', 'Z'})));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  size_t actually_written_bytes = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      writable->WriteData(base::byte_span_from_cstring("BAZ"),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 3u);

  client->OnDataFrame(false, WebSocketMessageType::BINARY, 1);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 1);
  client->OnDataFrame(true, WebSocketMessageType::CONTINUATION, 1);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveBinaryWithNullBytes) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'\0', 'A', '3'})));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'B', '\0', 'Z'})));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'Q', 'U', '\0'})));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'\0', '\0', '\0'})));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  using std::string_view_literals::operator""sv;  // For NUL characters.
  size_t actually_written_bytes = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      writable->WriteData(base::as_byte_span("\0A3B\0ZQU\0\0\0\0"sv),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 12u);

  client->OnDataFrame(true, WebSocketMessageType::BINARY, 3);
  client->OnDataFrame(true, WebSocketMessageType::BINARY, 3);
  client->OnDataFrame(true, WebSocketMessageType::BINARY, 3);
  client->OnDataFrame(true, WebSocketMessageType::BINARY, 3);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveBinaryNonLatin1UTF8) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{
                    '\xe7', '\x8b', '\x90', '\xe0', '\xa4', '\x94'})));
  }
  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  size_t actually_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData(
                base::byte_span_from_cstring("\xe7\x8b\x90\xe0\xa4\x94"),
                MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 6u);

  client->OnDataFrame(true, WebSocketMessageType::BINARY, 6);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveBinaryNonLatin1UTF8Continuation) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{
                    '\xe7', '\x8b', '\x90', '\xe0', '\xa4', '\x94'})));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  size_t actually_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData(
                base::byte_span_from_cstring("\xe7\x8b\x90\xe0\xa4\x94"),
                MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 6u);

  client->OnDataFrame(false, WebSocketMessageType::BINARY, 2);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 2);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 1);
  client->OnDataFrame(true, WebSocketMessageType::CONTINUATION, 1);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveBinaryNonUTF8) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'\x80', '\xff'})));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  size_t actually_written_bytes = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      writable->WriteData(base::byte_span_from_cstring("\x80\xff"),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 2u);

  client->OnDataFrame(true, WebSocketMessageType::BINARY, 2);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveWithExplicitBackpressure) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("abc")));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  size_t actually_written_bytes = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      writable->WriteData(base::byte_span_from_cstring("abc"),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 3u);

  Channel()->ApplyBackpressure();

  client->OnDataFrame(true, WebSocketMessageType::TEXT, 3);
  test::RunPendingTasks();

  checkpoint.Call(1);
  Channel()->RemoveBackpressure();
}

TEST_F(WebSocketChannelImplTest,
       ReceiveMultipleMessagesWithSmallDataPipeWrites) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("abc")));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("")));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("")));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("de")));
    EXPECT_CALL(checkpoint, Call(4));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("")));
    EXPECT_CALL(checkpoint, Call(5));
    EXPECT_CALL(checkpoint, Call(6));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("fghijkl")));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->OnDataFrame(true, WebSocketMessageType::TEXT, 3);
  client->OnDataFrame(true, WebSocketMessageType::TEXT, 0);
  client->OnDataFrame(true, WebSocketMessageType::TEXT, 0);
  client->OnDataFrame(true, WebSocketMessageType::TEXT, 2);
  test::RunPendingTasks();

  checkpoint.Call(1);
  size_t actually_written_bytes = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      writable->WriteData(base::byte_span_from_cstring("ab"),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 2u);
  test::RunPendingTasks();

  checkpoint.Call(2);
  ASSERT_EQ(
      MOJO_RESULT_OK,
      writable->WriteData(base::byte_span_from_cstring("cd"),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 2u);
  test::RunPendingTasks();

  checkpoint.Call(3);
  ASSERT_EQ(
      MOJO_RESULT_OK,
      writable->WriteData(base::byte_span_from_cstring("efgh"),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 4u);
  test::RunPendingTasks();

  checkpoint.Call(4);
  client->OnDataFrame(true, WebSocketMessageType::TEXT, 0);
  client->OnDataFrame(false, WebSocketMessageType::TEXT, 1);
  test::RunPendingTasks();

  checkpoint.Call(5);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 1);
  client->OnDataFrame(true, WebSocketMessageType::CONTINUATION, 5);
  test::RunPendingTasks();

  checkpoint.Call(6);
  ASSERT_EQ(
      MOJO_RESULT_OK,
      writable->WriteData(base::byte_span_from_cstring("ijkl"),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, 4u);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ConnectionCloseInitiatedByServer) {
  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(), DidStartClosingHandshake());
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(*ChannelClient(),
                DidClose(WebSocketChannelClient::kClosingHandshakeComplete,
                         WebSocketChannel::kCloseEventCodeNormalClosure,
                         String("close reason")));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->OnClosingHandshake();
  test::RunPendingTasks();

  EXPECT_FALSE(websocket->IsStartClosingHandshakeCalled());

  checkpoint.Call(1);
  Channel()->Close(WebSocketChannel::kCloseEventCodeNormalClosure,
                   "close reason");
  test::RunPendingTasks();

  EXPECT_TRUE(websocket->IsStartClosingHandshakeCalled());
  EXPECT_EQ(websocket->GetClosingCode(),
            WebSocketChannel::kCloseEventCodeNormalClosure);
  EXPECT_EQ(websocket->GetClosingReason(), "close reason");

  checkpoint.Call(2);
  client->OnDropChannel(true, WebSocketChannel::kCloseEventCodeNormalClosure,
                        "close reason");
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ConnectionCloseInitiatedByClient) {
  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(),
                DidClose(WebSocketChannelClient::kClosingHandshakeComplete,
                         WebSocketChannel::kCloseEventCodeNormalClosure,
                         String("close reason")));
    EXPECT_CALL(checkpoint, Call(2));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  EXPECT_FALSE(websocket->IsStartClosingHandshakeCalled());
  Channel()->Close(WebSocketChannel::kCloseEventCodeNormalClosure,
                   "close reason");
  test::RunPendingTasks();
  EXPECT_TRUE(websocket->IsStartClosingHandshakeCalled());
  EXPECT_EQ(websocket->GetClosingCode(),
            WebSocketChannel::kCloseEventCodeNormalClosure);
  EXPECT_EQ(websocket->GetClosingReason(), "close reason");

  checkpoint.Call(1);
  client->OnDropChannel(true, WebSocketChannel::kCloseEventCodeNormalClosure,
                        "close reason");
  test::RunPendingTasks();
  checkpoint.Call(2);
}

TEST_F(WebSocketChannelImplTest, MojoConnectionError) {
  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(
        *ChannelClient(),
        DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete,
                 WebSocketChannel::kCloseEventCodeAbnormalClosure, String()));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  // Send a frame so that the WebSocketChannelImpl try to read the data pipe.
  client->OnDataFrame(true, WebSocketMessageType::TEXT, 1024);

  // We shouldn't detect a connection error on data pipes and mojom::WebSocket.
  writable.reset();
  websocket = nullptr;
  test::RunPendingTasks();

  // We should detect a connection error on the client.
  checkpoint.Call(1);
  client.reset();
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, FailFromClient) {
  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(
        *ChannelClient(),
        DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete,
                 WebSocketChannel::kCloseEventCodeAbnormalClosure, String()));
    EXPECT_CALL(checkpoint, Call(2));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  Channel()->Fail(
      "fail message from WebSocket", mojom::ConsoleMessageLevel::kError,
      std::make_unique<SourceLocation>(String(), String(), 0, 0, nullptr));
  checkpoint.Call(1);

  test::RunPendingTasks();
  checkpoint.Call(2);
}

class WebSocketChannelImplHandshakeThrottleTest
    : public WebSocketChannelImplTest {
 public:
  WebSocketChannelImplHandshakeThrottleTest()
      : WebSocketChannelImplTest(
            std::make_unique<StrictMock<MockWebSocketHandshakeThrottle>>()) {}

  static KURL url() { return KURL("ws://localhost/"); }
};

TEST_F(WebSocketChannelImplHandshakeThrottleTest, ThrottleSucceedsFirst) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _, _, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  }

  ASSERT_TRUE(Channel()->Connect(url(), ""));
  test::RunPendingTasks();

  auto connect_args = connector_.TakeConnectArgs();

  ASSERT_EQ(1u, connect_args.size());

  mojo::Remote<network::mojom::blink::WebSocketHandshakeClient>
      handshake_client(std::move(connect_args[0].handshake_client));
  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  ASSERT_EQ(CreateDataPipe(32, &writable, &readable), MOJO_RESULT_OK);

  mojo::ScopedDataPipeProducerHandle outgoing_writable;
  mojo::ScopedDataPipeConsumerHandle outgoing_readable;
  ASSERT_EQ(CreateDataPipe(32, &outgoing_writable, &outgoing_readable),
            MOJO_RESULT_OK);

  mojo::Remote<network::mojom::blink::WebSocketClient> client;

  checkpoint.Call(1);
  test::RunPendingTasks();

  Channel()->OnCompletion(std::nullopt);
  checkpoint.Call(2);

  auto websocket =
      EstablishConnection(handshake_client.get(), "", "", std::move(readable),
                          std::move(outgoing_writable), &client);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplHandshakeThrottleTest, HandshakeSucceedsFirst) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _, _, _));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;

  checkpoint.Call(1);
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  checkpoint.Call(2);
  Channel()->OnCompletion(std::nullopt);
}

// This happens if JS code calls close() during the handshake.
TEST_F(WebSocketChannelImplHandshakeThrottleTest, FailDuringThrottle) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _, _, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(2));
  }

  Channel()->Connect(url(), "");
  Channel()->Fail(
      "close during handshake", mojom::ConsoleMessageLevel::kWarning,
      std::make_unique<SourceLocation>(String(), String(), 0, 0, nullptr));
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
}

// It makes no difference to the behaviour if the WebSocketHandle has actually
// connected.
TEST_F(WebSocketChannelImplHandshakeThrottleTest,
       FailDuringThrottleAfterConnect) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _, _, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(2));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  Channel()->Fail(
      "close during handshake", mojom::ConsoleMessageLevel::kWarning,
      std::make_unique<SourceLocation>(String(), String(), 0, 0, nullptr));
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
}

TEST_F(WebSocketChannelImplHandshakeThrottleTest, DisconnectDuringThrottle) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _, _, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(1));
  }

  Channel()->Connect(url(), "");
  test::RunPendingTasks();

  Channel()->Disconnect();
  checkpoint.Call(1);

  auto connect_args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, connect_args.size());

  mojo::Remote<network::mojom::blink::WebSocketHandshakeClient>
      handshake_client(std::move(connect_args[0].handshake_client));

  CallTrackingClosure closure;
  handshake_client.set_disconnect_handler(closure.Closure());
  EXPECT_FALSE(closure.WasCalled());

  test::RunPendingTasks();

  EXPECT_TRUE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplHandshakeThrottleTest,
       DisconnectDuringThrottleAfterConnect) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _, _, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(1));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  Channel()->Disconnect();
  checkpoint.Call(1);

  CallTrackingClosure closure;
  client.set_disconnect_handler(closure.Closure());
  EXPECT_FALSE(closure.WasCalled());

  test::RunPendingTasks();

  EXPECT_TRUE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplHandshakeThrottleTest,
       ThrottleReportsErrorBeforeConnect) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _, _, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
    EXPECT_CALL(checkpoint, Call(3));
  }

  Channel()->Connect(url(), "");

  test::RunPendingTasks();
  checkpoint.Call(1);

  Channel()->OnCompletion("Connection blocked by throttle");
  checkpoint.Call(2);

  test::RunPendingTasks();
  checkpoint.Call(3);
}

TEST_F(WebSocketChannelImplHandshakeThrottleTest,
       ThrottleReportsErrorAfterConnect) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _, _, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
    EXPECT_CALL(checkpoint, Call(2));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  Channel()->OnCompletion("Connection blocked by throttle");
  checkpoint.Call(1);

  test::RunPendingTasks();
  checkpoint.Call(2);
}

TEST_F(WebSocketChannelImplHandshakeThrottleTest, ConnectFailBeforeThrottle) {
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _, _, _));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
  }

  ASSERT_TRUE(Channel()->Connect(url(), ""));
  test::RunPendingTasks();

  auto connect_args = connector_.TakeConnectArgs();

  ASSERT_EQ(1u, connect_args.size());

  connect_args.clear();
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, RemoteConnectionCloseDuringSend) {
  {
    InSequence s;

    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_));
    EXPECT_CALL(*ChannelClient(), DidStartClosingHandshake());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  // The message must be larger than the data pipe.
  std::string message(16 * 1024, 'a');
  Channel()->Send(message, base::OnceClosure());

  client->OnClosingHandshake();
  test::RunPendingTasks();

  client->OnDropChannel(true, WebSocketChannel::kCloseEventCodeNormalClosure,
                        "");

  // The test passes if this doesn't crash.
  test::RunPendingTasks();
}

class MockWebSocketConnector : public mojom::blink::WebSocketConnector {
 public:
  MOCK_METHOD(
      void,
      Connect,
      (const KURL&,
       const Vector<String>&,
       const net::SiteForCookies&,
       const String&,
       net::StorageAccessApiStatus,
       mojo::PendingRemote<network::mojom::blink::WebSocketHandshakeClient>,
       const std::optional<base::UnguessableToken>&));
};

// This can't use WebSocketChannelImplTest because it requires multiple
// WebSocketChannels to be connected.
class WebSocketChannelImplMultipleTest : public WebSocketChannelImplTestBase {
 public:
  base::WeakPtr<WebSocketChannelImplTestBase> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void BindWebSocketConnector(mojo::ScopedMessagePipeHandle handle) override {
    connector_receiver_set_.Add(
        &connector_, mojo::PendingReceiver<mojom::blink::WebSocketConnector>(
                         std::move(handle)));
  }

 protected:
  mojo::ReceiverSet<mojom::blink::WebSocketConnector> connector_receiver_set_;
  StrictMock<MockWebSocketConnector> connector_;

  base::WeakPtrFactory<WebSocketChannelImplMultipleTest> weak_ptr_factory_{
      this};
};

TEST_F(WebSocketChannelImplMultipleTest, ConnectionLimit) {
  Checkpoint checkpoint;

  // We need to keep the handshake clients alive otherwise they will cause
  // connection failures.
  mojo::RemoteSet<network::mojom::blink::WebSocketHandshakeClient>
      handshake_clients;
  auto handshake_client_add_action =
      [&handshake_clients](
          Unused, Unused, Unused, Unused, Unused,
          mojo::PendingRemote<network::mojom::blink::WebSocketHandshakeClient>
              handshake_client,
          Unused) { handshake_clients.Add(std::move(handshake_client)); };

  auto failure_handshake_throttle =
      std::make_unique<StrictMock<MockWebSocketHandshakeThrottle>>();
  auto* failure_channel_client = MockWebSocketChannelClient::Create();

  auto successful_handshake_throttle =
      std::make_unique<StrictMock<MockWebSocketHandshakeThrottle>>();
  auto* successful_channel_client = MockWebSocketChannelClient::Create();

  auto url = KURL("ws://localhost/");

  {
    InSequence s;
    EXPECT_CALL(connector_, Connect(_, _, _, _, _, _, _))
        .Times(WebSocketChannelImpl::kMaxWebSocketsPerRenderProcess)
        .WillRepeatedly(handshake_client_add_action);

    EXPECT_CALL(checkpoint, Call(1));

    EXPECT_CALL(*failure_channel_client, DidError());
    EXPECT_CALL(
        *failure_channel_client,
        DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete,
                 WebSocketChannel::kCloseEventCodeAbnormalClosure, String()));
    EXPECT_CALL(*failure_handshake_throttle, Destructor());

    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(*successful_handshake_throttle, ThrottleHandshake(_, _, _, _));
    EXPECT_CALL(connector_, Connect(_, _, _, _, _, _, _))
        .WillOnce(handshake_client_add_action);
    EXPECT_CALL(*successful_handshake_throttle, Destructor());
  }

  WebSocketChannelImpl*
      channels[WebSocketChannelImpl::kMaxWebSocketsPerRenderProcess] = {};
  for (WebSocketChannelImpl*& channel : channels) {
    auto handshake_throttle =
        std::make_unique<StrictMock<MockWebSocketHandshakeThrottle>>();
    EXPECT_CALL(*handshake_throttle, ThrottleHandshake(_, _, _, _));
    EXPECT_CALL(*handshake_throttle, Destructor());

    // This is kept alive by WebSocketChannelImpl so we don't need to retain
    // our own reference.
    auto* channel_client = MockWebSocketChannelClient::Create();

    channel = WebSocketChannelImpl::CreateForTesting(
        GetFrame().DomWindow(), channel_client, CaptureSourceLocation(),
        std::move(handshake_throttle));
    channel->Connect(url, "");
  }

  // Connect() is called via mojo and so asynchronously.
  test::RunPendingTasks();

  auto* failing_channel = WebSocketChannelImpl::CreateForTesting(
      GetFrame().DomWindow(), failure_channel_client, CaptureSourceLocation(),
      std::move(failure_handshake_throttle));
  failing_channel->Connect(url, "");

  checkpoint.Call(1);

  // Give DidClose() a chance to be called.
  test::RunPendingTasks();

  // Abort all the pending connections to permit more to be created.
  for (auto* channel : channels) {
    channel->Disconnect();
  }

  checkpoint.Call(2);

  auto* successful_channel = WebSocketChannelImpl::CreateForTesting(
      GetFrame().DomWindow(), successful_channel_client,
      CaptureSourceLocation(), std::move(successful_handshake_throttle));
  successful_channel->Connect(url, "");

  // Let the connect be passed through mojo.
  test::RunPendingTasks();

  // Destroy the channel to stop it interfering with other tests.
  successful_channel->Disconnect();
}

}  // namespace blink
