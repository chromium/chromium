// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/ping_loader.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

class PartialResourceRequest {
 public:
  PartialResourceRequest() : PartialResourceRequest(ResourceRequest()) {}
  PartialResourceRequest(const ResourceRequest& request)
      : url_(request.Url()), priority_(request.Priority()) {
    http_header_fields_.Adopt(request.HttpHeaderFields().CopyData());
  }

  bool IsNull() const { return url_.IsNull(); }
  const KURL& Url() const { return url_; }
  const AtomicString& HttpHeaderField(const AtomicString& name) const {
    return http_header_fields_.Get(name);
  }
  ResourceLoadPriority Priority() const { return priority_; }

 private:
  KURL url_;
  HTTPHeaderMap http_header_fields_;
  ResourceLoadPriority priority_;
};

class PingLocalFrameClient : public EmptyLocalFrameClient {
 public:
  PingLocalFrameClient() = default;

  std::unique_ptr<URLLoader> CreateURLLoaderForTesting() override {
    return URLLoaderMockFactory::GetSingletonInstance()->CreateURLLoader();
  }

  void DispatchFinalizeRequest(ResourceRequest& request) override {
    if (request.GetKeepalive())
      ping_request_ = PartialResourceRequest(request);
  }

  const PartialResourceRequest& PingRequest() const { return ping_request_; }

 private:
  PartialResourceRequest ping_request_;
};

class PingLoaderTest : public PageTestBase {
 public:
  void SetUp() override {
    client_ = MakeGarbageCollected<PingLocalFrameClient>();
    PageTestBase::SetupPageWithClients(nullptr, client_);
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void SetDocumentURL(const KURL& url) {
    GetFrame().Loader().CommitNavigation(
        WebNavigationParams::CreateWithEmptyHTMLForTesting(url),
        nullptr /* extra_data */);
    blink::test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }

  const PartialResourceRequest& PingAndGetRequest(const KURL& ping_url) {
    KURL destination_url("http://navigation.destination");
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |PageTestBase::dummy_page_holder_|.
    url_test_helpers::RegisterMockedURLLoad(
        ping_url, test::CoreTestDataPath("bar.html"), "text/html");
    PingLoader::SendLinkAuditPing(&GetFrame(), ping_url, destination_url);
    const PartialResourceRequest& ping_request = client_->PingRequest();
    if (!ping_request.IsNull()) {
      EXPECT_EQ(destination_url.GetString(),
                ping_request.HttpHeaderField(AtomicString("Ping-To")));
    }
    // Serve the ping request, since it will otherwise bleed in to the next
    // test, and once begun there is no way to cancel it directly.
    url_test_helpers::ServeAsynchronousRequests();
    return ping_request;
  }

 protected:
  Persistent<PingLocalFrameClient> client_;
};

TEST_F(PingLoaderTest, HTTPSToHTTPS) {
  KURL ping_url("https://localhost/bar.html");
  SetDocumentURL(KURL("https://127.0.0.1:8000/foo.html"));
  const PartialResourceRequest& ping_request = PingAndGetRequest(ping_url);
  ASSERT_FALSE(ping_request.IsNull());
  EXPECT_EQ(ping_url, ping_request.Url());
  EXPECT_EQ(String(), ping_request.HttpHeaderField(AtomicString("Ping-From")));
}

TEST_F(PingLoaderTest, HTTPToHTTPS) {
  KURL document_url("http://127.0.0.1:8000/foo.html");
  KURL ping_url("https://localhost/bar.html");
  SetDocumentURL(document_url);
  const PartialResourceRequest& ping_request = PingAndGetRequest(ping_url);
  ASSERT_FALSE(ping_request.IsNull());
  EXPECT_EQ(ping_url, ping_request.Url());
  EXPECT_EQ(document_url.GetString(),
            ping_request.HttpHeaderField(AtomicString("Ping-From")));
}

TEST_F(PingLoaderTest, NonHTTPPingTarget) {
  SetDocumentURL(KURL("http://127.0.0.1:8000/foo.html"));
  const PartialResourceRequest& ping_request =
      PingAndGetRequest(KURL("ftp://localhost/bar.html"));
  ASSERT_TRUE(ping_request.IsNull());
}

TEST_F(PingLoaderTest, LinkAuditPingPriority) {
  KURL destination_url("http://navigation.destination");
  SetDocumentURL(KURL("http://localhost/foo.html"));

  KURL ping_url("https://localhost/bar.html");
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |PageTestBase::dummy_page_holder_|.
  url_test_helpers::RegisterMockedURLLoad(
      ping_url, test::CoreTestDataPath("bar.html"), "text/html");
  PingLoader::SendLinkAuditPing(&GetFrame(), ping_url, destination_url);
  url_test_helpers::ServeAsynchronousRequests();
  const PartialResourceRequest& request = client_->PingRequest();
  ASSERT_FALSE(request.IsNull());
  ASSERT_EQ(request.Url(), ping_url);
  EXPECT_EQ(ResourceLoadPriority::kVeryLow, request.Priority());
}

TEST_F(PingLoaderTest, ViolationPriority) {
  SetDocumentURL(KURL("http://localhost/foo.html"));

  KURL ping_url("https://localhost/bar.html");
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |PageTestBase::dummy_page_holder_|.
  url_test_helpers::RegisterMockedURLLoad(
      ping_url, test::CoreTestDataPath("bar.html"), "text/html");
  PingLoader::SendViolationReport(GetFrame().DomWindow(), ping_url,
                                  EncodedFormData::Create(), false);
  url_test_helpers::ServeAsynchronousRequests();
  const PartialResourceRequest& request = client_->PingRequest();
  ASSERT_FALSE(request.IsNull());
  ASSERT_EQ(request.Url(), ping_url);
  EXPECT_EQ(ResourceLoadPriority::kVeryLow, request.Priority());
}

TEST_F(PingLoaderTest, FrameAncestorsViolationHasOpaqueOrigin) {
  SetDocumentURL(KURL("http://localhost/foo.html"));

  KURL ping_url("https://localhost/bar.html");
  // TODO(crbug.com/41337257): We should use the mock functionality
  // via |PageTestBase::dummy_page_holder_|.
  url_test_helpers::RegisterMockedURLLoad(
      ping_url, test::CoreTestDataPath("bar.html"), "text/html");
  PingLoader::SendViolationReport(GetFrame().DomWindow(), ping_url,
                                  EncodedFormData::Create(), true);
  url_test_helpers::ServeAsynchronousRequests();
  const PartialResourceRequest& request = client_->PingRequest();
  ASSERT_FALSE(request.IsNull());
  ASSERT_EQ(request.Url(), ping_url);
  EXPECT_EQ(request.HttpHeaderField(AtomicString("Origin")), String());
}

TEST_F(PingLoaderTest, BeaconPriority) {
  SetDocumentURL(KURL("https://localhost/foo.html"));

  KURL ping_url("https://localhost/bar.html");
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |PageTestBase::dummy_page_holder_|.
  url_test_helpers::RegisterMockedURLLoad(
      ping_url, test::CoreTestDataPath("bar.html"), "text/html");
  PingLoader::SendBeacon(*ToScriptStateForMainWorld(&GetFrame()), &GetFrame(),
                         ping_url, "hello");
  url_test_helpers::ServeAsynchronousRequests();
  const PartialResourceRequest& request = client_->PingRequest();
  ASSERT_FALSE(request.IsNull());
  ASSERT_EQ(request.Url(), ping_url);
  EXPECT_EQ(ResourceLoadPriority::kVeryLow, request.Priority());
}

}  // namespace

}  // namespace blink
