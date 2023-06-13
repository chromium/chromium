// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base_switches.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/smart_card/mock_smart_card_context_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/smart_card_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
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

using base::test::TestFuture;
using device::mojom::SmartCardConnection;
using device::mojom::SmartCardContext;
using device::mojom::SmartCardDisposition;
using device::mojom::SmartCardError;
using device::mojom::SmartCardProtocol;
using device::mojom::SmartCardReaderStateFlags;
using device::mojom::SmartCardReaderStateOut;
using device::mojom::SmartCardReaderStateOutPtr;
using device::mojom::SmartCardResult;
using device::mojom::SmartCardShareMode;
using device::mojom::SmartCardSuccess;
using ::testing::_;
using testing::Exactly;
using testing::InSequence;

namespace content {

namespace {

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
};

class FakeSmartCardDelegate : public SmartCardDelegate {
 public:
  FakeSmartCardDelegate() = default;
  // SmartCardDelegate overrides:
  mojo::PendingRemote<device::mojom::SmartCardContextFactory>
  GetSmartCardContextFactory(BrowserContext& browser_context) override;
  bool SupportsReaderAddedRemovedNotifications() const override { return true; }

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
  SmartCardDelegate* GetSmartCardDelegate(
      content::BrowserContext* browser_context) override;
  bool ShouldUrlUseApplicationIsolationLevel(BrowserContext* browser_context,
                                             const GURL& url) override;
  absl::optional<blink::ParsedPermissionsPolicy>
  GetPermissionsPolicyForIsolatedWebApp(
      content::BrowserContext* browser_context,
      const url::Origin& app_origin) override;

 private:
  std::unique_ptr<SmartCardDelegate> delegate_;
};

class SmartCardTest : public ContentBrowserTest {
 public:
  GURL GetIsolatedContextUrl() {
    return https_server_.GetURL(
        "a.com",
        "/set-header?Cross-Origin-Opener-Policy: same-origin&"
        "Cross-Origin-Embedder-Policy: require-corp&"
        "Permissions-Policy: smart-card%3D(self)");
  }

  FakeSmartCardDelegate& GetFakeSmartCardDelegate() {
    return *static_cast<FakeSmartCardDelegate*>(
        test_client_->GetSmartCardDelegate(nullptr));
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    test_client_ = std::make_unique<SmartCardTestContentBrowserClient>();
    test_client_->SetSmartCardDelegate(
        std::make_unique<FakeSmartCardDelegate>());

    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    // Serve a.com (and any other domain).
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add a handler for the "/set-header" page (among others)
    https_server_.AddDefaultHandlers(GetTestDataFilePath());

    ASSERT_TRUE(https_server_.Start());
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void TearDown() override {
    ASSERT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
    ContentBrowserTest::TearDown();
  }

  std::unique_ptr<SmartCardTestContentBrowserClient> test_client_;

  // Need a mock CertVerifier for HTTPS connections to succeed with the test
  // server.
  ContentMockCertVerifier mock_cert_verifier_;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kSmartCard};
};
}  // namespace

SmartCardTestContentBrowserClient::SmartCardTestContentBrowserClient() =
    default;

SmartCardTestContentBrowserClient::~SmartCardTestContentBrowserClient() =
    default;

