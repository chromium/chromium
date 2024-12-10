// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_later_util.h"

#include <string>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

const KURL GetSourcePageURL(const String& relative_url) {
  static const String kSourcePageURL = "https://example.com";
  return KURL(kSourcePageURL + relative_url);
}

// The default priority for FetchLater request without FetchPriorityHint or
// RenderBlockingBehavior should be kHigh.
TEST(ComputeFetchLaterLoadPriorityTest, DefaultHigh) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ResourceLoaderOptions options(scope.GetExecutionContext()->GetCurrentWorld());

  ResourceRequest request(GetSourcePageURL("/fetch-later"));
  FetchParameters params(std::move(request), options);

  ResourceLoadPriority computed_priority =
      ComputeFetchLaterLoadPriority(params);
  EXPECT_EQ(computed_priority, ResourceLoadPriority::kHigh);
}

// The priority for FetchLater request with FetchPriorityHint::kAuto should be
// kHigh.
TEST(ComputeFetchLaterLoadPriorityTest, WithFetchPriorityHintAuto) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ResourceLoaderOptions options(scope.GetExecutionContext()->GetCurrentWorld());

  ResourceRequest request(GetSourcePageURL("/fetch-later"));
  request.SetFetchPriorityHint(mojom::blink::FetchPriorityHint::kAuto);
  FetchParameters params(std::move(request), options);

  ResourceLoadPriority computed_priority =
      ComputeFetchLaterLoadPriority(params);
  EXPECT_EQ(computed_priority, ResourceLoadPriority::kHigh);
}

// The priority for FetchLater request with FetchPriorityHint::kLow should be
// kLow.
TEST(ComputeFetchLaterLoadPriorityTest, WithFetchPriorityHintLow) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ResourceLoaderOptions options(scope.GetExecutionContext()->GetCurrentWorld());

  ResourceRequest request(GetSourcePageURL("/fetch-later"));
  request.SetFetchPriorityHint(mojom::blink::FetchPriorityHint::kLow);
  FetchParameters params(std::move(request), options);

  ResourceLoadPriority computed_priority =
      ComputeFetchLaterLoadPriority(params);
  EXPECT_EQ(computed_priority, ResourceLoadPriority::kLow);
}

// The priority for FetchLater request with RenderBlockingBehavior::kBlocking
// should be kHigh.
TEST(ComputeFetchLaterLoadPriorityTest,
     WithFetchPriorityHintLowAndRenderBlockingBehaviorBlocking) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ResourceLoaderOptions options(scope.GetExecutionContext()->GetCurrentWorld());

  ResourceRequest request(GetSourcePageURL("/fetch-later"));
  request.SetFetchPriorityHint(mojom::blink::FetchPriorityHint::kLow);
  FetchParameters params(std::move(request), options);
  params.SetRenderBlockingBehavior(RenderBlockingBehavior::kBlocking);

  ResourceLoadPriority computed_priority =
      ComputeFetchLaterLoadPriority(params);
  EXPECT_EQ(computed_priority, ResourceLoadPriority::kHigh);
}

class DeferredFetchPolicyTestBase : public SimTest {
 protected:
  const String kMainUrl = "https://example.com/";
  const String kCrossSubdomainUrl = "https://test.example.com/";
  const String kCrossDomainUrl = "https://example.org/";

  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kFetchLaterAPI, {
                                      {"use_deferred_fetch_policy", "true"},
                                  });
    SimTest::SetUp();
  }

  using RequestUrlAndDataType = Vector<std::pair<const String, const String>>;
  // Navigates to `root_url` with `html` as response.
  // `url_and_data` provides additional responses to the requests generated by
  // loading `html`.
  void NavigateTo(const String& root_url,
                  const String& html,
                  const RequestUrlAndDataType& url_and_data = {}) {
    // Queues all upcoming requests first.
    SimRequest root_request(root_url, "text/html");
    WTF::Vector<std::unique_ptr<SimRequest>> requests;
    for (const auto& [url, _] : url_and_data) {
      requests.emplace_back(std::make_unique<SimRequest>(url, "text/html"));
    }

    // Simulates loading the root page.
    LoadURL(root_url);
    root_request.Complete(String(html));
    // Simulates loading all other requests from the root page.
    for (size_t i = 0; i < url_and_data.size(); i++) {
      requests[i]->Complete(url_and_data[i].second);
    }

    WaitForNavigation(root_url);
  }

  void WaitForNavigation(const String& url) {
    Compositor().BeginFrame();
    test::RunPendingTasks();
    ASSERT_EQ(url, GetDocument().Url().GetString());
  }

  // Renders a series of sibling <iframe> elements with the given `iframe_urls`.
  String RenderWithIframes(const Vector<String>& iframe_urls) {
    StringBuilder html;
    for (const auto& url : iframe_urls) {
      html.Append("<iframe src=\"");
      html.Append(url);
      html.Append("\"></iframe>");
    }
    return html.ToString();
  }

  LocalFrame* GetMainFrame() { return GetDocument().GetFrame(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

using GetDeferredFetchQuotaSharingFramesTest = DeferredFetchPolicyTestBase;

TEST_F(GetDeferredFetchQuotaSharingFramesTest, Null) {
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(nullptr), testing::IsEmpty());
}

// The main frame shares the deferred fetch quota with itself.
TEST_F(GetDeferredFetchQuotaSharingFramesTest, SingleDocument) {
  NavigateTo(kMainUrl, "");

  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(GetMainFrame()),
              testing::SizeIs(1));
}

