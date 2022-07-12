// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pending_beacon_host.h"

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "content/browser/renderer_host/pending_beacon_service.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom.h"

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
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();

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
          EXPECT_EQ(request.method, method) << location.ToString();
          EXPECT_EQ(request.url, url) << location.ToString();
          if (method == net::HttpRequestHeaders::kPostMethod) {
            EXPECT_TRUE(request.keepalive) << location.ToString();
          }
        }));
  }

  // Verifies if the total number of network requests sent via
  // `test_url_loader_factory_` equals to `expected`.
  void ExpectTotalNetworkRequests(const base::Location& location,
                                  const int expected) {
    EXPECT_EQ(test_url_loader_factory_->NumPending(), expected)
        << location.ToString();
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
  const base::TimeDelta timeout = base::Milliseconds(0);
  const auto url = GURL("/test_send_beacon");
  auto* host = CreateHost();
  mojo::Remote<blink::mojom::PendingBeacon> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  host->CreateBeacon(std::move(receiver), url, ToBeaconMethod(method), timeout);

  SetExpectNetworkRequest(FROM_HERE, method, url);
  remote->SendNow();
  ExpectTotalNetworkRequests(FROM_HERE, 1);
}

TEST_P(PendingBeaconHostTest, SendOneOfBeacons) {
  const std::string method = GetParam();
  const base::TimeDelta timeout = base::Milliseconds(0);
  const auto* url = "/test_send_beacon";
  const size_t total = 5;

  // Sends out only the 3rd of 5 created beacons.
  auto* host = CreateHost();
  std::vector<mojo::Remote<blink::mojom::PendingBeacon>> remotes(total);
  for (size_t i = 0; i < remotes.size(); i++) {
    auto receiver = remotes[i].BindNewPipeAndPassReceiver();
    host->CreateBeacon(std::move(receiver), GURL(url + i),
                       ToBeaconMethod(method), timeout);
  }

  const size_t sent_beacon_i = 2;
  SetExpectNetworkRequest(FROM_HERE, method, GURL(url + sent_beacon_i));
  remotes[sent_beacon_i]->SendNow();
  ExpectTotalNetworkRequests(FROM_HERE, 1);
}

TEST_P(PendingBeaconHostTest, SendBeacons) {
  const std::string method = GetParam();
  const base::TimeDelta timeout = base::Milliseconds(0);
  const auto* url = "/test_send_beacon";
  const size_t total = 5;

  // Sends out all 5 created beacons, in reversed order.
  auto* host = CreateHost();
  std::vector<mojo::Remote<blink::mojom::PendingBeacon>> remotes(total);
  for (size_t i = 0; i < remotes.size(); i++) {
    auto receiver = remotes[i].BindNewPipeAndPassReceiver();
    host->CreateBeacon(std::move(receiver), GURL(url + i),
                       ToBeaconMethod(method), timeout);
  }
  for (int i = remotes.size() - 1; i >= 0; i--) {
    SetExpectNetworkRequest(FROM_HERE, method, GURL(url + i));
    remotes[i]->SendNow();
  }
  ExpectTotalNetworkRequests(FROM_HERE, total);
}

TEST_P(PendingBeaconHostTest, DeleteAndSendBeacon) {
  const std::string method = GetParam();
  const base::TimeDelta timeout = base::Milliseconds(0);
  const auto url = GURL("/test_send_beacon");
  auto* host = CreateHost();
  mojo::Remote<blink::mojom::PendingBeacon> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  host->CreateBeacon(std::move(receiver), url, ToBeaconMethod(method), timeout);

  // Deleted beacon won't be sent out by host.
  remote->Deactivate();
  remote->SendNow();
  ExpectTotalNetworkRequests(FROM_HERE, 0);
}

TEST_P(PendingBeaconHostTest, DeleteOneAndSendOtherBeacons) {
  const std::string method = GetParam();
  const base::TimeDelta timeout = base::Milliseconds(0);
  const auto* url = "/test_send_beacon";
  const size_t total = 5;

  // Creates 5 beacons. Deletes the 3rd of them, and sends out the others.
  auto* host = CreateHost();
  std::vector<mojo::Remote<blink::mojom::PendingBeacon>> remotes(total);
  for (size_t i = 0; i < remotes.size(); i++) {
    auto receiver = remotes[i].BindNewPipeAndPassReceiver();
    host->CreateBeacon(std::move(receiver), GURL(url + i),
                       ToBeaconMethod(method), timeout);
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

class BeaconTest : public PendingBeaconHostTestBase {
 protected:
  void TearDown() override {
    host_ = nullptr;
    PendingBeaconHostTestBase::TearDown();
  }

  mojo::Remote<blink::mojom::PendingBeacon> CreateBeaconAndPassRemote(
      const std::string& method) {
    const base::TimeDelta timeout = base::Milliseconds(0);
    const auto url = GURL("/test_send_beacon");
    host_ = CreateHost();
    mojo::Remote<blink::mojom::PendingBeacon> remote;
    auto receiver = remote.BindNewPipeAndPassReceiver();
    host_->CreateBeacon(std::move(receiver), url, ToBeaconMethod(method),
                        timeout);
    return remote;
  }

  scoped_refptr<network::ResourceRequestBody> CreateRequestBody(
      const std::string& data) {
    return network::ResourceRequestBody::CreateFromBytes(data.data(),
                                                         data.size());
  }

 private:
  // Owned by `main_rfh()`.
  PendingBeaconHost* host_;
};

TEST_F(BeaconTest, AttemptToSetDataForGetBeaconAndTerminated) {
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

TEST_F(BeaconTest, AttemptToSetUnsafeContentTypeAndTerminated) {
  auto beacon_remote =
      CreateBeaconAndPassRemote(net::HttpRequestHeaders::kPostMethod);
  // Intercepts Mojo bad-message error.
  std::string bad_message;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string& error) {
        ASSERT_TRUE(bad_message.empty());
        bad_message = error;
      }));

  beacon_remote->SetRequestData(CreateRequestBody("data"),
                                "application/unsafe");
  beacon_remote.FlushForTesting();

  EXPECT_EQ(bad_message, "Unexpected Content-Type from renderer");
}

}  // namespace content
