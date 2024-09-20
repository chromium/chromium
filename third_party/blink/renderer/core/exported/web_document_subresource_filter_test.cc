// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_document_subresource_filter.h"

#include "base/containers/contains.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class TestDocumentSubresourceFilter : public WebDocumentSubresourceFilter {
 public:
  explicit TestDocumentSubresourceFilter(LoadPolicy policy)
      : load_policy_(policy) {}

  LoadPolicy GetLoadPolicy(const WebURL& resource_url,
                           mojom::blink::RequestContextType) override {
    String resource_path = KURL(resource_url).GetPath().ToString();
    if (!base::Contains(queried_subresource_paths_, resource_path)) {
      queried_subresource_paths_.push_back(resource_path);
    }
    String resource_string = resource_url.GetString();
    for (const String& suffix : blocklisted_suffixes_) {
      if (resource_string.EndsWith(suffix))
        return load_policy_;
    }
    return LoadPolicy::kAllow;
  }

  LoadPolicy GetLoadPolicyForWebSocketConnect(const WebURL& url) override {
    return kAllow;
  }

  LoadPolicy GetLoadPolicyForWebTransportConnect(const WebURL&) override {
    return kAllow;
  }

  void ReportDisallowedLoad() override {}

  bool ShouldLogToConsole() override { return false; }

  void AddToBlocklist(const String& suffix) {
    blocklisted_suffixes_.push_back(suffix);
  }

  const Vector<String>& QueriedSubresourcePaths() const {
    return queried_subresource_paths_;
  }

 private:
  // Using STL types for compatibility with gtest/gmock.
  Vector<String> queried_subresource_paths_;
  Vector<String> blocklisted_suffixes_;
  LoadPolicy load_policy_;
};

class SubresourceFilteringWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  void DidCommitNavigation(
      WebHistoryCommitType commit_type,
      bool should_reset_browser_interface_broker,
      const ParsedPermissionsPolicy& permissions_policy_header,
      const DocumentPolicyFeatureState& document_policy_header) override {
    subresource_filter_ =
        new TestDocumentSubresourceFilter(load_policy_for_next_load_);
    subresource_filter_->AddToBlocklist("1x1.png");
    Frame()->GetDocumentLoader()->SetSubresourceFilter(subresource_filter_);
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
                                  BaseURL().Utf8() + "foo_with_image.html");
  }

  void ExpectSubresourceWasLoaded(bool loaded) {
    WebElement web_element =
        MainFrame()->GetDocument().QuerySelector(AtomicString("img"));
    auto* image_element = To<HTMLImageElement>(web_element.Unwrap<Node>());
    EXPECT_EQ(loaded, !!image_element->naturalWidth());
  }

  const String& BaseURL() const { return base_url_; }
  WebLocalFrameImpl* MainFrame() { return web_view_helper_.LocalMainFrame(); }
  const Vector<String>& QueriedSubresourcePaths() const {
    return client_.SubresourceFilter()->QueriedSubresourcePaths();
  }

 private:
  void RegisterMockedHttpURLLoad(const String& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |web_view_helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString(base_url_), test::CoreTestDataPath(), WebString(file_name));
  }

  // testing::Test:
  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  test::TaskEnvironment task_environment_;
  SubresourceFilteringWebFrameClient client_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  String base_url_;
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
