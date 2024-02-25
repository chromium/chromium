// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_resource_preloader.h"

#include <memory>
#include <utility>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/parser/preload_request.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

struct HTMLResourcePreconnectTestCase {
  const char* base_url;
  const char* url;
  bool is_cors;
  bool is_https;
};

class PreloaderNetworkHintsMock : public WebPrescientNetworking {
 public:
  PreloaderNetworkHintsMock() : did_preconnect_(false) {}

  void PrefetchDNS(const WebURL& url) override {}
  void Preconnect(const WebURL& url, bool allow_credentials) override {
    did_preconnect_ = true;
    is_https_ = url.ProtocolIs("https");
    allow_credentials_ = allow_credentials;
  }

  bool DidPreconnect() { return did_preconnect_; }
  bool IsHTTPS() { return is_https_; }
  bool AllowCredentials() { return allow_credentials_; }

 private:
  mutable bool did_preconnect_;
  mutable bool is_https_;
  mutable bool allow_credentials_;
};

class HTMLResourcePreloaderTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    GetFrame().SetPrescientNetworkingForTesting(
        std::make_unique<PreloaderNetworkHintsMock>());
    mock_network_hints_ = static_cast<PreloaderNetworkHintsMock*>(
        GetFrame().PrescientNetworking());
  }

  void Test(HTMLResourcePreconnectTestCase test_case) {
    // TODO(yoav): Need a mock loader here to verify things are happenning
    // beyond preconnect.
    auto preload_request = PreloadRequest::CreateIfNeeded(
        String(), test_case.url, KURL(test_case.base_url), ResourceType::kImage,
        network::mojom::ReferrerPolicy(), ResourceFetcher::kImageNotImageSet,
        nullptr /* exclusion_info */, std::nullopt /* resource_width */,
        std::nullopt /* resource_height */,
        PreloadRequest::kRequestTypePreconnect);
    DCHECK(preload_request);
    if (test_case.is_cors)
      preload_request->SetCrossOrigin(kCrossOriginAttributeAnonymous);
    auto* preloader =
        MakeGarbageCollected<HTMLResourcePreloader>(GetDocument());
    preloader->Preload(std::move(preload_request));
    ASSERT_TRUE(mock_network_hints_->DidPreconnect());
    ASSERT_NE(test_case.is_cors, mock_network_hints_->AllowCredentials());
    ASSERT_EQ(test_case.is_https, mock_network_hints_->IsHTTPS());
  }

  PreloaderNetworkHintsMock* mock_network_hints_ = nullptr;
};

TEST_F(HTMLResourcePreloaderTest, testPreconnect) {
  HTMLResourcePreconnectTestCase test_cases[] = {
      {"http://example.test", "http://example.com", false, false},
      {"http://example.test", "http://example.com", true, false},
      {"http://example.test", "https://example.com", true, true},
      {"http://example.test", "https://example.com", false, true},
      {"http://example.test", "//example.com", false, false},
      {"http://example.test", "//example.com", true, false},
      {"https://example.test", "//example.com", false, true},
      {"https://example.test", "//example.com", true, true},
  };

  for (const auto& test_case : test_cases)
    Test(test_case);
}

}  // namespace blink
