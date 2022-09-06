// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pending_beacon_host.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "content/browser/renderer_host/pending_beacon_service.h"
#include "content/public/browser/permission_result.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom.h"
#include "url/origin.h"

namespace content {

class PendingBeaconHostTestBase
    : public RenderViewHostTestHarness,
      public testing::WithParamInterface<std::string> {
 public:
  PendingBeaconHostTestBase(const PendingBeaconHostTestBase&) = delete;
  PendingBeaconHostTestBase& operator=(const PendingBeaconHostTestBase&) =
      delete;
  PendingBeaconHostTestBase() = default;

 protected:
  // Creates a new instance of PendingBeaconHost, which uses a new instance of
  // TestURLLoaderFactory stored at `test_url_loader_factory_`.
  // The network requests made by the returned PendingBeaconHost will go through
  // `test_url_loader_factory_` which is useful for examining requests.
  PendingBeaconHost* CreateHost() {
    SetPermissionStatus(blink::PermissionType::BACKGROUND_SYNC,
                        blink::mojom::PermissionStatus::GRANTED);

    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    NavigateAndCommit(GURL(kBeaconPageURL));

    PendingBeaconHost::CreateForCurrentDocument(
        main_rfh(), test_url_loader_factory_->GetSafeWeakWrapper(),
        PendingBeaconService::GetInstance());
    return PendingBeaconHost::GetForCurrentDocument(main_rfh());
  }

  static blink::mojom::BeaconMethod ToBeaconMethod(const std::string& method) {
    if (method == net::HttpRequestHeaders::kGetMethod) {
      return blink::mojom::BeaconMethod::kGet;
    }
    return blink::mojom::BeaconMethod::kPost;
  }

  static GURL CreateBeaconTargetURL(size_t i) {
    return GURL(base::StringPrintf("%s/%zu", kBeaconTargetURL, i));
  }

  // Verifies if the total number of network requests sent via
  // `test_url_loader_factory_` equals to `expected`.
  void ExpectTotalNetworkRequests(const base::Location& location,
                                  const int expected) {
    EXPECT_EQ(test_url_loader_factory_->NumPending(), expected)
        << location.ToString();
  }

  std::unique_ptr<BrowserContext> CreateBrowserContext() override {
    auto context = std::make_unique<TestBrowserContext>();
    context->SetPermissionControllerDelegate(
        std::make_unique<::testing::NiceMock<MockPermissionManager>>());
    return context;
  }

  // Updates the `permission_type` to the given `permission_status` through
  // the MockPermissionManager.
  void SetPermissionStatus(blink::PermissionType permission_type,
                           blink::mojom::PermissionStatus permission_status) {
    auto* mock_permission_manager = static_cast<MockPermissionManager*>(
        browser_context()->GetPermissionControllerDelegate());

    ON_CALL(*mock_permission_manager,
            GetPermissionResultForOriginWithoutContext(permission_type,
                                                       ::testing::_))
        .WillByDefault(::testing::Return(PermissionResult(
            permission_status, PermissionStatusSource::UNSPECIFIED)));
  }

  static constexpr char kBeaconTargetURL[] = "/test_send_beacon";
  static constexpr char kBeaconPageURL[] = "http://test-pending-beacon";
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
};

class PendingBeaconHostTest : public PendingBeaconHostTestBase {
 protected:
  // Registers a callback to verify if the most-recent network request's content
  // matches the given `method` and `url`.
  void SetExpectNetworkRequest(const base::Location& location,
                               const std::string& method,
                               const GURL& url) {
    test_url_loader_factory_->SetInterceptor(base::BindLambdaForTesting(
        [location, method, url](const network::ResourceRequest& request) {
          EXPECT_EQ(request.mode, network::mojom::RequestMode::kCors);
          EXPECT_EQ(request.request_initiator,
                    url::Origin::Create(GURL(kBeaconPageURL)));
          EXPECT_EQ(request.credentials_mode,
                    network::mojom::CredentialsMode::kSameOrigin);

          EXPECT_EQ(request.method, method) << location.ToString();
          EXPECT_EQ(request.url, url) << location.ToString();
          if (method == net::HttpRequestHeaders::kPostMethod) {
            EXPECT_TRUE(request.keepalive) << location.ToString();
          }
        }));
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PendingBeaconHostTest,
    testing::ValuesIn<std::vector<std::string>>(
        {net::HttpRequestHeaders::kGetMethod,
         net::HttpRequestHeaders::kPostMethod}),
    [](const testing::TestParamInfo<PendingBeaconHostTest::ParamType>& info) {
      return info.param;
    });

TEST_P(PendingBeaconHostTest, SendBeacon) {
  const std::string method = GetParam();
  const auto url = GURL("/test_send_beacon");
  auto* host = CreateHost();
  mojo::Remote<blink::mojom::PendingBeacon> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  host->CreateBeacon(std::move(receiver), url, ToBeaconMethod(method));

  SetExpectNetworkRequest(FROM_HERE, method, url);
  remote->SendNow();
  ExpectTotalNetworkRequests(FROM_HERE, 1);
}

TEST_P(PendingBeaconHostTest, SendOneOfBeacons) {
  const std::string method = GetParam();
  const auto* url = "/test_send_beacon";
  const size_t total = 5;

  // Sends out only the 3rd of 5 created beacons.
  auto* host = CreateHost();
  std::vector<mojo::Remote<blink::mojom::PendingBeacon>> remotes(total);
  for (size_t i = 0; i < remotes.size(); i++) {
    auto receiver = remotes[i].BindNewPipeAndPassReceiver();
    host->CreateBeacon(std::move(receiver), GURL(url + i),
                       ToBeaconMethod(method));
  }

  const size_t sent_beacon_i = 2;
  SetExpectNetworkRequest(FROM_HERE, method, GURL(url + sent_beacon_i));
  remotes[sent_beacon_i]->SendNow();
  ExpectTotalNetworkRequests(FROM_HERE, 1);
}

TEST_P(PendingBeaconHostTest, SendBeacons) {
  const std::string method = GetParam();
  const auto* url = "/test_send_beacon";
  const size_t total = 5;

  // Sends out all 5 created beacons, in reversed order.
  auto* host = CreateHost();
  std::vector<mojo::Remote<blink::mojom::PendingBeacon>> remotes(total);
  for (size_t i = 0; i < remotes.size(); i++) {
    auto receiver = remotes[i].BindNewPipeAndPassReceiver();
    host->CreateBeacon(std::move(receiver), GURL(url + i),
                       ToBeaconMethod(method));
  }
  for (int i = remotes.size() - 1; i >= 0; i--) {
    SetExpectNetworkRequest(FROM_HERE, method, GURL(url + i));
    remotes[i]->SendNow();
  }
  ExpectTotalNetworkRequests(FROM_HERE, total);
}

TEST_P(PendingBeaconHostTest, DeleteAndSendBeacon) {
  const std::string method = GetParam();
  const auto url = GURL("/test_send_beacon");
  auto* host = CreateHost();
  mojo::Remote<blink::mojom::PendingBeacon> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  host->CreateBeacon(std::move(receiver), url, ToBeaconMethod(method));

  // Deleted beacon won't be sent out by host.
  remote->Deactivate();
  remote->SendNow();
  ExpectTotalNetworkRequests(FROM_HERE, 0);
}

TEST_P(PendingBeaconHostTest, DeleteOneAndSendOtherBeacons) {
  const std::string method = GetParam();
  const auto* url = "/test_send_beacon";
  const size_t total = 5;

  // Creates 5 beacons. Deletes the 3rd of them, and sends out the others.
  auto* host = CreateHost();
  std::vector<mojo::Remote<blink::mojom::PendingBeacon>> remotes(total);
  for (size_t i = 0; i < remotes.size(); i++) {
    auto receiver = remotes[i].BindNewPipeAndPassReceiver();
    host->CreateBeacon(std::move(receiver), GURL(url + i),
                       ToBeaconMethod(method));
  }

  const size_t deleted_beacon_i = 2;
  remotes[deleted_beacon_i]->Deactivate();

  for (int i = remotes.size() - 1; i >= 0; i--) {
    if (i != deleted_beacon_i) {
      SetExpectNetworkRequest(FROM_HERE, method, GURL(url + i));
    }
    remotes[i]->SendNow();
  }
  ExpectTotalNetworkRequests(FROM_HERE, total - 1);
}

TEST_P(PendingBeaconHostTest, SendOnDocumentUnloadWithBackgroundSync) {
  const std::string method = GetParam();
  const size_t total = 5;

  // Creates 5 beacons on the page.
  auto* host = CreateHost();
  std::vector<mojo::Remote<blink::mojom::PendingBeacon>> remotes(total);
  for (size_t i = 0; i < remotes.size(); i++) {
    auto receiver = remotes[i].BindNewPipeAndPassReceiver();
    host->CreateBeacon(std::move(receiver), CreateBeaconTargetURL(i),
                       ToBeaconMethod(method));
  }

  SetPermissionStatus(blink::PermissionType::BACKGROUND_SYNC,
                      blink::mojom::PermissionStatus::GRANTED);
  // Forces deleting the page where `host` resides.
  DeleteContents();

  ExpectTotalNetworkRequests(FROM_HERE, total);
}

TEST_P(PendingBeaconHostTest,
       DoesNotSendOnDocumentUnloadWithoutBackgroundSync) {
  const std::string method = GetParam();
  const size_t total = 5;

  // Creates 5 beacons on the page.
  auto* host = CreateHost();
  std::vector<mojo::Remote<blink::mojom::PendingBeacon>> remotes(total);
  for (size_t i = 0; i < remotes.size(); i++) {
    auto receiver = remotes[i].BindNewPipeAndPassReceiver();
    host->CreateBeacon(std::move(receiver), CreateBeaconTargetURL(i),
                       ToBeaconMethod(method));
  }

  SetPermissionStatus(blink::PermissionType::BACKGROUND_SYNC,
                      blink::mojom::PermissionStatus::ASK);
  // Forces deleting the page where `host` resides.
  DeleteContents();

  ExpectTotalNetworkRequests(FROM_HERE, 0);
}

class BeaconTestBase : public PendingBeaconHostTestBase {
 protected:
  void TearDown() override {
    host_ = nullptr;
    PendingBeaconHostTestBase::TearDown();
  }

