// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base_switches.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/smart_card/mock_smart_card_context_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/smart_card_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/mojom/smart_card.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom.h"

using base::test::RunOnceCallback;
using base::test::TestFuture;
using device::mojom::SmartCardConnection;
using device::mojom::SmartCardConnectionState;
using device::mojom::SmartCardContext;
using device::mojom::SmartCardDisposition;
using device::mojom::SmartCardError;
using device::mojom::SmartCardListReadersResult;
using device::mojom::SmartCardProtocol;
using device::mojom::SmartCardReaderStateFlags;
using device::mojom::SmartCardReaderStateOut;
using device::mojom::SmartCardReaderStateOutPtr;
using device::mojom::SmartCardResult;
using device::mojom::SmartCardShareMode;
using device::mojom::SmartCardStatus;
using device::mojom::SmartCardSuccess;
using device::mojom::SmartCardTransaction;
using ::testing::_;
using testing::Exactly;
using testing::HasSubstr;
using testing::InSequence;
using testing::MatchesRegex;
using testing::Return;
using testing::StrictMock;

namespace content {

namespace {

constexpr char kFakeReader[] = "Fake reader";

class MockSmartCardConnection : public device::mojom::SmartCardConnection {
 public:
  MOCK_METHOD(void,
              Disconnect,
              (SmartCardDisposition disposition, DisconnectCallback callback),
              (override));

  MOCK_METHOD(void,
              Transmit,
              (device::mojom::SmartCardProtocol protocol,
               const std::vector<uint8_t>& data,
               TransmitCallback callback),
              (override));

  MOCK_METHOD(void,
              Control,
              (uint32_t control_code,
               const std::vector<uint8_t>& data,
               ControlCallback callback),
              (override));

  MOCK_METHOD(void,
              GetAttrib,
              (uint32_t id, GetAttribCallback callback),
              (override));

  MOCK_METHOD(void,
              SetAttrib,
              (uint32_t id,
               const std::vector<uint8_t>& data,
               SetAttribCallback callback),
              (override));

  MOCK_METHOD(void, Status, (StatusCallback callback), (override));

  MOCK_METHOD(void,
              BeginTransaction,
              (BeginTransactionCallback callback),
              (override));

  void ExpectBeginTransaction(
      mojo::AssociatedReceiver<SmartCardTransaction>& transaction_receiver) {
    EXPECT_CALL(*this, BeginTransaction(_))
        .WillOnce([&transaction_receiver](
                      SmartCardConnection::BeginTransactionCallback callback) {
          std::move(callback).Run(
              device::mojom::SmartCardTransactionResult::NewTransaction(
                  transaction_receiver.BindNewEndpointAndPassRemote()));
        });
  }
};

class MockSmartCardTransaction : public SmartCardTransaction {
 public:
  MOCK_METHOD(void,
              EndTransaction,
              (SmartCardDisposition disposition,
               EndTransactionCallback callback),
              (override));

  void ExpectEndTransaction(SmartCardDisposition disposition) {
    EXPECT_CALL(*this, EndTransaction(disposition, _))
        .WillOnce([](SmartCardDisposition disposition,
                     SmartCardTransaction::EndTransactionCallback callback) {
          std::move(callback).Run(
              SmartCardResult::NewSuccess(SmartCardSuccess::kOk));
        });
  }
};

class FakeSmartCardDelegate : public SmartCardDelegate {
 public:
  FakeSmartCardDelegate() = default;
  // SmartCardDelegate overrides:
  mojo::PendingRemote<device::mojom::SmartCardContextFactory>
  GetSmartCardContextFactory(BrowserContext& browser_context) override;

  MOCK_METHOD(bool,
              IsPermissionBlocked,
              (RenderFrameHost & render_frame_host),
              (override));

  MOCK_METHOD(bool,
              HasReaderPermission,
              (content::RenderFrameHost & render_frame_host,
               const std::string& reader_name),
              (override));

  MOCK_METHOD(void,
              RequestReaderPermission,
              (content::RenderFrameHost & render_frame_host,
               const std::string& reader_name,
               RequestReaderPermissionCallback callback),
              (override));

  void ExpectHasReaderPermission(const std::string& reader_name) {
    EXPECT_CALL(*this, HasReaderPermission(_, reader_name))
        .WillOnce(Return(true));
  }

  MockSmartCardContextFactory mock_context_factory;
};

class SmartCardTestContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  SmartCardTestContentBrowserClient();
  SmartCardTestContentBrowserClient(SmartCardTestContentBrowserClient&) =
      delete;
  SmartCardTestContentBrowserClient& operator=(
      SmartCardTestContentBrowserClient&) = delete;
  ~SmartCardTestContentBrowserClient() override;

  void SetSmartCardDelegate(std::unique_ptr<SmartCardDelegate>);

  // ContentBrowserClient:
  SmartCardDelegate* GetSmartCardDelegate() override;
  bool ShouldUrlUseApplicationIsolationLevel(BrowserContext* browser_context,
                                             const GURL& url) override;
  std::optional<blink::ParsedPermissionsPolicy>
  GetPermissionsPolicyForIsolatedWebApp(WebContents* web_contents,
                                        const url::Origin& app_origin) override;

 private:
  std::unique_ptr<SmartCardDelegate> delegate_;
};

class SmartCardTest : public ContentBrowserTest {
 public:
  GURL GetIsolatedContextUrl() {
    return embedded_https_test_server().GetURL(
        "a.com",
        "/set-header?Cross-Origin-Opener-Policy: same-origin&"
        "Cross-Origin-Embedder-Policy: require-corp&"
        "Permissions-Policy: smart-card%3D(self)");
  }

