// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_document_subresource_filter.h"

#include <string>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class TestDocumentSubresourceFilter : public WebDocumentSubresourceFilter {
 public:
  explicit TestDocumentSubresourceFilter(LoadPolicy policy)
      : load_policy_(policy) {}

  LoadPolicy GetLoadPolicy(const WebURL& resource_url,
                           mojom::RequestContextType) override {
    std::string resource_path = WebString(KURL(resource_url).GetPath()).Utf8();
    if (std::find(queried_subresource_paths_.begin(),
                  queried_subresource_paths_.end(),
                  resource_path) == queried_subresource_paths_.end()) {
      queried_subresource_paths_.push_back(resource_path);
    }
    String resource_string = resource_url.GetString();
    for (const String& suffix : blacklisted_suffixes_) {
      if (resource_string.EndsWith(suffix))
        return load_policy_;
    }
    return LoadPolicy::kAllow;
  }

  LoadPolicy GetLoadPolicyForWebSocketConnect(const WebURL& url) override {
    return kAllow;
  }

  void ReportDisallowedLoad() override {}

  bool ShouldLogToConsole() override { return false; }

  void AddToBlacklist(const String& suffix) {
    blacklisted_suffixes_.push_back(suffix);
  }

  const std::vector<std::string>& QueriedSubresourcePaths() const {
    return queried_subresource_paths_;
  }

  bool GetIsAssociatedWithAdSubframe() const override { return false; }

 private:
  // Using STL types for compatibility with gtest/gmock.
  std::vector<std::string> queried_subresource_paths_;
  Vector<String> blacklisted_suffixes_;
  LoadPolicy load_policy_;
};

class SubresourceFilteringWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  void DidStartProvisionalLoad(
      WebDocumentLoader* data_source,
      WebURLRequest& request,
      mojo::ScopedMessagePipeHandle navigation_initiator_handle) override {
    // Normally, the filter should be set when the load is committed. For
    // the sake of this test, however, inject it earlier to verify that it
    // is not consulted for the main resource load.
    subresource_filter_ =
        new TestDocumentSubresourceFilter(load_policy_for_next_load_);
    subresource_filter_->AddToBlacklist("1x1.png");
    data_source->SetSubresourceFilter(subresource_filter_);
  }

  void SetLoadPolicyFromNextLoad(
      TestDocumentSubresourceFilter::LoadPolicy policy) {
    load_policy_for_next_load_ = policy;
  }
  const TestDocumentSubresourceFilter* SubresourceFilter() const {
    return subresource_filter_;
  }

 private:
  // Weak, owned by WebDocumentLoader.
  TestDocumentSubresourceFilter* subresource_filter_ = nullptr;
  TestDocumentSubresourceFilter::LoadPolicy load_policy_for_next_load_;
};

}  // namespace

class WebDocumentSubresourceFilterTest : public testing::Test {
 protected:
  WebDocumentSubresourceFilterTest() : base_url_("http://internal.test/") {
    RegisterMockedHttpURLLoad("white-1x1.png");
    RegisterMockedHttpURLLoad("foo_with_image.html");
    web_view_helper_.Initialize(&client_);
  }

  void LoadDocument(TestDocumentSubresourceFilter::LoadPolicy policy) {
    client_.SetLoadPolicyFromNextLoad(policy);
    frame_test_helpers::LoadFrame(MainFrame(),
                                  BaseURL() + "foo_with_image.html");
  }

  void ExpectSubresourceWasLoaded(bool loaded) {
    WebElement web_element = MainFrame()->GetDocument().QuerySelector("img");
    HTMLImageElement* image_element = ToHTMLImageElement(web_element);
    EXPECT_EQ(loaded, !!image_element->naturalWidth());
  }

  const std::string& BaseURL() const { return base_url_; }
  WebLocalFrameImpl* MainFrame() { return web_view_helper_.LocalMainFrame(); }
  const std::vector<std::string>& QueriedSubresourcePaths() const {
    return client_.SubresourceFilter()->QueriedSubresourcePaths();
  }

 private:
  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  // testing::Test:
  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  SubresourceFilteringWebFrameClient client_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  std::string base_url_;
};

TEST_F(WebDocumentSubresourceFilterTest, AllowedSubresource) {
  LoadDocument(TestDocumentSubresourceFilter::kAllow);
  ExpectSubresourceWasLoaded(true);
  // The filter should not be consulted for the main document resource.
  EXPECT_THAT(QueriedSubresourcePaths(),
              testing::ElementsAre("/white-1x1.png"));
}

TEST_F(WebDocumentSubresourceFilterTest, DisallowedSubresource) {
  LoadDocument(TestDocumentSubresourceFilter::kDisallow);
  ExpectSubresourceWasLoaded(false);
}

TEST_F(WebDocumentSubresourceFilterTest, FilteringDecisionIsMadeLoadByLoad) {
  for (const TestDocumentSubresourceFilter::LoadPolicy policy :
       {TestDocumentSubresourceFilter::kDisallow,
        TestDocumentSubresourceFilter::kAllow,
        TestDocumentSubresourceFilter::kWouldDisallow}) {
    SCOPED_TRACE(testing::Message() << "First load policy= " << policy);

    LoadDocument(policy);
    ExpectSubresourceWasLoaded(policy !=
                               TestDocumentSubresourceFilter::kDisallow);
    EXPECT_THAT(QueriedSubresourcePaths(),
                testing::ElementsAre("/white-1x1.png"));

    WebCache::Clear();
  }
}

}  // namespace blink