  mojo::Remote<blink::mojom::PendingBeacon> CreateBeaconAndPassRemote(
      const std::string& method) {
    const auto url = GURL("/test_send_beacon");
    host_ = CreateHost();
    mojo::Remote<blink::mojom::PendingBeacon> remote;
    auto receiver = remote.BindNewPipeAndPassReceiver();
    host_->CreateBeacon(std::move(receiver), url, ToBeaconMethod(method));
    return remote;
  }

  scoped_refptr<network::ResourceRequestBody> CreateRequestBody(
      const std::string& data) {
    return network::ResourceRequestBody::CreateFromBytes(data.data(),
                                                         data.size());
  }

  scoped_refptr<network::ResourceRequestBody> CreateFileRequestBody(
      uint64_t offset = 0,
      uint64_t length = 10) {
    scoped_refptr<network::ResourceRequestBody> body =
        base::MakeRefCounted<network::ResourceRequestBody>();
    body->AppendFileRange(base::FilePath(FILE_PATH_LITERAL("file.txt")), offset,
                          length, base::Time());
    return body;
  }

  scoped_refptr<network::ResourceRequestBody> CreateComplexRequestBody() {
    auto body = CreateRequestBody("part1");
    body->AppendFileRange(base::FilePath(FILE_PATH_LITERAL("part2.txt")), 0, 10,
                          base::Time());
    return body;
  }