  FakeSmartCardDelegate& GetFakeSmartCardDelegate() {
    return *static_cast<FakeSmartCardDelegate*>(
        test_client_->GetSmartCardDelegate());
  }

  void TestEmptyTransaction(std::string expected_result,
                            std::string transaction_callback) {
    ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

    MockSmartCardContextFactory& mock_context_factory =
        GetFakeSmartCardDelegate().mock_context_factory;
    MockSmartCardConnection mock_connection;
    mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

    MockSmartCardTransaction mock_transaction;
    mojo::AssociatedReceiver<SmartCardTransaction> transaction_receiver(
        &mock_transaction);

    {
      InSequence s;

      GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);
      mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);
      mock_connection.ExpectBeginTransaction(transaction_receiver);
      mock_transaction.ExpectEndTransaction(SmartCardDisposition::kReset);
    }

    std::string js_snippet = base::StringPrintf(R"(
      (async () => {
        let context = await navigator.smartCard.establishContext();

        let connection =
          (await context.connect("Fake reader", "shared",
            {preferredProtocols: ["t1"]})).connection;

        let transaction = %s;

        let transactionPromise = connection.startTransaction(transaction);
        try {
          await transactionPromise;
        } catch (e) {
          return `startTransaction: ${e.name}, ${e.message}`;
        }

        return "ok";
      })())",
                                                transaction_callback.c_str());

    EXPECT_EQ(expected_result, EvalJs(shell(), js_snippet));
  }

 private:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    test_client_ = std::make_unique<SmartCardTestContentBrowserClient>();
    test_client_->SetSmartCardDelegate(
        std::make_unique<FakeSmartCardDelegate>());

    // Serve a.com (and any other domain).
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add a handler for the "/set-header" page (among others)
    embedded_https_test_server().AddDefaultHandlers(GetTestDataFilePath());

    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void TearDown() override {
    ASSERT_TRUE(embedded_https_test_server().ShutdownAndWaitUntilComplete());
    ContentBrowserTest::TearDown();
  }

  std::unique_ptr<SmartCardTestContentBrowserClient> test_client_;

  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kSmartCard};
};
}  // namespace

SmartCardTestContentBrowserClient::SmartCardTestContentBrowserClient() =
    default;

SmartCardTestContentBrowserClient::~SmartCardTestContentBrowserClient() =
    default;

SmartCardDelegate* SmartCardTestContentBrowserClient::GetSmartCardDelegate() {
  return delegate_.get();
}

void SmartCardTestContentBrowserClient::SetSmartCardDelegate(
    std::unique_ptr<SmartCardDelegate> delegate) {
  delegate_ = std::move(delegate);
}

bool SmartCardTestContentBrowserClient::ShouldUrlUseApplicationIsolationLevel(
    BrowserContext* browser_context,
    const GURL& url) {
  return true;
}

std::optional<blink::ParsedPermissionsPolicy>
SmartCardTestContentBrowserClient::GetPermissionsPolicyForIsolatedWebApp(
    WebContents* web_contents,
    const url::Origin& app_origin) {
  blink::ParsedPermissionsPolicyDeclaration coi_decl(
      blink::mojom::PermissionsPolicyFeature::kCrossOriginIsolated,
      /*allowed_origins=*/{},
      /*self_if_matches=*/std::nullopt, /*matches_all_origins=*/true,
      /*matches_opaque_src=*/false);
  blink::ParsedPermissionsPolicyDeclaration smart_card_decl(
      blink::mojom::PermissionsPolicyFeature::kSmartCard,
      /*allowed_origins=*/{},
      /*self_if_matches=*/app_origin, /*matches_all_origins=*/false,
      /*matches_opaque_src=*/false);
  return {{coi_decl, smart_card_decl}};
}

