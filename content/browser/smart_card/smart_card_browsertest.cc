// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base_switches.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/smart_card/mock_smart_card_context_factory.h"
#include "content/browser/smart_card/smart_card_reader_tracker.h"
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

using device::mojom::SmartCardContext;
using device::mojom::SmartCardError;
using device::mojom::SmartCardProtocol;
using device::mojom::SmartCardReaderStateFlags;
using device::mojom::SmartCardReaderStateOut;
using device::mojom::SmartCardReaderStateOutPtr;
using device::mojom::SmartCardShareMode;
using ::testing::_;
using testing::Exactly;
using testing::InSequence;

namespace content {

namespace {

class MockSmartCardConnection : public device::mojom::SmartCardConnection {
 public:
  MOCK_METHOD(void,
              Disconnect,
              (device::mojom::SmartCardDisposition disposition,
               DisconnectCallback callback),
              (override));
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

class MockSmartCardReaderTracker : public SmartCardReaderTracker {
 public:
  MOCK_METHOD(void, Start, (Observer * observer, StartCallback), (override));
  MOCK_METHOD(void, Stop, (Observer * observer), (override));

  ObserverList observer_list;
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

  MockSmartCardReaderTracker& CreateMockSmartCardReaderTracker() {
    BrowserContext* browser_context =
        shell()->web_contents()->GetBrowserContext();

    auto unique_tracker = std::make_unique<MockSmartCardReaderTracker>();
    MockSmartCardReaderTracker& mock_tracker = *unique_tracker.get();
    browser_context->SetUserData(
        SmartCardReaderTracker::user_data_key_for_testing(),
        std::move(unique_tracker));

    return mock_tracker;
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

IN_PROC_BROWSER_TEST_F(SmartCardTest, GetReaders) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  {
    InSequence s;

    // Request what readers are currently available.
    EXPECT_CALL(mock_context_factory, ListReaders(_))
        .WillOnce([](SmartCardContext::ListReadersCallback callback) {
          std::vector<std::string> readers{"Fake Reader"};
          auto result =
              device::mojom::SmartCardListReadersResult::NewReaders(readers);
          std::move(callback).Run(std::move(result));
        });

    // Request the state of each of those readers.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), size_t(1));
              ASSERT_EQ(states_in[0]->reader, "Fake Reader");
              EXPECT_TRUE(states_in[0]->current_state->unaware);
              EXPECT_FALSE(states_in[0]->current_state->ignore);
              EXPECT_FALSE(states_in[0]->current_state->changed);
              EXPECT_FALSE(states_in[0]->current_state->unknown);
              EXPECT_FALSE(states_in[0]->current_state->unavailable);
              EXPECT_FALSE(states_in[0]->current_state->empty);
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
              state_flags->empty = true;
              state_flags->present = false;
              state_flags->exclusive = false;
              state_flags->inuse = false;
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

    // Request to be notified of state changes on those readers and on the
    // addition of a new reader.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), size_t(2));

              EXPECT_EQ(states_in[0]->reader, R"(\\?PnP?\Notification)");
              EXPECT_FALSE(states_in[0]->current_state->unaware);
              EXPECT_FALSE(states_in[0]->current_state->ignore);
              EXPECT_FALSE(states_in[0]->current_state->changed);
              EXPECT_FALSE(states_in[0]->current_state->unknown);
              EXPECT_FALSE(states_in[0]->current_state->unavailable);
              EXPECT_FALSE(states_in[0]->current_state->empty);
              EXPECT_FALSE(states_in[0]->current_state->present);
              EXPECT_FALSE(states_in[0]->current_state->exclusive);
              EXPECT_FALSE(states_in[0]->current_state->inuse);
              EXPECT_FALSE(states_in[0]->current_state->mute);
              EXPECT_FALSE(states_in[0]->current_state->unpowered);

              EXPECT_EQ(states_in[1]->reader, "Fake Reader");
              EXPECT_FALSE(states_in[1]->current_state->unaware);
              EXPECT_FALSE(states_in[1]->current_state->ignore);
              EXPECT_FALSE(states_in[1]->current_state->changed);
              EXPECT_FALSE(states_in[1]->current_state->unknown);
              EXPECT_FALSE(states_in[1]->current_state->unavailable);
              EXPECT_TRUE(states_in[1]->current_state->empty);
              EXPECT_FALSE(states_in[1]->current_state->present);
              EXPECT_FALSE(states_in[1]->current_state->exclusive);
              EXPECT_FALSE(states_in[1]->current_state->inuse);
              EXPECT_FALSE(states_in[1]->current_state->mute);
              EXPECT_FALSE(states_in[1]->current_state->unpowered);

              // Fail so that SmartCardReaderTracker stops requesting.
              std::move(callback).Run(
                  device::mojom::SmartCardStatusChangeResult::NewError(
                      SmartCardError::kNoService));
            });
  }

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ(true, EvalJs(shell(),
                         R"((async () => {
               let readers = await navigator.smartCard.getReaders();

               return readers.length == 1 && readers[0].name == "Fake Reader";
             })())"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ReaderAdd) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  base::test::TestFuture<SmartCardContext::GetStatusChangeCallback>
      first_get_status_callback;