// The about::blank iframe shares the deferred fetch quota with the main frame.
TEST_F(GetDeferredFetchQuotaSharingFramesTest, SingleDocumentSingleBlankFrame) {
  NavigateTo(kMainUrl, "<iframe></iframe>");

  auto* root = GetMainFrame();
  auto* blank_frame = root->Tree().FirstChild();
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(root), testing::SizeIs(2));
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(blank_frame),
              testing::SizeIs(2));
}

// The same-origin iframe shares the deferred fetch quota with the main frame.
TEST_F(GetDeferredFetchQuotaSharingFramesTest,
       SingleDocumentSingleSameOriginFrame) {
  // The structure of the document:
  // root -> frame_a (same-origin)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";

  NavigateTo(root_url, RenderWithIframes({"frame-a.html"}),
             {{frame_a_url, ""}});

  // Root and Frame A are same-origin.
  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(root), testing::SizeIs(2));
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_a), testing::SizeIs(2));
}

// The same-origin iframe does not share the deferred fetch quota with the main
// frame.
TEST_F(GetDeferredFetchQuotaSharingFramesTest,
       SingleDocumentSingleCrossOriginFrame) {
  // The structure of the document:
  // root -> frame_a (cross-origin)
  String root_url = kMainUrl;
  String frame_a_url = kCrossSubdomainUrl + "frame-a.html";

  NavigateTo(root_url, RenderWithIframes({frame_a_url}), {{frame_a_url, ""}});

  // Root and Frame A are cross-origin.
  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(root), testing::SizeIs(1));
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_a), testing::SizeIs(1));
}

// The main frame and 3 cross-origin iframes don't share the same deferred
// fetch quota.
TEST_F(GetDeferredFetchQuotaSharingFramesTest,
       MultipleCrossOriginSiblingFrames) {
  // The structure of the document:
  // root -> frame_a (cross-origin)
  //      -> frame_b (cross-origin)
  //      -> frame_c (cross-origin)
  String root_url = kMainUrl;
  String frame_a_url = kCrossSubdomainUrl + "frame-a.html";
  String frame_b_url = kCrossSubdomainUrl + "frame-b.html";
  String frame_c_url = kCrossSubdomainUrl + "frame-c.html";

  NavigateTo(root_url,
             RenderWithIframes({frame_a_url, frame_b_url, frame_c_url}),
             {{frame_a_url, ""}, {frame_b_url, ""}, {frame_c_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();
  auto* frame_b = frame_a->Tree().NextSibling();
  auto* frame_c = frame_b->Tree().NextSibling();

  // Root is its owned origin.
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(root), testing::SizeIs(1));

  // Frame A, B, and C are same-origin.
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_a), testing::SizeIs(3));
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_b), testing::SizeIs(3));
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_c), testing::SizeIs(3));
}

// The same-origin main frame, frame-a, frame-b are in the same frame group for
// deferred fetch quota, while cross-origin frame-c, frame-d are in their own
// group.
TEST_F(GetDeferredFetchQuotaSharingFramesTest,
       MultipleDifferentOriginSiblingFrames) {
  // The structure of the document:
  // root -> frame_a (same-origin)
  //      -> frame_b (same-origin)
  //      -> frame_c (cross-origin)
  //      -> frame_d (cross-origin)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";
  String frame_b_url = kMainUrl + "frame-b.html";
  String frame_c_url = kCrossSubdomainUrl + "frame-c.html";
  String frame_d_url = kCrossSubdomainUrl + "frame-d.html";

  NavigateTo(root_url,
             RenderWithIframes(
                 {"frame-a.html", frame_b_url, frame_c_url, frame_d_url}),
             {{frame_a_url, ""},
              {frame_b_url, ""},
              {frame_c_url, ""},
              {frame_d_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();
  auto* frame_b = frame_a->Tree().NextSibling();
  auto* frame_c = frame_b->Tree().NextSibling();
  auto* frame_d = frame_c->Tree().NextSibling();

  // Root, Frame A and Frame B are same-origin.
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(root), testing::SizeIs(3));
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_a), testing::SizeIs(3));
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_b), testing::SizeIs(3));

  // Frame C and D are same-origin.
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_c), testing::SizeIs(2));
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_d), testing::SizeIs(2));
}