SmartCardDelegate* SmartCardTestContentBrowserClient::GetSmartCardDelegate(
    content::BrowserContext* browser_context) {
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

absl::optional<blink::ParsedPermissionsPolicy>
SmartCardTestContentBrowserClient::GetPermissionsPolicyForIsolatedWebApp(
    content::BrowserContext* browser_context,
    const url::Origin& app_origin) {
  blink::ParsedPermissionsPolicy out;
  blink::ParsedPermissionsPolicyDeclaration decl(
      blink::mojom::PermissionsPolicyFeature::kSmartCard,
      /*allowed_origins=*/{},
      /*self_if_matches=*/app_origin, /*matches_all_origins=*/false,
      /*matches_opaque_src=*/false);
  out.push_back(decl);
  return out;
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

      let connection = await context.connect("Fake reader", "shared", ["t1"]);

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
      "'SmartCardConnection': An operation is in progress.",
      EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();

      let connection = await context.connect("Fake reader", "shared", ["t1"]);

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

      let connection = await context.connect("Fake reader", "shared", ["t1"]);

      let apdu = new Uint8Array([0x03, 0x02, 0x01]);
      let response = await connection.transmit(apdu);

      let responseString = new Uint8Array(response).toString();
      return `response: ${responseString}`;
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, Control) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  MockSmartCardConnection mock_connection;
  mojo::Receiver<SmartCardConnection> connection_receiver(&mock_connection);

  {
    InSequence s;

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

      let connection = await context.connect("Fake reader", "shared", ["t1"]);

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

      let connection = await context.connect("Fake reader", "shared", ["t1"]);

      let response = await connection.getAttribute(42);

      let responseString = new Uint8Array(response).toString();
      return `response: ${responseString}`;
    })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ListReaders) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  EXPECT_CALL(mock_context_factory, ListReaders(_))
      .WillOnce([](SmartCardContext::ListReadersCallback callback) {
        std::vector<std::string> readers{"Foo", "Bar"};
        auto result =
            device::mojom::SmartCardListReadersResult::NewReaders(readers);
        std::move(callback).Run(std::move(result));
      });

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
      .WillOnce([](SmartCardContext::ListReadersCallback callback) {
        auto result = device::mojom::SmartCardListReadersResult::NewError(
            SmartCardError::kNoReadersAvailable);
        std::move(callback).Run(std::move(result));
      });

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
              GetStatusChange(base::TimeDelta::Max(), _, _))
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
                "Fake Reader", std::move(state_flags),
                std::vector<uint8_t>({1u, 2u, 3u, 4u})));
            auto result =
                device::mojom::SmartCardStatusChangeResult::NewReaderStates(
                    std::move(states_out));
            std::move(callback).Run(std::move(result));
          });

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ(
      "Fake Reader, {unaware=false, ignore=false, changed=false, "
      "unknown=false, unavailable=false, empty=false, present=true, "
      "exclusive=false, inuse=true, mute=false, unpowered=false}, {1,2,3,4}",
      EvalJs(shell(), R"((async () => {
       let context = await navigator.smartCard.establishContext();

       let readerStates = [{readerName: "Fake Reader",
                            currentState: {empty: true}}];
       let statesOut = await context.getStatusChange(
           readerStates,
           AbortSignal.timeout(4321));

       if (statesOut.length !== 1) {
         return `states array has size ${statesOut.length}`;
       }
       let atrString = new Uint8Array(statesOut[0].answerToReset).toString();

       let flags = statesOut[0].eventState;
       let eventStateString = `unaware=${flags.unaware}`
           + `, ignore=${flags.ignore}`
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
         `, {${atrString}}`;
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

  EXPECT_EQ("Exception: AbortError", EvalJs(shell(), R"((async () => {
       let context = await navigator.smartCard.establishContext();

       let abortController = new AbortController();

       let getStatusPromise = context.getStatusChange(
           [{readerName: "Fake Reader", currentState: {empty: true}}],
           abortController.signal);

       abortController.abort();

       try {
         let result = await getStatusPromise;
         return "Success";
       } catch (e) {
         return `Exception: ${e.name}`;
       }
     })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, Connect) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  EXPECT_CALL(mock_context_factory,
              Connect("Fake reader", SmartCardShareMode::kShared, _, _))
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

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  auto expected_reader_names =
      base::Value(base::Value::List().Append("Foo").Append("Bar"));

  EXPECT_EQ("[object SmartCardConnection]", EvalJs(shell(), R"(
    (async () => {
      let context = await navigator.smartCard.establishContext();
      let connection = await context.connect("Fake reader", "shared",
          ["t0", "t1"]);
      return `${connection}`;
    })())"));
}

}  // namespace content
