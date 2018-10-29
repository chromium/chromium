// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/ping_loader.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/loader/fetch/substitute_data.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

class PingLocalFrameClient : public EmptyLocalFrameClient {
 public:
  void DispatchWillSendRequest(ResourceRequest& request) override {
    if (request.GetKeepalive())
      ping_request_ = request;
  }

  const ResourceRequest& PingRequest() const { return ping_request_; }

 private:
  ResourceRequest ping_request_;
};

class PingLoaderTest : public PageTestBase {
 public:
  void SetUp() override {
    client_ = new PingLocalFrameClient;
    PageTestBase::SetupPageWithClients(nullptr, client_);
  }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void SetDocumentURL(const KURL& url) {
    GetFrame().Loader().CommitNavigation(
        ResourceRequest(url), SubstituteData(SharedBuffer::Create()),
        ClientRedirectPolicy::kNotClientRedirect,
        base::UnguessableToken::Create());
    blink::test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }

  const ResourceRequest& PingAndGetRequest(const KURL& ping_url) {
    KURL destination_url("http://navigation.destination");
    url_test_helpers::RegisterMockedURLLoad(
        ping_url, test::CoreTestDataPath("bar.html"), "text/html");
    PingLoader::SendLinkAuditPing(&GetFrame(), ping_url, destination_url);
    const ResourceRequest& ping_request = client_->PingRequest();
    if (!ping_request.IsNull()) {
      EXPECT_EQ(destination_url.GetString(),
                ping_request.HttpHeaderField("Ping-To"));
    }
    // Serve the ping request, since it will otherwise bleed in to the next
    // test, and once begun there is no way to cancel it directly.
    Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
    return ping_request;
  }

 protected:
  Persistent<PingLocalFrameClient> client_;
};

TEST_F(PingLoaderTest, HTTPSToHTTPS) {
  KURL ping_url("https://localhost/bar.html");
  SetDocumentURL(KURL("https://127.0.0.1:8000/foo.html"));
  const ResourceRequest& ping_request = PingAndGetRequest(ping_url);
  ASSERT_FALSE(ping_request.IsNull());
  EXPECT_EQ(ping_url, ping_request.Url());
  EXPECT_EQ(String(), ping_request.HttpHeaderField("Ping-From"));
}

TEST_F(PingLoaderTest, HTTPToHTTPS) {
  KURL document_url("http://127.0.0.1:8000/foo.html");
  KURL ping_url("https://localhost/bar.html");
  SetDocumentURL(document_url);
  const ResourceRequest& ping_request = PingAndGetRequest(ping_url);
  ASSERT_FALSE(ping_request.IsNull());
  EXPECT_EQ(ping_url, ping_request.Url());
  EXPECT_EQ(document_url.GetString(),
            ping_request.HttpHeaderField("Ping-From"));
}

TEST_F(PingLoaderTest, NonHTTPPingTarget) {
  SetDocumentURL(KURL("http://127.0.0.1:8000/foo.html"));
  const ResourceRequest& ping_request =
      PingAndGetRequest(KURL("ftp://localhost/bar.html"));
  ASSERT_TRUE(ping_request.IsNull());
}

TEST_F(PingLoaderTest, LinkAuditPingPriority) {
  KURL destination_url("http://navigation.destination");
  SetDocumentURL(KURL("http://localhost/foo.html"));

  KURL ping_url("https://localhost/bar.html");
  url_test_helpers::RegisterMockedURLLoad(
      ping_url, test::CoreTestDataPath("bar.html"), "text/html");
  PingLoader::SendLinkAuditPing(&GetFrame(), ping_url, destination_url);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  const ResourceRequest& request = client_->PingRequest();
  ASSERT_FALSE(request.IsNull());
  ASSERT_EQ(request.Url(), ping_url);
  EXPECT_EQ(ResourceLoadPriority::kVeryLow, request.Priority());
}

TEST_F(PingLoaderTest, ViolationPriority) {
  SetDocumentURL(KURL("http://localhost/foo.html"));

  KURL ping_url("https://localhost/bar.html");
  url_test_helpers::RegisterMockedURLLoad(
      ping_url, test::CoreTestDataPath("bar.html"), "text/html");
  PingLoader::SendViolationReport(&GetFrame(), ping_url,
                                  EncodedFormData::Create(),
                                  PingLoader::kXSSAuditorViolationReport);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  const ResourceRequest& request = client_->PingRequest();
  ASSERT_FALSE(request.IsNull());
  ASSERT_EQ(request.Url(), ping_url);
  EXPECT_EQ(ResourceLoadPriority::kVeryLow, request.Priority());
}

TEST_F(PingLoaderTest, BeaconPriority) {
  SetDocumentURL(KURL("https://localhost/foo.html"));

  KURL ping_url("https://localhost/bar.html");
  url_test_helpers::RegisterMockedURLLoad(
      ping_url, test::CoreTestDataPath("bar.html"), "text/html");
  PingLoader::SendBeacon(&GetFrame(), ping_url, "hello");
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  const ResourceRequest& request = client_->PingRequest();
  ASSERT_FALSE(request.IsNull());
  ASSERT_EQ(request.Url(), ping_url);
  EXPECT_EQ(ResourceLoadPriority::kVeryLow, request.Priority());
}

}  // namespace

}  // namespace blink