  scoped_refptr<network::ResourceRequestBody> CreateStreamingRequestBody() {
    mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter> remote;
    auto unused_receiver = remote.InitWithNewPipeAndPassReceiver();
    scoped_refptr<network::ResourceRequestBody> body =
        base::MakeRefCounted<network::ResourceRequestBody>();
    body->SetToChunkedDataPipe(
        std::move(remote), network::ResourceRequestBody::ReadOnlyOnce(false));
    return body;
  }

 private:
  // Owned by `main_rfh()`.
  PendingBeaconHost* host_;
};

using GetBeaconTest = BeaconTestBase;

TEST_F(GetBeaconTest, AttemptToSetRequestDataForGetBeaconAndTerminated) {
  auto beacon_remote =
      CreateBeaconAndPassRemote(net::HttpRequestHeaders::kGetMethod);
  // Intercepts Mojo bad-message error.
  std::string bad_message;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        ASSERT_TRUE(bad_message.empty());
        bad_message = error;
      }));

  beacon_remote->SetRequestData(CreateRequestBody("data"), "");
  beacon_remote.FlushForTesting();

  EXPECT_EQ(bad_message, "Unexpected BeaconMethod from renderer");
}

using PostBeaconTest = BeaconTestBase;

TEST_F(PostBeaconTest, AttemptToSetRequestDataWithComplexBodyAndTerminated) {
  auto beacon_remote =
      CreateBeaconAndPassRemote(net::HttpRequestHeaders::kPostMethod);
  // Intercepts Mojo bad-message error.
  std::string bad_message;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        ASSERT_TRUE(bad_message.empty());
        bad_message = error;
      }));

  beacon_remote->SetRequestData(CreateComplexRequestBody(), "");
  beacon_remote.FlushForTesting();

  EXPECT_EQ(bad_message, "Complex body is not supported yet");
}

