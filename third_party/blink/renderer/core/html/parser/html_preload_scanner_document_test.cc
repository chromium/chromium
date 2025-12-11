// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/no_state_prefetch_client.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

class MockNoStatePrefetchClient : public NoStatePrefetchClient {
 public:
  explicit MockNoStatePrefetchClient(Page& page)
      : NoStatePrefetchClient(page, nullptr) {}

 private:
  bool IsPrefetchOnly() override { return true; }
};

class MockPrescientNetworking : public WebPrescientNetworking {
 public:
  bool DidDnsPrefetch() const { return did_dns_prefetch_; }
  bool DidPreconnect() const { return did_preconnect_; }

 private:
  void PrefetchDNS(const WebURL&) override { did_dns_prefetch_ = true; }
  void Preconnect(const WebURL&, bool) override { did_preconnect_ = true; }

  bool did_dns_prefetch_ = false;
  bool did_preconnect_ = false;
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

    LocalFrame* frame = GetDocument().GetFrame();
    frame->SetPrescientNetworkingForTesting(
        std::make_unique<MockPrescientNetworking>());
    mock_network_hints_ =
        static_cast<MockPrescientNetworking*>(frame->PrescientNetworking());

    constexpr const char kTestUrl[] = "https://example.com/test.html";
    main_resource_ = std::make_unique<SimRequest>(kTestUrl, "text/html");
    LoadURL(kTestUrl);
  }

 protected:
  MockPrescientNetworking* mock_network_hints_ = nullptr;
  std::unique_ptr<SimRequest> main_resource_;
};

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/1141478)
#define MAYBE_DOMParser DISABLED_DOMParser
#else
#define MAYBE_DOMParser DOMParser
#endif  // BUILDFLAG(IS_IOS)
TEST_F(HTMLPreloadScannerDocumentTest, MAYBE_DOMParser) {
  main_resource_->Complete(R"(<script>
    var p = new DOMParser();
    p.parseFromString(
      '<link rel="preconnect" href="https://target.example.com/"/>',
      'text/html');
  </script>)");

  EXPECT_FALSE(mock_network_hints_->DidDnsPrefetch());
  EXPECT_FALSE(mock_network_hints_->DidPreconnect());
}

TEST_F(HTMLPreloadScannerDocumentTest, DetachedDocumentInnerHTML) {
  main_resource_->Complete(R"(<script>
    var doc = document.implementation.createHTMLDocument('');
    doc.body.innerHTML =
        '<link rel="preconnect" href="https://target.example.com/"/>';
  </script>)");

  EXPECT_FALSE(mock_network_hints_->DidDnsPrefetch());
  EXPECT_FALSE(mock_network_hints_->DidPreconnect());
}

TEST_F(HTMLPreloadScannerDocumentTest, XHRResponseDocument) {
  main_resource_->Complete(R"(<script>
    var xhr = new XMLHttpRequest();
    xhr.open('GET', 'data:text/html,' +
        '<link rel="preconnect" href="https://target.example.com/"/>');
    xhr.responseType = 'document';
    xhr.send();
  </script>)");

  EXPECT_FALSE(mock_network_hints_->DidDnsPrefetch());
  EXPECT_FALSE(mock_network_hints_->DidPreconnect());
}

TEST_F(HTMLPreloadScannerDocumentTest,
       SetsClientHintsPreferencesOnFrameDelegateCH) {
  // Create a prefetch only document since that will ensure only the preload
  // scanner runs.
  ProvideNoStatePrefetchClientTo(
      *GetDocument().GetPage(), MakeGarbageCollected<MockNoStatePrefetchClient>(
                                    *GetDocument().GetPage()));
  EXPECT_TRUE(GetDocument().IsPrefetchOnly());
  main_resource_->Complete(
      R"(<meta http-equiv="Delegate-CH" content="sec-ch-dpr">)");
  EXPECT_TRUE(GetDocument().GetFrame()->GetClientHintsPreferences().ShouldSend(
      network::mojom::WebClientHintsType::kDpr));
}

TEST_F(HTMLPreloadScannerDocumentTest,
       SetsClientHintsPreferencesOnFrameAcceptCH) {
  // Create a prefetch only document since that will ensure only the preload
  // scanner runs.
  ProvideNoStatePrefetchClientTo(
      *GetDocument().GetPage(), MakeGarbageCollected<MockNoStatePrefetchClient>(
                                    *GetDocument().GetPage()));
  EXPECT_TRUE(GetDocument().IsPrefetchOnly());
  main_resource_->Complete(
      R"(<meta http-equiv="Accept-CH" content="sec-ch-dpr">)");
  EXPECT_TRUE(GetDocument().GetFrame()->GetClientHintsPreferences().ShouldSend(
      network::mojom::WebClientHintsType::kDpr));
}

}  // namespace blink