mojo::PendingRemote<device::mojom::SmartCardContextFactory>
FakeSmartCardDelegate::GetSmartCardContextFactory(
    BrowserContext& browser_context) {
  return mock_context_factory.GetRemote();
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, Disconnect) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    EXPECT_CALL(mock_connection, Disconnect(SmartCardDisposition::kEject, _))
        .WillOnce([](SmartCardDisposition disposition,
                     SmartCardConnection::DisconnectCallback callback) {
          std::move(callback).Run(
              SmartCardResult::NewSuccess(SmartCardSuccess::kOk));
        });
  }

  EXPECT_EQ(
      "second disconnect: InvalidStateError, Failed to execute 'disconnect' on "
      "'SmartCardConnection': Is disconnected.",
      EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      await connection.disconnect("eject");

      // A second attempt should fail.
      try {
        await connection.disconnect("unpower");
      } catch (e) {
        return `second disconnect: ${e.name}, ${e.message}`;
      }

      return `second disconnect did not throw`;
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ConcurrentDisconnect) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  TestFuture<SmartCardConnection::DisconnectCallback> disconnect_future;

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    EXPECT_CALL(mock_connection, Disconnect(SmartCardDisposition::kEject, _))
        .WillOnce([&disconnect_future](
                      SmartCardDisposition disposition,
                      SmartCardConnection::DisconnectCallback callback) {
          // Ensure this disconnect() call doesn't finish before the second
          // one is issued.
          disconnect_future.SetValue(std::move(callback));
        });
  }

  EXPECT_EQ(
      "second disconnect: InvalidStateError, Failed to execute 'disconnect' on "
      "'SmartCardConnection': An operation is already in progress in this "
      "smart card context.",
      EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      // This first disconnect() call will go through but won't be finished
      // before the end of this script.
      connection.disconnect("eject");

      // A second attempt should fail since the first one is still ongoing.
      try {
        await connection.disconnect("unpower");
      } catch (e) {
        return `second disconnect: ${e.name}, ${e.message}`;
      }

      return `second disconnect did not throw`;
    })())"));

  // Let the first disconnect() finish.
  disconnect_future.Take().Run(
      SmartCardResult::NewSuccess(SmartCardSuccess::kOk));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, Transmit) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    EXPECT_CALL(mock_connection, Transmit(SmartCardProtocol::kT1, _, _))
        .WillOnce([](SmartCardProtocol protocol,
                     const std::vector<uint8_t>& data,
                     SmartCardConnection::TransmitCallback callback) {
          EXPECT_EQ(data, std::vector<uint8_t>({3u, 2u, 1u}));
          std::move(callback).Run(
              device::mojom::SmartCardDataResult::NewData({12u, 34u}));
        });
  }

  EXPECT_EQ("response: 12,34", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let apdu = new Uint8Array([0x03, 0x02, 0x01]);
      let response = await connection.transmit(apdu);

      let responseString = new Uint8Array(response).toString();
      return `response: ${responseString}`;
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, TransmitWithOptions) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    EXPECT_CALL(mock_context_factory,
                Connect(kFakeReader, SmartCardShareMode::kDirect, _, _))
        .WillOnce([&connection_receiver](
                      const std::string& reader,
                      device::mojom::SmartCardShareMode share_mode,
                      device::mojom::SmartCardProtocolsPtr preferred_protocols,
                      SmartCardContext::ConnectCallback callback) {
          EXPECT_FALSE(preferred_protocols->t0);
          EXPECT_FALSE(preferred_protocols->t1);
          EXPECT_FALSE(preferred_protocols->raw);

          auto success = device::mojom::SmartCardConnectSuccess::New(
              connection_receiver.BindNewPipeAndPassRemote(),
              SmartCardProtocol::kUndefined);

          std::move(callback).Run(
              device::mojom::SmartCardConnectResult::NewSuccess(
                  std::move(success)));
        });

    EXPECT_CALL(mock_connection, Transmit(SmartCardProtocol::kT0, _, _))
        .WillOnce([](SmartCardProtocol protocol,
                     const std::vector<uint8_t>& data,
                     SmartCardConnection::TransmitCallback callback) {
          EXPECT_EQ(data, std::vector<uint8_t>({3u, 2u, 1u}));
          std::move(callback).Run(
              device::mojom::SmartCardDataResult::NewData({12u, 34u}));
        });
  }

  EXPECT_EQ("response: 12,34", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "direct")).connection;

      // In real usage you would have in between:
      // let IOCTL_SMARTCARD_SET_PROTOCOL = ...;
      // connection.control(IOCTL_SMARTCARD_SET_PROTOCOL, ...);

      let apdu = new Uint8Array([0x03, 0x02, 0x01]);
      let response = await connection.transmit(apdu, {protocol: "t0"});

      let responseString = new Uint8Array(response).toString();
      return `response: ${responseString}`;
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, TransmitNoProtocol) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  StrictMock<MockSmartCardConnection> mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    EXPECT_CALL(mock_context_factory,
                Connect(kFakeReader, SmartCardShareMode::kDirect, _, _))
        .WillOnce([&connection_receiver](
                      const std::string& reader,
                      device::mojom::SmartCardShareMode share_mode,
                      device::mojom::SmartCardProtocolsPtr preferred_protocols,
                      SmartCardContext::ConnectCallback callback) {
          EXPECT_FALSE(preferred_protocols->t0);
          EXPECT_FALSE(preferred_protocols->t1);
          EXPECT_FALSE(preferred_protocols->raw);

          auto success = device::mojom::SmartCardConnectSuccess::New(
              connection_receiver.BindNewPipeAndPassRemote(),
              SmartCardProtocol::kUndefined);

          std::move(callback).Run(
              device::mojom::SmartCardConnectResult::NewSuccess(
                  std::move(success)));
        });
  }

  EXPECT_THAT(
      EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "direct")).connection;

      let apdu = new Uint8Array([0x03, 0x02, 0x01]);
      try {
        await connection.transmit(apdu);
      } catch(e) {
        return `transmit: ${e.name}, ${e.message}`;
      }

      return "ok";
    })())")
          .ExtractString(),
      MatchesRegex("transmit: InvalidStateError, .*No active protocol\\."));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, Control) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    EXPECT_CALL(mock_connection, Control(42, _, _))
        .WillOnce([](uint32_t control_code, const std::vector<uint8_t>& data,
                     SmartCardConnection::ControlCallback callback) {
          EXPECT_EQ(data, std::vector<uint8_t>({3u, 2u, 1u}));
          std::move(callback).Run(
              device::mojom::SmartCardDataResult::NewData({12u, 34u}));
        });
  }

  EXPECT_EQ("response: 12,34", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let data = new Uint8Array([0x03, 0x02, 0x01]);
      let response = await connection.control(42, data);

      let responseString = new Uint8Array(response).toString();
      return `response: ${responseString}`;
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, GetAttribute) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    EXPECT_CALL(mock_connection, GetAttrib(42, _))
        .WillOnce(
            [](uint32_t tag, SmartCardConnection::GetAttribCallback callback) {
              std::move(callback).Run(
                  device::mojom::SmartCardDataResult::NewData({12u, 34u}));
            });
  }

  EXPECT_EQ("response: 12,34", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let response = await connection.getAttribute(42);

      let responseString = new Uint8Array(response).toString();
      return `response: ${responseString}`;
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, SetAttribute) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    EXPECT_CALL(mock_connection,
                SetAttrib(42, std::vector<uint8_t>({3u, 2u, 1u}), _))
        .WillOnce([](uint32_t tag, const std::vector<uint8_t>& data,
                     SmartCardConnection::SetAttribCallback callback) {
          std::move(callback).Run(device::mojom::SmartCardResult::NewSuccess(
              SmartCardSuccess::kOk));
        });
  }

  EXPECT_EQ("success", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let data = new Uint8Array([0x03, 0x02, 0x01]);
      await connection.setAttribute(42, data);

      return 'success';
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, Status) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    EXPECT_CALL(mock_connection, Status(_))
        .WillOnce([](SmartCardConnection::StatusCallback callback) {
          auto result = device::mojom::SmartCardStatusResult::NewStatus(
              SmartCardStatus::New(
                  kFakeReader, SmartCardConnectionState::kSpecific,
                  SmartCardProtocol::kT1, std::vector<uint8_t>({3u, 2u, 1u})));
          std::move(callback).Run(std::move(result));
        });
  }

  EXPECT_EQ("Fake reader, t1, {3,2,1}", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let status = await connection.status();

      let atr = new Uint8Array(status.answerToReset).toString();
      return `${status.readerName}, ${status.state}, {${atr}}`;
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ListReaders) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  mock_context_factory.ExpectListReaders({"Foo", "Bar"});

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  auto expected_reader_names =
      base::Value(base::Value::List().Append("Foo").Append("Bar"));

  EXPECT_EQ(expected_reader_names, EvalJs(shell(), R"((async () => {
       let context = await navigator.smartCard.establishContext();
       return await context.listReaders();
     })())"));
}