TEST_F(PostBeaconTest, AttemptToSetRequestDataWithStreamingBodyAndTerminated) {
  auto beacon_remote =
      CreateBeaconAndPassRemote(net::HttpRequestHeaders::kPostMethod);
  // Intercepts Mojo bad-message error.
  std::string bad_message;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        ASSERT_TRUE(bad_message.empty());
        bad_message = error;
      }));

  beacon_remote->SetRequestData(CreateStreamingRequestBody(), "");
  beacon_remote.FlushForTesting();

  EXPECT_EQ(bad_message, "Streaming body is not supported.");
}

TEST_F(PostBeaconTest, AttemptToSetRequestURLForPostBeaconAndTerminated) {
  auto beacon_remote =
      CreateBeaconAndPassRemote(net::HttpRequestHeaders::kPostMethod);
  // Intercepts Mojo bad-message error.
  std::string bad_message;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        ASSERT_TRUE(bad_message.empty());
        bad_message = error;
      }));

  beacon_remote->SetRequestURL(GURL("/test_set_url"));
  beacon_remote.FlushForTesting();

  EXPECT_EQ(bad_message, "Unexpected BeaconMethod from renderer");
}

class PostBeaconRequestDataTest : public BeaconTestBase {
 protected:
  // Registers a callback to verify if the most-recent network request's content
  // matches the given `expected_body` and `expected_content_type`.
  void SetExpectNetworkRequest(
      const base::Location& location,
      scoped_refptr<network::ResourceRequestBody> expected_body,
      const absl::optional<std::string>& expected_content_type =
          absl::nullopt) {
    test_url_loader_factory_->SetInterceptor(base::BindLambdaForTesting(
        [location, expected_body,
         expected_content_type](const network::ResourceRequest& request) {
          ASSERT_EQ(request.method, net::HttpRequestHeaders::kPostMethod)
              << location.ToString();
          ASSERT_EQ(request.request_body->elements()->size(), 1u)
              << location.ToString();

          const auto& expected_element = expected_body->elements()->at(0);
          const auto& element = request.request_body->elements()->at(0);
          EXPECT_EQ(element.type(), expected_element.type());
          if (expected_element.type() == network::DataElement::Tag::kBytes) {
            const auto& expected_bytes =
                expected_element.As<network::DataElementBytes>();
            const auto& bytes = element.As<network::DataElementBytes>();
            EXPECT_EQ(bytes.AsStringPiece(), expected_bytes.AsStringPiece())
                << location.ToString();
          } else if (expected_element.type() ==
                     network::DataElement::Tag::kFile) {
            const auto& expected_file =
                expected_element.As<network::DataElementFile>();
            const auto& file = element.As<network::DataElementFile>();
            EXPECT_EQ(file.path(), expected_file.path()) << location.ToString();
            EXPECT_EQ(file.offset(), expected_file.offset())
                << location.ToString();
            EXPECT_EQ(file.length(), expected_file.length())
                << location.ToString();
          }

          if (!expected_content_type.has_value()) {
            EXPECT_FALSE(request.headers.HasHeader(
                net::HttpRequestHeaders::kContentType))
                << location.ToString();
            return;
          }
          std::string content_type;
          EXPECT_TRUE(request.headers.GetHeader(
              net::HttpRequestHeaders::kContentType, &content_type))
              << location.ToString();
          EXPECT_EQ(content_type, expected_content_type) << location.ToString();
        }));
  }