// The same-origin main frame, frame-a, frame-b are in the same frame group for
// deferred fetch quota, while cross-origin frame-c, frame-d are in their own
// group.
TEST_F(GetDeferredFetchQuotaSharingFramesTest, MultipleLevelFrames) {
  // The structure of the document:
  // root -> frame_a (same-origin) -> frame_c (cross-origin)
  //      -> frame_d (cross-origin) -> frame_b (same-origin)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";
  String frame_b_url = kMainUrl + "frame-b.html";
  String frame_c_url = kCrossSubdomainUrl + "frame-c.html";
  String frame_d_url = kCrossSubdomainUrl + "frame-d.html";

  NavigateTo(root_url, RenderWithIframes({"frame-a.html", frame_d_url}),
             {{frame_a_url, RenderWithIframes({frame_c_url})},
              {frame_d_url, RenderWithIframes({frame_b_url})},
              {frame_c_url, ""},
              {frame_b_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();
  auto* frame_d = frame_a->Tree().NextSibling();
  auto* frame_c = frame_a->Tree().FirstChild();
  auto* frame_b = frame_d->Tree().FirstChild();

  // Root, Frame A and Frame B are same-origin.
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(root), testing::SizeIs(3));
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_a), testing::SizeIs(3));
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_b), testing::SizeIs(3));

  // Frame C and D are same-origin.
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_c), testing::SizeIs(2));
  EXPECT_THAT(GetDeferredFetchQuotaSharingFrames(frame_d), testing::SizeIs(2));
}

class CountFramesWithMinimalQuotaPolicyTest
    : public DeferredFetchPolicyTestBase {
 protected:
  [[nodiscard]] size_t CountFramesWithMinimalQuotaPolicyFor(
      Frame* target_frame) {
    // Should only be called by content of an iframe.
    CHECK(target_frame->Owner());
    // Should not be called by frame without permissions policy
    // `deferred-fetch`.
    CHECK(!target_frame->GetSecurityContext()
               ->GetPermissionsPolicy()
               ->IsFeatureEnabledForOrigin(
                   mojom::blink::PermissionsPolicyFeature::kDeferredFetch,
                   target_frame->GetSecurityContext()
                       ->GetSecurityOrigin()
                       ->ToUrlOrigin()));

    // `GetDeferredFetchQuotaSharingFrames()` must be run on the
    // `container’s container document`.
    auto top_level_relatives =
        GetDeferredFetchQuotaSharingFrames(target_frame->Parent());
    return CountFramesWithMinimalQuotaPolicyForTesting(target_frame->Owner(),
                                                       top_level_relatives);
  }
};