/*
This test checks that in case there are no readers available, listReaders() call
will return an empty list of readers with no errors.

Note that internally we will receive a kNoReadersAvailable error from
SmartCardDelegate. However, we should not forward this error to Javascript.
*/
IN_PROC_BROWSER_TEST_F(SmartCardTest, ListReadersEmpty) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  EXPECT_CALL(mock_context_factory, ListReaders(_))
      .WillOnce(RunOnceCallback<0>(SmartCardListReadersResult::NewError(
          SmartCardError::kNoReadersAvailable)));

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  auto expected_reader_names = base::Value(base::Value::List());

  EXPECT_EQ(expected_reader_names, EvalJs(shell(), R"((async () => {
       let context = await navigator.smartCard.establishContext();
       return await context.listReaders();
     })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, GetStatusChange) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  EXPECT_CALL(mock_context_factory,
              GetStatusChange(base::Milliseconds(4321), _, _))
      .WillOnce(
          [](base::TimeDelta timeout,
             std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
             SmartCardContext::GetStatusChangeCallback callback) {
            ASSERT_EQ(states_in.size(), 1u);
            ASSERT_EQ(states_in[0]->reader, "Fake Reader");
            EXPECT_FALSE(states_in[0]->current_state->unaware);
            EXPECT_FALSE(states_in[0]->current_state->ignore);
            EXPECT_FALSE(states_in[0]->current_state->changed);
            EXPECT_FALSE(states_in[0]->current_state->unknown);
            EXPECT_FALSE(states_in[0]->current_state->unavailable);
            EXPECT_TRUE(states_in[0]->current_state->empty);
            EXPECT_FALSE(states_in[0]->current_state->present);
            EXPECT_FALSE(states_in[0]->current_state->exclusive);
            EXPECT_FALSE(states_in[0]->current_state->inuse);
            EXPECT_FALSE(states_in[0]->current_state->mute);
            EXPECT_FALSE(states_in[0]->current_state->unpowered);
            EXPECT_EQ(states_in[0]->current_count, 6u);

            auto state_flags = SmartCardReaderStateFlags::New();
            state_flags->unaware = false;
            state_flags->ignore = false;
            state_flags->changed = false;
            state_flags->unknown = false;
            state_flags->unavailable = false;
            state_flags->empty = false;
            state_flags->present = true;
            state_flags->exclusive = false;
            state_flags->inuse = true;
            state_flags->mute = false;
            state_flags->unpowered = false;

            std::vector<SmartCardReaderStateOutPtr> states_out;
            states_out.push_back(SmartCardReaderStateOut::New(
                "Fake Reader", std::move(state_flags), 7,
                std::vector<uint8_t>({1u, 2u, 3u, 4u})));
            auto result =
                device::mojom::SmartCardStatusChangeResult::NewReaderStates(
                    std::move(states_out));
            std::move(callback).Run(std::move(result));
          });

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ(
      "Fake Reader, {ignore=false, changed=false, "
      "unknown=false, unavailable=false, empty=false, present=true, "
      "exclusive=false, inuse=true, mute=false, unpowered=false}, 7, {1,2,3,4}",
      EvalJs(shell(), R"((async () => {
       let context = await navigator.smartCard.establishContext();

       let readerStates = [{readerName: "Fake Reader",
                            currentState: {empty: true},
                            currentCount: 6 }];
       let statesOut = await context.getStatusChange(
           readerStates,
           {timeout: 4321});

       if (statesOut.length !== 1) {
         return `states array has size ${statesOut.length}`;
       }
       let atrString = new Uint8Array(statesOut[0].answerToReset).toString();

       let flags = statesOut[0].eventState;
       let eventStateString = `ignore=${flags.ignore}`
           + `, changed=${flags.changed}`
           + `, unknown=${flags.unknown}`
           + `, unavailable=${flags.unavailable}`
           + `, empty=${flags.empty}`
           + `, present=${flags.present}`
           + `, exclusive=${flags.exclusive}`
           + `, inuse=${flags.inuse}`
           + `, mute=${flags.mute}`
           + `, unpowered=${flags.unpowered}`;

       return `${statesOut[0].readerName}, {${eventStateString}}` +
         `, ${statesOut[0].eventCount}, {${atrString}}`;
     })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, GetStatusChangeAborted) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  base::test::TestFuture<SmartCardContext::GetStatusChangeCallback>
      get_status_callback;

  {
    InSequence s;

    EXPECT_CALL(mock_context_factory,
                GetStatusChange(base::TimeDelta::Max(), _, _))
        .WillOnce(
            [&get_status_callback](
                base::TimeDelta timeout,
                std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
                SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), size_t(1));
              ASSERT_EQ(states_in[0]->reader, "Fake Reader");
              EXPECT_FALSE(states_in[0]->current_state->unaware);
              EXPECT_FALSE(states_in[0]->current_state->ignore);
              EXPECT_FALSE(states_in[0]->current_state->changed);
              EXPECT_FALSE(states_in[0]->current_state->unknown);
              EXPECT_FALSE(states_in[0]->current_state->unavailable);
              EXPECT_TRUE(states_in[0]->current_state->empty);
              EXPECT_FALSE(states_in[0]->current_state->present);
              EXPECT_FALSE(states_in[0]->current_state->exclusive);
              EXPECT_FALSE(states_in[0]->current_state->inuse);
              EXPECT_FALSE(states_in[0]->current_state->mute);
              EXPECT_FALSE(states_in[0]->current_state->unpowered);

              // Don't respond immediately.
              get_status_callback.SetValue(std::move(callback));
            });

    // Aborting a blink context.getStatusChange() call means sending a Cancel()
    // request down to device.mojom.
    EXPECT_CALL(mock_context_factory, Cancel(_))
        .WillOnce(
            [&get_status_callback](SmartCardContext::CancelCallback callback) {
              std::move(get_status_callback)
                  .Take()
                  .Run(device::mojom::SmartCardStatusChangeResult::NewError(
                      SmartCardError::kCancelled));

              std::move(callback).Run(
                  SmartCardResult::NewSuccess(SmartCardSuccess::kOk));
            });
  }

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ("Exception: Error, Something", EvalJs(shell(), R"((async () => {
       let context = await navigator.smartCard.establishContext();

       let abortController = new AbortController();

       let getStatusPromise = context.getStatusChange(
           [{readerName: "Fake Reader", currentState: {empty: true}}],
           {signal: abortController.signal});

       abortController.abort(Error("Something"));

       try {
         let result = await getStatusPromise;
         return "Success";
       } catch (e) {
         return `Exception: ${e.name}, ${e.message}`;
       }
     })())"));
}