  {
    InSequence s;

    // Request what readers are currently available.
    // There are none.
    EXPECT_CALL(mock_context_factory, ListReaders(_))
        .WillOnce([](SmartCardContext::ListReadersCallback callback) {
          auto result = device::mojom::SmartCardListReadersResult::NewReaders(
              std::vector<std::string>());
          std::move(callback).Run(std::move(result));
        });

    // Request to be notified of on the addition of a new reader.
    // Don't respond immediately.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [&first_get_status_callback](
                base::TimeDelta timeout,
                std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
                SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), size_t(1));

              EXPECT_EQ(states_in[0]->reader, R"(\\?PnP?\Notification)");
              EXPECT_FALSE(states_in[0]->current_state->unaware);
              EXPECT_FALSE(states_in[0]->current_state->ignore);
              EXPECT_FALSE(states_in[0]->current_state->changed);
              EXPECT_FALSE(states_in[0]->current_state->unknown);
              EXPECT_FALSE(states_in[0]->current_state->unavailable);
              EXPECT_FALSE(states_in[0]->current_state->empty);
              EXPECT_FALSE(states_in[0]->current_state->present);
              EXPECT_FALSE(states_in[0]->current_state->exclusive);
              EXPECT_FALSE(states_in[0]->current_state->inuse);
              EXPECT_FALSE(states_in[0]->current_state->mute);
              EXPECT_FALSE(states_in[0]->current_state->unpowered);