// The single cross-origin iframe has default `deferred-fetch-minimal` policy
// enabled `*`. However, there is no other cross-origin iframs share this quota
// with it.
TEST_F(CountFramesWithMinimalQuotaPolicyTest, SingleCrossOriginFrame) {
  // The structure of the document:
  // root -> frame_a (cross-origin)
  String root_url = kMainUrl;
  String frame_a_url = kCrossSubdomainUrl + "frame-a.html";
  NavigateTo(root_url, RenderWithIframes({frame_a_url}), {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();

  // Expects no other frames share the minimal quota policy with `frame_a`.
  EXPECT_EQ(CountFramesWithMinimalQuotaPolicyFor(frame_a), 0u);
}

TEST_F(CountFramesWithMinimalQuotaPolicyTest,
       MultipleDifferentOriginSiblingFrames) {
  // The structure of the document:
  // root -> frame_a (same-origin)
  //      -> frame_b (same-origin)
  //      -> frame_c (cross-origin)
  //      -> frame_d (cross-origin)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";
  String frame_b_url = kMainUrl + "frame-b.html";
  String frame_c_url = kCrossSubdomainUrl + "frame-c.html";
  String frame_d_url = kCrossSubdomainUrl + "frame-d.html";

  NavigateTo(root_url,
             RenderWithIframes(
                 {"frame-a.html", frame_b_url, frame_c_url, frame_d_url}),
             {{frame_a_url, ""},
              {frame_b_url, ""},
              {frame_c_url, ""},
              {frame_d_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();
  auto* frame_b = frame_a->Tree().NextSibling();
  auto* frame_c = frame_b->Tree().NextSibling();
  auto* frame_d = frame_c->Tree().NextSibling();

  // Frame A and Frame B are same-origin with root and are not counted toward
  // minimal quota policy. Hence, they cannot be used in
  // `CountFramesWithMinimalQuotaPolicyFor()`.

  // Frame C and D are different origin with root, and shares the minimal quota
  // policy with each other.
  EXPECT_EQ(CountFramesWithMinimalQuotaPolicyFor(frame_c), 1u);
  EXPECT_EQ(CountFramesWithMinimalQuotaPolicyFor(frame_d), 1u);
}

using GetContainerDeferredFetchPolicyOnNavigationTest =
    DeferredFetchPolicyTestBase;

// Tests the default behavior of a document with a same-origin iframe.
// It should have deferred fetch policy in the owner frame set to
// `kDeferredFetch`. The after-navigation call to
// `GetContainerDeferredFetchPolicyOnNavigation()` should also give the same
// result.
TEST_F(GetContainerDeferredFetchPolicyOnNavigationTest, SingleSameOriginFrame) {
  // The structure of the document:
  // root -> frame_a (same-origin)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";
  NavigateTo(root_url, RenderWithIframes({"frame-a.html"}),
             {{frame_a_url, ""}});
  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();

  // `GetContainerDeferredFetchPolicyOnNavigation()` should have been executed
  // when iframe is loaded.
  EXPECT_EQ(frame_a->Owner()->GetFramePolicy().deferred_fetch_policy,
            FramePolicy::DeferredFetchPolicy::kDeferredFetch);
  EXPECT_EQ(GetContainerDeferredFetchPolicyOnNavigation(frame_a->Owner()),
            FramePolicy::DeferredFetchPolicy::kDeferredFetch);
}

// Tests the default behavior of a document with a same-origin iframe.
// It should have deferred fetch policy in the owner frame set to
// `kDeferredFetchMinimal`. The after-navigation call to
// `GetContainerDeferredFetchPolicyOnNavigation()` should also give the same
// result.
TEST_F(GetContainerDeferredFetchPolicyOnNavigationTest,
       SingleCrossOriginFrame) {
  // The structure of the document:
  // root -> frame_a (cross-origin)
  String root_url = kMainUrl;
  String frame_a_url = kCrossSubdomainUrl + "frame-a.html";
  NavigateTo(root_url, RenderWithIframes({frame_a_url}), {{frame_a_url, ""}});

  // Root and Frame A are cross-origin.
  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();

  EXPECT_EQ(GetContainerDeferredFetchPolicyOnNavigation(frame_a->Owner()),
            FramePolicy::DeferredFetchPolicy::kDeferredFetchMinimal);
}

// Tests the default behavior of a document with 17 cross-origin sibling
// iframes. The first 16 iframes should get kDeferredFetchMinimal as their
// deferred fetch policy, while the last one should be kDisabled.
TEST_F(GetContainerDeferredFetchPolicyOnNavigationTest, ManyCrossOriginFrames) {
  // The structure of the document:
  // root -> frame_1 (cross-origin)
  //      -> ...
  //      -> frame_16 (cross-origin)
  //      -> frame_17 (cross-origin)
  const size_t kNumCrossOriginFrames = 17;
  String root_url = kMainUrl;

  Vector<String> frame_urls;
  RequestUrlAndDataType frame_url_and_data;
  for (size_t i = 0; i < kNumCrossOriginFrames; i++) {
    auto frame_id = i + 1;
    frame_urls.emplace_back(kCrossSubdomainUrl + "frame-" +
                            String::Number(frame_id) + ".html");
    frame_url_and_data.emplace_back(std::make_pair(frame_urls[i], ""));
  }

  NavigateTo(root_url, RenderWithIframes(frame_urls), frame_url_and_data);

  auto* root = GetMainFrame();
  auto* frame = root->Tree().FirstChild();
  size_t i = 0;
  while (i < kNumCrossOriginFrames - 1 && frame) {
    EXPECT_EQ(frame->Owner()->GetFramePolicy().deferred_fetch_policy,
              FramePolicy::DeferredFetchPolicy::kDeferredFetchMinimal)
        << i + 1 << "-th cross-origin iframe";
    frame = frame->Tree().NextSibling();
    i++;
  }

  // The last cross-origin iframe should have `kDisabled` set.
  EXPECT_EQ(frame->Owner()->GetFramePolicy().deferred_fetch_policy,
            FramePolicy::DeferredFetchPolicy::kDisabled)
      << i + 1 << "-th cross-origin iframe";
}

}  // namespace
}  // namespace blink