// Tests passing an AbortSignal to getStatusChange() that is already aborted.
IN_PROC_BROWSER_TEST_F(SmartCardTest, GetStatusChangeAlreadyAborted) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ("Exception: Error, Something", EvalJs(shell(), R"((async () => {
       let context = await navigator.smartCard.establishContext();

       let getStatusPromise = context.getStatusChange(
           [{readerName: "Fake Reader", currentState: {empty: true}}],
           {signal: AbortSignal.abort(Error("Something"))});

       try {
         let result = await getStatusPromise;
         return "Success";
       } catch (e) {
         return `Exception: ${e.name}, ${e.message}`;
       }
     })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, Connect) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  EXPECT_CALL(mock_context_factory,
              Connect(kFakeReader, SmartCardShareMode::kShared, _, _))
      .WillOnce([](const std::string& reader,
                   device::mojom::SmartCardShareMode share_mode,
                   device::mojom::SmartCardProtocolsPtr preferred_protocols,
                   SmartCardContext::ConnectCallback callback) {
        mojo::PendingRemote<device::mojom::SmartCardConnection> pending_remote;

        EXPECT_TRUE(preferred_protocols->t0);
        EXPECT_TRUE(preferred_protocols->t1);
        EXPECT_FALSE(preferred_protocols->raw);

        mojo::MakeSelfOwnedReceiver(
            std::make_unique<MockSmartCardConnection>(),
            pending_remote.InitWithNewPipeAndPassReceiver());

        auto success = device::mojom::SmartCardConnectSuccess::New(
            std::move(pending_remote), SmartCardProtocol::kT1);

        std::move(callback).Run(
            device::mojom::SmartCardConnectResult::NewSuccess(
                std::move(success)));
      });

  GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  auto expected_reader_names =
      base::Value(base::Value::List().Append("Foo").Append("Bar"));

  EXPECT_EQ("[object SmartCardConnection], t1", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();
      let result = await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t0", "t1"]});
      return `${result.connection}, ${result.activeProtocol}`;
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ConnectDenied) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  EXPECT_CALL(mock_context_factory, Connect(_, _, _, _)).Times(0);

  {
    InSequence s;

    mock_context_factory.ExpectListReaders({kFakeReader});

    // No permission yet. So renderer will have to request it.
    EXPECT_CALL(GetFakeSmartCardDelegate(), HasReaderPermission(_, kFakeReader))
        .WillOnce(Return(false));

    // Permission was requested and it got denied.
    EXPECT_CALL(GetFakeSmartCardDelegate(),
                RequestReaderPermission(_, kFakeReader, _))
        .WillOnce(RunOnceCallback<2>(false));
  }

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ("NotAllowedError", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();
      let readers = await context.listReaders();
      try {
        let result = await context.connect(readers[0], "shared",
            {preferredProtocols: ["t0", "t1"]});
      } catch (e) {
        return e.name;
      }
      return "ok";
    })())"));
}

