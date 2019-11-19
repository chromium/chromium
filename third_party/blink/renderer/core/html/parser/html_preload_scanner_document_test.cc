// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

class MockPrescientNetworking : public WebPrescientNetworking {
 public:
  bool DidDnsPrefetch() const { return did_dns_prefetch_; }
  bool DidPreconnect() const { return did_preconnect_; }

 private:
  void PrefetchDNS(const WebString&) override { did_dns_prefetch_ = true; }

  void Preconnect(WebLocalFrame*, const WebURL&, const bool) override {
    did_preconnect_ = true;
  }

  bool did_dns_prefetch_ = false;
  bool did_preconnect_ = false;
};

class TestingPlatformSupportWithMockPrescientNetworking
    : public TestingPlatformSupport {
 public:
  MockPrescientNetworking& GetMockPrescientNetworking() {
    return mock_prescient_networking_;
  }

 private:
  WebPrescientNetworking* PrescientNetworking() override {
    return &mock_prescient_networking_;
  }

  MockPrescientNetworking mock_prescient_networking_;
};

// HTMLPreloadScannerDocumentTest tests if network hints are
// properly committed/suppressed on various HTMLDocumentParser uses.
//
// HTMLPreloadScannerDocumentTest uses SimTest so we have a valid
// ResourceFetcher. SimTest disables asynchronous parsing mode, so we rely on
// web_tests for asynchronous parsing testing cases.
//
// See also: web_tests/http/tests/preload and web_tests/fast/preloader.
class HTMLPreloadScannerDocumentTest : public SimTest {
 private:
  void SetUp() override {
    SimTest::SetUp();

    constexpr const char kTestUrl[] = "https://example.com/test.html";
    main_resource_ = std::make_unique<SimRequest>(kTestUrl, "text/html");
    LoadURL(kTestUrl);
  }

 protected:
  ScopedTestingPlatformSupport<
      TestingPlatformSupportWithMockPrescientNetworking>
      platform_;
  std::unique_ptr<SimRequest> main_resource_;
};

TEST_F(HTMLPreloadScannerDocumentTest, DOMParser) {
  main_resource_->Complete(R"(<script>
    var p = new DOMParser();
    p.parseFromString(
      '<link rel="preconnect" href="https://target.example.com/"/>',
      'text/html');
  </script>)");

  EXPECT_FALSE(platform_->GetMockPrescientNetworking().DidDnsPrefetch());
  EXPECT_FALSE(platform_->GetMockPrescientNetworking().DidPreconnect());
}

TEST_F(HTMLPreloadScannerDocumentTest, DetachedDocumentInnerHTML) {
  main_resource_->Complete(R"(<script>
    var doc = document.implementation.createHTMLDocument('');
    doc.body.innerHTML =
        '<link rel="preconnect" href="https://target.example.com/"/>';
  </script>)");

  EXPECT_FALSE(platform_->GetMockPrescientNetworking().DidDnsPrefetch());
  EXPECT_FALSE(platform_->GetMockPrescientNetworking().DidPreconnect());
}

TEST_F(HTMLPreloadScannerDocumentTest, XHRResponseDocument) {
  main_resource_->Complete(R"(<script>
    var xhr = new XMLHttpRequest();
    xhr.open('GET', 'data:text/html,' +
        '<link rel="preconnect" href="https://target.example.com/"/>');
    xhr.responseType = 'document';
    xhr.send();
  </script>)");

  EXPECT_FALSE(platform_->GetMockPrescientNetworking().DidDnsPrefetch());
  EXPECT_FALSE(platform_->GetMockPrescientNetworking().DidPreconnect());
}

}  // namespace blink