              first_get_status_callback.SetValue(std::move(callback));
            });
    // Once the previous GetStatusChange is answered, the SmartCardReaderTracker
    // will start over from ListReader again.
    // Fail so that it stops requesting.
    EXPECT_CALL(mock_context_factory, ListReaders(_))
        .WillOnce([](SmartCardContext::ListReadersCallback callback) {
          auto result = device::mojom::SmartCardListReadersResult::NewError(
              SmartCardError::kNoService);
          std::move(callback).Run(std::move(result));
        });
  }

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_TRUE(ExecJs(shell(), R"((async () => {
    let observer = await navigator.smartCard.watchForReaders();
    window.promise = new Promise((resolve) => {
      observer.addEventListener('readeradd', (e) => {
        resolve(e.reader.name);
      }, { once: true });
    });
  })())"));

  // Now that a listener to 'readeradd' has been set, notify that a new reader
  // was added.
  {
    auto state_flags = SmartCardReaderStateFlags::New();
    state_flags->empty = true;

    std::vector<SmartCardReaderStateOutPtr> states_out;
    states_out.push_back(
        SmartCardReaderStateOut::New("New Fake Reader", std::move(state_flags),
                                     std::vector<uint8_t>({1u, 2u, 3u, 4u})));
    auto result = device::mojom::SmartCardStatusChangeResult::NewReaderStates(
        std::move(states_out));
    first_get_status_callback.Take().Run(std::move(result));
  }

  EXPECT_EQ("New Fake Reader", EvalJs(shell(), "window.promise"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, ReaderRemove) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  base::test::TestFuture<SmartCardContext::GetStatusChangeCallback>
      first_get_status_callback;

  const std::string reader_name = "Fake Reader";

  {
    InSequence s;

    // Request what readers are currently available.
    // There is already one.
    EXPECT_CALL(mock_context_factory, ListReaders(_))
        .WillOnce([&reader_name](
                      SmartCardContext::ListReadersCallback callback) {
          std::vector<std::string> readers{reader_name};
          auto result =
              device::mojom::SmartCardListReadersResult::NewReaders(readers);
          std::move(callback).Run(std::move(result));
        });

    // Request the state of each of those readers.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [&reader_name](
                base::TimeDelta timeout,
                std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
                SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), size_t(1));
              ASSERT_EQ(states_in[0]->reader, reader_name);
              EXPECT_TRUE(states_in[0]->current_state->unaware);
              EXPECT_FALSE(states_in[0]->current_state->ignore);
              EXPECT_FALSE(states_in[0]->current_state->changed);
              EXPECT_FALSE(states_in[0]->current_state->unknown);
              EXPECT_FALSE(states_in[0]->current_state->unavailable);
              EXPECT_FALSE(states_in[0]->current_state->empty);
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
              state_flags->empty = true;
              state_flags->present = false;
              state_flags->exclusive = false;
              state_flags->inuse = false;
              state_flags->mute = false;
              state_flags->unpowered = false;

              std::vector<SmartCardReaderStateOutPtr> states_out;
              states_out.push_back(SmartCardReaderStateOut::New(
                  reader_name, std::move(state_flags),
                  std::vector<uint8_t>({1u, 2u, 3u, 4u})));
              auto result =
                  device::mojom::SmartCardStatusChangeResult::NewReaderStates(
                      std::move(states_out));
              std::move(callback).Run(std::move(result));
            });

    // Request to be notified of on the addition of a new reader.
    // Don't respond immediately.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [&first_get_status_callback, &reader_name](
                base::TimeDelta timeout,
                std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
                SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), size_t(2));

              EXPECT_EQ(states_in[0]->reader, R"(\\?PnP?\Notification)");
              EXPECT_FALSE(states_in[0]->current_state->unaware);
              EXPECT_FALSE(states_in[0]->current_state->ignore);
              EXPECT_FALSE(states_in[0]->current_state->changed);
              EXPECT_FALSE(states_in[0]->current_state->unknown);
              EXPECT_FALSE(states_in[0]->current_state->unavailable);
              EXPECT_FALSE(states_in[0]->current_state->empty);
              EXPECT_FALSE(states_in[0]->current_state->present);
              EXPECT_FALSE(states_in[0]->current_state->exclusive);
              EXPECT_FALSE(states_in[0]->current_state->inuse);
              EXPECT_FALSE(states_in[0]->current_state->mute);
              EXPECT_FALSE(states_in[0]->current_state->unpowered);

              EXPECT_EQ(states_in[1]->reader, reader_name);
              EXPECT_FALSE(states_in[1]->current_state->unaware);
              EXPECT_FALSE(states_in[1]->current_state->ignore);
              EXPECT_FALSE(states_in[1]->current_state->changed);
              EXPECT_FALSE(states_in[1]->current_state->unknown);
              EXPECT_FALSE(states_in[1]->current_state->unavailable);
              EXPECT_TRUE(states_in[1]->current_state->empty);
              EXPECT_FALSE(states_in[1]->current_state->present);
              EXPECT_FALSE(states_in[1]->current_state->exclusive);
              EXPECT_FALSE(states_in[1]->current_state->inuse);
              EXPECT_FALSE(states_in[1]->current_state->mute);
              EXPECT_FALSE(states_in[1]->current_state->unpowered);

              // Don't respond immediately.
              first_get_status_callback.SetValue(std::move(callback));
            });

    // Once the previous GetStatusChange is answered, the SmartCardReaderTracker
    // will start over from ListReader again.
    // Fail so that it stops requesting.
    EXPECT_CALL(mock_context_factory, ListReaders(_))
        .WillOnce([](SmartCardContext::ListReadersCallback callback) {
          auto result = device::mojom::SmartCardListReadersResult::NewError(
              SmartCardError::kNoService);
          std::move(callback).Run(std::move(result));
        });
  }

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_TRUE(ExecJs(shell(), R"((async () => {
    let observer = await navigator.smartCard.watchForReaders();
    window.promise = new Promise((resolve) => {
      observer.addEventListener('readerremove', (e) => {
        resolve(e.reader.name);
      }, { once: true });
    });
  })())"));

  // Now that a listener to 'readerremove' has been set, notify about removal.
  {
    auto state_flags = SmartCardReaderStateFlags::New();
    state_flags->unaware = false;
    state_flags->ignore = true;
    state_flags->changed = false;
    state_flags->unknown = true;
    state_flags->unavailable = false;
    state_flags->empty = false;
    state_flags->present = false;
    state_flags->exclusive = false;
    state_flags->inuse = false;
    state_flags->mute = false;
    state_flags->unpowered = false;

    std::vector<SmartCardReaderStateOutPtr> states_out;
    states_out.push_back(SmartCardReaderStateOut::New(
        reader_name, std::move(state_flags), std::vector<uint8_t>()));
    auto result = device::mojom::SmartCardStatusChangeResult::NewReaderStates(
        std::move(states_out));
    first_get_status_callback.Take().Run(std::move(result));
  }

  EXPECT_EQ(reader_name, EvalJs(shell(), "window.promise"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, GetReadersFails) {
  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;

  EXPECT_CALL(mock_context_factory, ListReaders(_))
      .WillOnce([](SmartCardContext::ListReadersCallback callback) {
        std::move(callback).Run(
            device::mojom::SmartCardListReadersResult::NewError(
                SmartCardError::kNoService));
      });

  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  EXPECT_EQ("SmartCardError: no-service", EvalJs(shell(), R"(
    (async () => {
      try {
        let readers = await navigator.smartCard.getReaders();
      } catch (e) {
        return `${e.name}: ${e.responseCode}`;
      }
    })()
  )"));
}

// Tests that the SmartCardReader.state attribute can be read and that
// "onstatechange" is emitted when its value changes.
IN_PROC_BROWSER_TEST_F(SmartCardTest, ReaderState) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardReaderTracker& mock_tracker = CreateMockSmartCardReaderTracker();

  {
    InSequence s;

    EXPECT_CALL(mock_tracker, Start(_, _))
        .WillOnce(
            [&mock_tracker](SmartCardReaderTracker::Observer* observer,
                            SmartCardReaderTracker::StartCallback callback) {
              mock_tracker.observer_list.AddObserverIfMissing(observer);

              std::vector<blink::mojom::SmartCardReaderInfoPtr> readers;
              readers.push_back(blink::mojom::SmartCardReaderInfo::New(
                  "Fake reader", blink::mojom::SmartCardReaderState::kEmpty,
                  std::vector<uint8_t>()));
              std::move(callback).Run(
                  blink::mojom::SmartCardGetReadersResult::NewReaders(
                      std::move(readers)));
            });

    // When the document is destroyed
    EXPECT_CALL(mock_tracker, Stop(_));
  }

  EXPECT_EQ("state: empty", EvalJs(shell(), R"(
    (async () => {
      let readers = await navigator.smartCard.getReaders();

      if (readers.length !== 1) {
        return "reader not found";
      }

      let reader = readers[0];

      window.promise = new Promise((resolve) => {
        reader.addEventListener('statechange', (e) => {
          resolve(`state changed: ${e.target.state}`);
        }, { once: true });
      });

      return `state: ${reader.state}`;
    })())"));

  blink::mojom::SmartCardReaderInfo reader_info(
      "Fake reader", blink::mojom::SmartCardReaderState::kPresent,
      std::vector<uint8_t>({1u, 2u, 3u}));
  mock_tracker.observer_list.NotifyReaderChanged(reader_info);

  EXPECT_EQ("state changed: present", EvalJs(shell(), "window.promise"));
}

IN_PROC_BROWSER_TEST_F(SmartCardTest, Connect) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardReaderTracker& mock_tracker = CreateMockSmartCardReaderTracker();

  {
    InSequence s;

    EXPECT_CALL(mock_tracker, Start(_, _))
        .WillOnce(
            [&mock_tracker](SmartCardReaderTracker::Observer* observer,
                            SmartCardReaderTracker::StartCallback callback) {
              mock_tracker.observer_list.AddObserverIfMissing(observer);

              std::vector<blink::mojom::SmartCardReaderInfoPtr> readers;
              readers.push_back(blink::mojom::SmartCardReaderInfo::New(
                  "Fake reader", blink::mojom::SmartCardReaderState::kEmpty,
                  std::vector<uint8_t>()));
              std::move(callback).Run(
                  blink::mojom::SmartCardGetReadersResult::NewReaders(
                      std::move(readers)));
            });

    // When the document is destroyed
    EXPECT_CALL(mock_tracker, Stop(_));
  }

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

  EXPECT_EQ("[object SmartCardConnection]", EvalJs(shell(), R"(
    (async () => {
      let readers = await navigator.smartCard.getReaders();

      if (readers.length !== 1) {
        return "reader not found";
      }

      let reader = readers[0];
      let connection = await reader.connect("shared", ["t0", "t1"]);

      return `${connection}`;
    })())"));
}

// Tests that multiple, parallel, SmartCardReader.connect calls on the same
// reader are supported.
//
// It's up to the SmartCardContext implementation to put those requests in a
// FIFO queue when sending them down to the platform's PC/SC stack.
IN_PROC_BROWSER_TEST_F(SmartCardTest, MultipleConnect) {
  ASSERT_TRUE(NavigateToURL(shell(), GetIsolatedContextUrl()));

  MockSmartCardReaderTracker& mock_tracker = CreateMockSmartCardReaderTracker();

  {
    InSequence s;

    EXPECT_CALL(mock_tracker, Start(_, _))
        .WillOnce(
            [&mock_tracker](SmartCardReaderTracker::Observer* observer,
                            SmartCardReaderTracker::StartCallback callback) {
              mock_tracker.observer_list.AddObserverIfMissing(observer);

              std::vector<blink::mojom::SmartCardReaderInfoPtr> readers;
              readers.push_back(blink::mojom::SmartCardReaderInfo::New(
                  "Fake reader", blink::mojom::SmartCardReaderState::kEmpty,
                  std::vector<uint8_t>()));
              std::move(callback).Run(
                  blink::mojom::SmartCardGetReadersResult::NewReaders(
                      std::move(readers)));
            });

    // When the document is destroyed
    EXPECT_CALL(mock_tracker, Stop(_));
  }

  MockSmartCardContextFactory& mock_context_factory =
      GetFakeSmartCardDelegate().mock_context_factory;
  std::queue<SmartCardContext::ConnectCallback> connect_callbacks;

  // The first two Connect calls are queued. Only from the third call onwards
  // are all requests answered and the qeue emptied.
  {
    InSequence s;

    EXPECT_CALL(mock_context_factory,
                Connect("Fake reader", SmartCardShareMode::kShared, _, _))
        .Times(Exactly(2))
        .WillRepeatedly(
            [&connect_callbacks](
                const std::string& reader,
                device::mojom::SmartCardShareMode share_mode,
                device::mojom::SmartCardProtocolsPtr preferred_protocols,
                SmartCardContext::ConnectCallback callback) {
              mojo::PendingRemote<device::mojom::SmartCardConnection>
                  pending_remote;

              mojo::MakeSelfOwnedReceiver(
                  std::make_unique<MockSmartCardConnection>(),
                  pending_remote.InitWithNewPipeAndPassReceiver());

              auto success = device::mojom::SmartCardConnectSuccess::New(
                  std::move(pending_remote), SmartCardProtocol::kT1);

              connect_callbacks.push(std::move(callback));
            });

    EXPECT_CALL(mock_context_factory,
                Connect("Fake reader", SmartCardShareMode::kShared, _, _))
        .Times(Exactly(2))
        .WillRepeatedly(
            [&connect_callbacks](
                const std::string& reader,
                device::mojom::SmartCardShareMode share_mode,
                device::mojom::SmartCardProtocolsPtr preferred_protocols,
                SmartCardContext::ConnectCallback callback) {
              connect_callbacks.push(std::move(callback));

              // Now finally solve all Connect() calls in the oder they arrived:
              while (!connect_callbacks.empty()) {
                mojo::PendingRemote<device::mojom::SmartCardConnection>
                    pending_remote;

                mojo::MakeSelfOwnedReceiver(
                    std::make_unique<MockSmartCardConnection>(),
                    pending_remote.InitWithNewPipeAndPassReceiver());

                auto success = device::mojom::SmartCardConnectSuccess::New(
                    std::move(pending_remote), SmartCardProtocol::kT1);
                std::move(connect_callbacks.front())
                    .Run(device::mojom::SmartCardConnectResult::NewSuccess(
                        std::move(success)));
                connect_callbacks.pop();
              }
            });
  }

  EXPECT_EQ(
      "[object SmartCardConnection],[object SmartCardConnection],[object "
      "SmartCardConnection]",
      EvalJs(shell(), R"(
    (async () => {
      let readers = await navigator.smartCard.getReaders();

      if (readers.length !== 1) {
        return "reader not found";
      }

      let reader = readers[0];

      // These first calls will get queued also in SmartCardService
      // since it doesn't initially have a SmartCardContext remote.
      let promise1 = reader.connect("shared");
      let promise2 = reader.connect("shared");
      let promise3 = reader.connect("shared");

      let connections = await Promise.all([promise1, promise2, promise3]);

      // This will go straight to SmartCardContext since SmartCardService
      // already has a remote by now.
      await reader.connect("shared");

      return `${connections}`;
    })())"));
}

}  // namespace content