// Tests that a connection request is immediately denied if the application
// passes a reader name string that is not known to have come from the smart
// card API.
// This is to avoid presenting unfiltered strings to the user in a permission
// prompt.
IN_PROC_BROWSER_TEST_F(SmartCardTest, ConnectDeniedUnknownString) {
  constexpr char kMyDisturbingString[] = "my disturbing string";

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  // The connection request shall not go through.
  EXPECT_CALL(mock_context_factory, Connect(_, _, _, _)).Times(0);

  // We tell that there's no permission yet.
  EXPECT_CALL(GetFakeSmartCardDelegate(),
              HasReaderPermission(_, kMyDisturbingString))
      .WillOnce(Return(false));

  // But the permission should not be requested as the reader name string is
  // unknown.
  EXPECT_CALL(GetFakeSmartCardDelegate(),
              RequestReaderPermission(_, kMyDisturbingString, _))
      .Times(0);

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ("NotAllowedError", EvalJs(shell(), JsReplace(R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();
      try {
        let result = await context.connect($1, "shared",
            {preferredProtocols: ["t0", "t1"]});
      } catch (e) {
        return e.name;
      }
      return "ok";
    })())",
                                                         kMyDisturbingString)));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ConnectPermissionGranted) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  {
    InSequence s;

    mock_context_factory.ExpectListReaders({kFakeReader});

    // No permission yet. So renderer will have to request it.
    EXPECT_CALL(GetFakeSmartCardDelegate(), HasReaderPermission(_, kFakeReader))
        .WillOnce(Return(false));

    // Permission was requested and granted.
    EXPECT_CALL(GetFakeSmartCardDelegate(),
                RequestReaderPermission(_, kFakeReader, _))
        .WillOnce(RunOnceCallback<2>(true));

    // The Connect request will then finally go through.
    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);
  }

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ("ok", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();
      let readers = await context.listReaders();
      try {
        let result = await context.connect(readers[0], "shared",
            {preferredProtocols: ["t1"]});
      } catch (e) {
        return `${e.name}, ${e.message}`;
      }
      return "ok";
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, StartTransaction) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  MockSmartCardTransaction mock_transaction;
  mojo::AssociatedReceiver<SmartCardTransaction> transaction_receiver(
      &mock_transaction);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    mock_connection.ExpectBeginTransaction(transaction_receiver);

    EXPECT_CALL(mock_connection, Transmit(SmartCardProtocol::kT1, _, _))
        .WillOnce([](SmartCardProtocol protocol,
                     const std::vector<uint8_t>& data,
                     SmartCardConnection::TransmitCallback callback) {
          EXPECT_EQ(data, std::vector<uint8_t>({3u, 2u, 1u}));
          std::move(callback).Run(
              device::mojom::SmartCardDataResult::NewData({12u, 34u}));
        });

    mock_transaction.ExpectEndTransaction(SmartCardDisposition::kReset);
  }

  EXPECT_EQ("ok", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let transaction = async () => {
        let apdu = new Uint8Array([0x03, 0x02, 0x01]);
        await connection.transmit(apdu);
        return "reset";
      }

      await connection.startTransaction(transaction);

      return "ok";
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, StartTransactionAborted) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  StrictMock<MockSmartCardConnection> mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  base::test::TestFuture<SmartCardConnection::BeginTransactionCallback>
      begin_transaction_callback;

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    EXPECT_CALL(mock_connection, BeginTransaction(_))
        .WillOnce([&begin_transaction_callback](
                      SmartCardConnection::BeginTransactionCallback callback) {
          // Don't respond immediately.
          begin_transaction_callback.SetValue(std::move(callback));
        });

    // Aborting a blink connection.startTransaction() call means sending a
    // Cancel() request down to device.mojom.SmartCardContext
    EXPECT_CALL(mock_context_factory, Cancel(_))
        .WillOnce([&begin_transaction_callback](
                      SmartCardContext::CancelCallback callback) {
          begin_transaction_callback.Take().Run(
              device::mojom::SmartCardTransactionResult::NewError(
                  SmartCardError::kCancelled));

          std::move(callback).Run(
              SmartCardResult::NewSuccess(SmartCardSuccess::kOk));
        });
  }

  EXPECT_EQ("Exception: Error, Something", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let transaction = async () => {
        let apdu = new Uint8Array([0x03, 0x02, 0x01]);
        await connection.transmit(apdu);
        return "reset";
      }

      let abortController = new AbortController();

      let promise =
          connection.startTransaction(transaction,
              {signal: abortController.signal});

      abortController.abort(Error("Something"));

      try {
        await promise;
        return "Success";
      } catch (e) {
        return `Exception: ${e.name}, ${e.message}`;
      }
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, TransactionCallbackRejects) {
  TestEmptyTransaction("startTransaction: Error, Oops!", R"(
      async () => { throw new Error('Oops!'); }
    )");
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, TransactionCallbackThrows) {
  TestEmptyTransaction("startTransaction: Error, Oops!", R"(
      () => { throw new Error('Oops!'); }
    )");
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, TransactionCallbackReturnsEmptyPromise) {
  TestEmptyTransaction("ok", R"( async () => {} )");
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, TransactionCallbackReturnsNothing) {
  TestEmptyTransaction("ok", R"( () => {} )");
}