  mojo::Remote<blink::mojom::PendingBeacon> CreateBeaconAndPassRemote() {
    return BeaconTestBase::CreateBeaconAndPassRemote(
        net::HttpRequestHeaders::kPostMethod);
  }
};

TEST_F(PostBeaconRequestDataTest, SendBytesWithCorsSafelistedContentType) {
  auto beacon_remote = CreateBeaconAndPassRemote();

  auto body = CreateRequestBody("data");
  beacon_remote->SetRequestData(body, "text/plain");

  SetExpectNetworkRequest(FROM_HERE, body, "text/plain");
  beacon_remote->SendNow();
  ExpectTotalNetworkRequests(FROM_HERE, 1);
}

TEST_F(PostBeaconRequestDataTest, SendBytesWithEmptyContentType) {
  auto beacon_remote = CreateBeaconAndPassRemote();

  auto body = CreateRequestBody("data");
  beacon_remote->SetRequestData(body, "");

  SetExpectNetworkRequest(FROM_HERE, body);
  beacon_remote->SendNow();
  ExpectTotalNetworkRequests(FROM_HERE, 1);
}

TEST_F(PostBeaconRequestDataTest, SendBlobWithCorsSafelistedContentType) {
  auto beacon_remote = CreateBeaconAndPassRemote();

  auto body = CreateFileRequestBody();
  beacon_remote->SetRequestData(body, "text/plain");

  SetExpectNetworkRequest(FROM_HERE, body, "text/plain");
  beacon_remote->SendNow();
  ExpectTotalNetworkRequests(FROM_HERE, 1);
}

TEST_F(PostBeaconRequestDataTest, SendBlobWithEmptyContentType) {
  auto beacon_remote = CreateBeaconAndPassRemote();

  auto body = CreateFileRequestBody();
  beacon_remote->SetRequestData(body, "");

  SetExpectNetworkRequest(FROM_HERE, body);
  beacon_remote->SendNow();
  ExpectTotalNetworkRequests(FROM_HERE, 1);
}

TEST_F(PostBeaconRequestDataTest, SendBlobWithNonCorsSafelistedContentType) {
  auto beacon_remote = CreateBeaconAndPassRemote();

  auto body = CreateFileRequestBody();
  beacon_remote->SetRequestData(body, "application/unsafe");

  SetExpectNetworkRequest(FROM_HERE, body, "application/unsafe");
  beacon_remote->SendNow();
  ExpectTotalNetworkRequests(FROM_HERE, 1);
}

}  // namespace content