// A transaction callback must return a SmartCardDisposition.
// Check that if the callback returns something else, startTransaction() returns
// an appropriate error.
IN_PROC_BROWSER_TEST_F(SmartCardTest, TransactionCallbackReturnsInvalidValue) {
  TestEmptyTransaction(
      "startTransaction: TypeError, Failed to execute 'startTransaction' on "
      "'SmartCardConnection': The provided value '[object Object]' is not a "
      "valid enum value of type SmartCardDisposition.",
      R"(
      async () => {
        // Return some random object instead of a SmartCardDisposition
        return {foo: 'bar', hello: 42};
      }
      )");
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, EndTransactionFails) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  MockSmartCardTransaction mock_transaction;
  mojo::AssociatedReceiver<SmartCardTransaction> transaction_receiver(
      &mock_transaction);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);
    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);
    mock_connection.ExpectBeginTransaction(transaction_receiver);

    EXPECT_CALL(mock_transaction,
                EndTransaction(SmartCardDisposition::kEject, _))
        .WillOnce([](SmartCardDisposition disposition,
                     SmartCardTransaction::EndTransactionCallback callback) {
          std::move(callback).Run(
              SmartCardResult::NewError(SmartCardError::kResetCard));
        });
  }

  EXPECT_EQ(
      "startTransaction: SmartCardError, The smart card has been reset, so any "
      "shared state information is invalid.",
      EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let transaction = async () => {
        return "eject";
      }

      transactionPromise = connection.startTransaction(transaction);
      try {
        await transactionPromise;
      } catch (e) {
        return `startTransaction: ${e.name}, ${e.message}`;
      }

      return "startTransaction did not throw";
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, DisconnectedOnTransactionReturn) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  StrictMock<MockSmartCardTransaction> mock_transaction;
  mojo::AssociatedReceiver<SmartCardTransaction> transaction_receiver(
      &mock_transaction);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);
    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);
    mock_connection.ExpectBeginTransaction(transaction_receiver);

    EXPECT_CALL(mock_connection, Disconnect(SmartCardDisposition::kLeave, _))
        .WillOnce([](SmartCardDisposition disposition,
                     SmartCardConnection::DisconnectCallback callback) {
          std::move(callback).Run(
              SmartCardResult::NewSuccess(SmartCardSuccess::kOk));
        });
  }

  EXPECT_EQ(
      "startTransaction: InvalidStateError, Failed to execute "
      "'startTransaction' on 'SmartCardConnection': Cannot end transaction "
      "with an invalid connection.",
      EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let transaction = async () => {
        await connection.disconnect();
        return "eject";
      }

      transactionPromise = connection.startTransaction(transaction);
      try {
        await transactionPromise;
      } catch (e) {
        return `startTransaction: ${e.name}, ${e.message}`;
      }

      return "startTransaction did not throw";
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, OngoingTransmitOnTransactionReturn) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  StrictMock<MockSmartCardTransaction> mock_transaction;
  mojo::AssociatedReceiver<SmartCardTransaction> transaction_receiver(
      &mock_transaction);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);
    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);
    mock_connection.ExpectBeginTransaction(transaction_receiver);

    EXPECT_CALL(mock_connection, Transmit(SmartCardProtocol::kT1, _, _))
        .WillOnce([](SmartCardProtocol protocol,
                     const std::vector<uint8_t>& data,
                     SmartCardConnection::TransmitCallback callback) {
          EXPECT_EQ(data, std::vector<uint8_t>({3u, 2u, 1u}));
          std::move(callback).Run(
              device::mojom::SmartCardDataResult::NewData({12u, 34u}));
        });

    mock_transaction.ExpectEndTransaction(SmartCardDisposition::kEject);
  }

  EXPECT_EQ(
      "startTransaction: InvalidStateError, Transaction callback returned "
      "while an operation was still in progress.",
      EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let transaction = async () => {
        // Return before the transmit() completes.
        let apdu = new Uint8Array([0x03, 0x02, 0x01]);
        connection.transmit(apdu);
        return "eject";
      }

      transactionPromise = connection.startTransaction(transaction);
      try {
        await transactionPromise;
      } catch (e) {
        return `startTransaction: ${e.name}, ${e.message}`;
      }
      return "ok";
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest,
                       ContextOperationBlocksConnectionOperation) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  StrictMock<MockSmartCardConnection> mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  TestFuture<SmartCardContext::ListReadersCallback> list_readers_callback;

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    EXPECT_CALL(mock_context_factory, ListReaders(_))
        .WillOnce([&list_readers_callback](
                      SmartCardContext::ListReadersCallback callback) {
          // Don't respond immediately.
          list_readers_callback.SetValue(std::move(callback));
        });
  }

  EXPECT_EQ(
      "control: InvalidStateError, Failed to execute 'control' on "
      "'SmartCardConnection': An operation is already in progress in this "
      "smart card context.",
      EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let listReadersPromise = context.listReaders();

      try {
        let data = new Uint8Array([0x03, 0x02, 0x01]);
        await connection.control(42, data);
      } catch (e) {
        return `control: ${e.name}, ${e.message}`;
      }

      await listReadersPromise;

      return `ok`;
    })())"));

  // Let context.listReaders() conclude
  list_readers_callback.Take().Run(
      SmartCardListReadersResult::NewReaders({kFakeReader}));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ConnectionDiesWithOperationInProgress) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  StrictMock<MockSmartCardConnection> mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    EXPECT_CALL(mock_connection, Control(42, _, _))
        .WillOnce([&connection_receiver](
                      uint32_t control_code, const std::vector<uint8_t>& data,
                      SmartCardConnection::ControlCallback callback) {
          connection_receiver.reset();
        });
  }

  EXPECT_EQ(
      "control: InvalidStateError, Failed to execute 'control' on "
      "'SmartCardConnection': Is disconnected.",
      EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      try {
        let data = new Uint8Array([0x03, 0x02, 0x01]);
        await connection.control(42, data);
      } catch (e) {
        return `control: ${e.name}, ${e.message}`;
      }
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ContextDiesConnectionStays) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  StrictMock<MockSmartCardConnection> mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    EXPECT_CALL(mock_connection, Control(42, _, _))
        .WillOnce([&mock_context_factory](
                      uint32_t control_code, const std::vector<uint8_t>& data,
                      SmartCardConnection::ControlCallback callback) {
          mock_context_factory.ClearContextReceivers();
          std::move(callback).Run(
              device::mojom::SmartCardDataResult::NewData({12u, 34u}));
        });
  }

  EXPECT_EQ("response: 12,34", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let data = new Uint8Array([0x03, 0x02, 0x01]);
      let response = await connection.control(42, data);

      let responseString = new Uint8Array(response).toString();
      return `response: ${responseString}`;
    })())"));
}

// A ContentBrowserClient that grants Isolated Web Apps the "smart-card"
// permission, but not "cross-origin-isolated", which should result in Smart
// Cards being disabled.
class NoCoiPermissionSmartCardTestContentBrowserClient
    : public SmartCardTestContentBrowserClient {
 public:
  std::optional<blink::ParsedPermissionsPolicy>
  GetPermissionsPolicyForIsolatedWebApp(
      WebContents* web_contents,
      const url::Origin& app_origin) override {
    return {{blink::ParsedPermissionsPolicyDeclaration(
        blink::mojom::PermissionsPolicyFeature::kSmartCard,
        /*allowed_origins=*/{},
        /*self_if_matches=*/app_origin,
        /*matches_all_origins=*/false, /*matches_opaque_src=*/false)}};
  }
};

IN_PROC_BROWSER_TEST_F(SmartCardTest, NoCoiPermission) {
  NoCoiPermissionSmartCardTestContentBrowserClient client;
  client.SetSmartCardDelegate(std::make_unique<FakeSmartCardDelegate>());

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ(false, EvalJs(shell(), "self.crossOriginIsolated"));
  EXPECT_THAT(
      EvalJs(shell(), "navigator.smartCard.establishContext()").error,
      HasSubstr("Frame is not sufficiently isolated to use smart cards."));
}

/* Tests the situation where a transaction callback erroneously returns while an
 * operation in this connection is ongoing. If that operation fails at PC/SC
 * level the Web API implementation should still cleanup after itself by ending
 * the PC/SC transaction once that operation completes.
 */
IN_PROC_BROWSER_TEST_F(SmartCardTest, EndTransactionAfterFailedOperation) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  MockSmartCardTransaction mock_transaction;
  mojo::AssociatedReceiver<SmartCardTransaction> transaction_receiver(
      &mock_transaction);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    mock_connection.ExpectBeginTransaction(transaction_receiver);

    EXPECT_CALL(mock_connection, Status(_))
        .WillOnce([](SmartCardConnection::StatusCallback callback) {
          // Simulate a PC/SC failure.
          auto result = device::mojom::SmartCardStatusResult::NewError(
              SmartCardError::kReaderUnavailable);
          std::move(callback).Run(std::move(result));
        });

    mock_transaction.ExpectEndTransaction(SmartCardDisposition::kReset);
  }

  EXPECT_EQ(
      "startTransaction: InvalidStateError, Transaction callback returned "
      "while an operation was still in progress.",
      EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let transaction = () => {
        connection.status();
      };

      try {
        await connection.startTransaction(transaction);
      } catch (e) {
        return `startTransaction: ${e.name}, ${e.message}`;
      }

      return "ok";
    })())"));
}

/* Tests the situation where a transaction callback erroneously returns while a
 * SmartCardContext operation is ongoing. The Web API implementation should
 * cleanup after itself by ending the PC/SC transaction once that operation
 * completes.
 */
IN_PROC_BROWSER_TEST_F(SmartCardTest, EndTransactionAfterContextOperation) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  MockSmartCardTransaction mock_transaction;
  mojo::AssociatedReceiver<SmartCardTransaction> transaction_receiver(
      &mock_transaction);

  {
    InSequence s;

    GetFakeSmartCardDelegate().ExpectHasReaderPermission(kFakeReader);

    mock_context_factory.ExpectConnectFakeReaderSharedT1(connection_receiver);

    mock_connection.ExpectBeginTransaction(transaction_receiver);

    mock_context_factory.ExpectListReaders({"Foo", "Bar"});

    mock_transaction.ExpectEndTransaction(SmartCardDisposition::kEject);
  }

  EXPECT_EQ(
      "startTransaction: InvalidStateError, Transaction callback returned "
      "while an operation was still in progress.",
      EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection =
        (await context.connect("Fake reader", "shared",
          {preferredProtocols: ["t1"]})).connection;

      let transaction = () => {
        context.listReaders();
        return "eject";
      };

      try {
        await connection.startTransaction(transaction);
      } catch (e) {
        return `startTransaction: ${e.name}, ${e.message}`;
      }

      return "ok";
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, EstablishContextDenied) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_CALL(GetFakeSmartCardDelegate(), IsPermissionBlocked(_))
      .WillOnce(Return(true));

  EXPECT_EQ("NotAllowedError", EvalJs(shell(), R"((async () => {
      try {
         let context = await navigator.smartCard.establishContext();
      } catch (e) {
         return e.name;
      }
      return "ok";
     })())"));
}

}  // namespace content
