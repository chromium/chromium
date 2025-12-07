// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_later_util.h"

#include <string>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/deferred_fetch_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/fetch_later_test_util.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
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

const KURL GetSourcePageURL(const String& relative_url) {
  static const String kSourcePageURL = "https://example.com";
  return KURL(kSourcePageURL + relative_url);
}

scoped_refptr<const SecurityOrigin> GetOrigin(const String& url) {
  return SecurityOrigin::Create(KURL(url));
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

// DeferredFetchPolicyTestBase supports setting up a page with multiple iframes
// for testing against their `FramePolicy.deferred_fetch_policy`.
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
    Vector<std::unique_ptr<SimRequest>> requests;
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
  static String RenderWithIframes(
      const Vector<String>& iframe_urls,
      const String& sandbox_policy = g_empty_string) {
    StringBuilder html;
    for (const auto& url : iframe_urls) {
      html.Append("<iframe src=\"");
      html.Append(url);
      if (!sandbox_policy.empty()) {
        html.Append("\" sandbox=\"");
        html.Append(sandbox_policy);
      }
      html.Append("\"></iframe>");
    }
    return html.ToString();
  }

  static void CheckFrameEnableDeferredFetchMinimal(Frame* frame) {
    CHECK(
        frame->GetSecurityContext()
            ->GetPermissionsPolicy()
            ->IsFeatureEnabledForOrigin(
                network::mojom::PermissionsPolicyFeature::kDeferredFetchMinimal,
                frame->GetSecurityContext()
                    ->GetSecurityOrigin()
                    ->ToUrlOrigin()));
  }

  LocalFrame* GetMainFrame() { return GetDocument().GetFrame(); }

  // SimTest overrides:
  std::unique_ptr<frame_test_helpers::TestWebFrameClient>
  CreateWebFrameClientForMainFrame() override {
    auto client = std::make_unique<FakeWebFrameClient>();
    client->GetLoaderFactoryBundle()->SetFetchLaterLoaderFactory(
        factory_.BindNewEndpointAndPassDedicatedRemote());
    return client;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  MockFetchLaterLoaderFactory factory_;
};

class CountDescendantsWithReservedMinimalQuotaTest
    : public DeferredFetchPolicyTestBase {
 protected:
  [[nodiscard]] static size_t CountDescendantsWithReservedMinimalQuotaFor(
      Frame* target_frame) {
    // Should only be called by content of an iframe.
    CHECK(target_frame->Owner());
    // Should not be called by frame without permissions policy
    // `deferred-fetch-minimal`.
    CheckFrameEnableDeferredFetchMinimal(target_frame);
    // Let controlDocument be container’s node document’s deferred-fetch control
    // document.
    auto* control_frame =
        FetchLaterUtil::GetDeferredFetchControlFrame(target_frame->Parent());

    return FetchLaterUtil::CountDescendantsWithReservedMinimalQuota(
        control_frame);
  }
};

// The single cross-origin iframe has default `deferred-fetch-minimal` policy
// enabled `*`. However, there is no other cross-origin iframe shares this quota
// with it.
TEST_F(CountDescendantsWithReservedMinimalQuotaTest, SingleCrossOriginFrame) {
  // The structure of the document:
  // root -> frame_a (cross-origin)
  String root_url = kMainUrl;
  String frame_a_url = kCrossSubdomainUrl + "frame-a.html";
  NavigateTo(root_url, RenderWithIframes({frame_a_url}), {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();

  // Expects only `frame_a` share the minimal quota policy.
  EXPECT_EQ(CountDescendantsWithReservedMinimalQuotaFor(frame_a), 1u);
}

TEST_F(CountDescendantsWithReservedMinimalQuotaTest,
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
  // `CountDescendantsWithReservedMinimalQuotaFor()`.

  // Frame C and D are different origin with root, and shares the minimal quota
  // policy with each other.
  EXPECT_EQ(CountDescendantsWithReservedMinimalQuotaFor(frame_c), 2u);
  EXPECT_EQ(CountDescendantsWithReservedMinimalQuotaFor(frame_d), 2u);
}

TEST_F(CountDescendantsWithReservedMinimalQuotaTest, MultipleLevelFrames) {
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

  // Frame A and Frame B are same-origin with root and are not counted toward
  // minimal quota policy. Hence, they cannot be used in
  // `CountDescendantsWithReservedMinimalQuotaFor()`.

  // Frame C and D are different origin with root, and shares the minimal quota
  // policy with each other.
  EXPECT_EQ(CountDescendantsWithReservedMinimalQuotaFor(frame_c), 2u);
  EXPECT_EQ(CountDescendantsWithReservedMinimalQuotaFor(frame_d), 2u);
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
  // when iframe A is loaded, which sets the policy to `kDeferredFetch`. After
  // that, `ShouldClearDeferredFetchPolicy()` leads A to clear its policy.
  // Hence, the final value is `kDisabled.
  EXPECT_EQ(frame_a->Owner()->GetFramePolicy().deferred_fetch_policy,
            mojom::blink::DeferredFetchPolicy::kDisabled);
  // Manually calling `GetContainerDeferredFetchPolicyOnNavigation()` again to
  // verify it actually returns `kDeferredFetch`.
  EXPECT_EQ(FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
                frame_a->Owner(), GetOrigin(frame_a_url)),
            mojom::blink::DeferredFetchPolicy::kDeferredFetch);
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

  EXPECT_EQ(FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
                frame_a->Owner(), GetOrigin(frame_a_url)),
            mojom::blink::DeferredFetchPolicy::kDeferredFetchMinimal);
}

TEST_F(GetContainerDeferredFetchPolicyOnNavigationTest,
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

  EXPECT_EQ(FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
                frame_a->Owner(), GetOrigin(frame_a_url)),
            mojom::blink::DeferredFetchPolicy::kDeferredFetch);
  EXPECT_EQ(FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
                frame_b->Owner(), GetOrigin(frame_b_url)),
            mojom::blink::DeferredFetchPolicy::kDeferredFetch);

  // Frame C and Frame D should have minimal quota policy set.
  EXPECT_EQ(FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
                frame_c->Owner(), GetOrigin(frame_c_url)),
            mojom::blink::DeferredFetchPolicy::kDeferredFetchMinimal);
  EXPECT_EQ(FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
                frame_d->Owner(), GetOrigin(frame_d_url)),
            mojom::blink::DeferredFetchPolicy::kDeferredFetchMinimal);
}

TEST_F(GetContainerDeferredFetchPolicyOnNavigationTest, MultipleLevelFrames) {
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

  EXPECT_EQ(FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
                frame_a->Owner(), GetOrigin(frame_a_url)),
            mojom::blink::DeferredFetchPolicy::kDeferredFetch);
  // Frame B will have NO quota, as
  // (1) its "inherited policy" from its parent Frame D, which is a cross-origin
  // iframe, will not have "deferred-fetch" policy enabled by default but only
  // "deferred-fetch-minimal".
  // (2) its parent Frame D does not share same quota with root frame.
  EXPECT_EQ(FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
                frame_b->Owner(), GetOrigin(frame_b_url)),
            mojom::blink::DeferredFetchPolicy::kDisabled);

  // Frame C and Frame D should have minimal quota policy set.
  EXPECT_EQ(FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
                frame_c->Owner(), GetOrigin(frame_c_url)),
            mojom::blink::DeferredFetchPolicy::kDeferredFetchMinimal);
  EXPECT_EQ(FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
                frame_d->Owner(), GetOrigin(frame_d_url)),
            mojom::blink::DeferredFetchPolicy::kDeferredFetchMinimal);
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
              mojom::blink::DeferredFetchPolicy::kDeferredFetchMinimal)
        << i + 1 << "-th cross-origin iframe";
    frame = frame->Tree().NextSibling();
    i++;
  }

  // The last cross-origin iframe should have `kDisabled` set.
  EXPECT_EQ(frame->Owner()->GetFramePolicy().deferred_fetch_policy,
            mojom::blink::DeferredFetchPolicy::kDisabled)
      << i + 1 << "-th cross-origin iframe";
}

// Tests that a sandboxed iframe (without allow-same-origin) should receive the
// `kDeferredFetchMinimal` policy by default as it's considered cross-origin.
TEST_F(GetContainerDeferredFetchPolicyOnNavigationTest,
       SandboxedIframeGetsMinimalPolicyByDefault) {
  // The structure of the document:
  // root -> frame_a (sandboxed)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";
  NavigateTo(root_url, RenderWithIframes({frame_a_url}, "allow-scripts"),
             {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();
  // Simulates a navigation target in a sandbox frame.
  auto to_origin = GetOrigin(frame_a_url)->DeriveNewOpaqueOrigin();

  EXPECT_EQ(FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
                frame_a->Owner(), to_origin),
            mojom::blink::DeferredFetchPolicy::kDeferredFetchMinimal);
}

// Tests that a sandboxed iframe with 'allow-same-origin' receives the
// `kDeferredFetch` policy, as it is treated as a same-origin frame.
TEST_F(GetContainerDeferredFetchPolicyOnNavigationTest,
       SandboxedIframeWithAllowSameOriginGetsNormalPolicy) {
  // The structure of the document:
  // root -> frame_a (sandboxed, allow-same-origin)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";
  NavigateTo(
      root_url,
      RenderWithIframes({frame_a_url}, "allow-scripts allow-same-origin"),
      {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();
  // Simulates a navigation target in a sandbox "allow-same-origin" frame.
  auto to_origin = GetOrigin(frame_a_url);

  EXPECT_EQ(FetchLaterUtil::GetContainerDeferredFetchPolicyOnNavigation(
                frame_a->Owner(), to_origin),
            mojom::blink::DeferredFetchPolicy::kDeferredFetch);
}

class GetDeferredFetchControlFrameTest : public DeferredFetchPolicyTestBase {};

TEST_F(GetDeferredFetchControlFrameTest, SingleDocument) {
  auto new_request_url = KURL(kMainUrl + "test.html");
  NavigateTo(kMainUrl, "");

  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(GetMainFrame()),
            GetMainFrame());
}

TEST_F(GetDeferredFetchControlFrameTest, SingleSameOriginFrame) {
  auto new_request_url = KURL(kMainUrl + "test.html");
  // The structure of the document:
  // root -> frame_a (same-origin)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";

  NavigateTo(root_url, RenderWithIframes({frame_a_url}), {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();

  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(root), root);
  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(frame_a), root);
}

TEST_F(GetDeferredFetchControlFrameTest, SingleCrossOriginFrame) {
  // The structure of the document:
  // root -> frame_a (cross-origin)
  String root_url = kMainUrl;
  String frame_a_url = kCrossSubdomainUrl + "frame-a.html";
  NavigateTo(root_url, RenderWithIframes({frame_a_url}), {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();

  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(root), root);
  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(frame_a), frame_a);
}

TEST_F(GetDeferredFetchControlFrameTest, MultipleDifferentOriginSiblingFrames) {
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

  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(root), root);
  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(frame_a), root);
  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(frame_b), root);
  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(frame_c), frame_c);
  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(frame_d), frame_d);
}

TEST_F(GetDeferredFetchControlFrameTest, MultipleLevelFrames) {
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

  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(root), root);
  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(frame_a), root);
  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(frame_b), frame_b);
  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(frame_c), frame_c);
  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(frame_d), frame_d);
}

// Tests that a sandboxed iframe (without allow-same-origin) is correctly
// identified as its own deferred-fetch control frame, as it's cross-origin.
TEST_F(GetDeferredFetchControlFrameTest, SandboxedIframeIsItsOwnControlFrame) {
  // The structure of the document:
  // root -> frame_a (sandboxed)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";
  NavigateTo(root_url, RenderWithIframes({frame_a_url}, "allow-scripts"),
             {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();

  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(root), root);
  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(frame_a), frame_a);
}

// Tests that a sandboxed iframe with 'allow-same-origin' shares its parent's
// deferred-fetch control frame.
TEST_F(GetDeferredFetchControlFrameTest,
       SandboxedIframeWithAllowSameOriginSharesControlFrame) {
  // The structure of the document:
  // root -> frame_a (sandboxed, allow-same-origin)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";
  NavigateTo(
      root_url,
      RenderWithIframes({frame_a_url}, "allow-scripts allow-same-origin"),
      {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();

  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(root), root);
  EXPECT_EQ(FetchLaterUtil::GetDeferredFetchControlFrame(frame_a), root);
}

class ToReservedDeferredFetchQuotaTest : public DeferredFetchPolicyTestBase {};

TEST_F(ToReservedDeferredFetchQuotaTest, PolicyDisabled) {
  EXPECT_EQ(FetchLaterUtil::ToReservedDeferredFetchQuota(
                mojom::blink::DeferredFetchPolicy::kDisabled),
            0u);
}

TEST_F(ToReservedDeferredFetchQuotaTest, PolicyDeferredFetch) {
  EXPECT_EQ(FetchLaterUtil::ToReservedDeferredFetchQuota(
                mojom::blink::DeferredFetchPolicy::kDeferredFetch),
            kNormalReservedDeferredFetchQuota);
}

TEST_F(ToReservedDeferredFetchQuotaTest, PolicyDeferredFetchMinimal) {
  EXPECT_EQ(FetchLaterUtil::ToReservedDeferredFetchQuota(
                mojom::blink::DeferredFetchPolicy::kDeferredFetchMinimal),
            kMinimalReservedDeferredFetchQuota);
}

class AreSameOriginTest : public DeferredFetchPolicyTestBase {
};

TEST_F(AreSameOriginTest, MultipleDifferentOriginSiblingFrames) {
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
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(root, frame_a));
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(frame_a, root));
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(root, frame_b));
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(frame_b, root));
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(frame_a, frame_b));
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(frame_b, frame_a));

  // Frame C and D are different origin with root, and shares the minimal quota
  // policy with each other.
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(frame_c, frame_d));
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(frame_d, frame_c));

  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(root, frame_c));
  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(root, frame_d));
  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(frame_a, frame_c));
  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(frame_a, frame_d));
  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(frame_b, frame_c));
  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(frame_b, frame_d));
}

TEST_F(AreSameOriginTest, MultipleLevelFrames) {
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
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(root, frame_a));
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(frame_a, root));
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(root, frame_b));
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(frame_b, root));
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(frame_a, frame_b));
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(frame_b, frame_a));

  // Frame C and D are different origin with root, and shares the minimal quota
  // policy with each other.
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(frame_c, frame_d));
  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(frame_d, frame_c));

  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(root, frame_c));
  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(root, frame_d));
  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(frame_a, frame_c));
  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(frame_a, frame_d));
  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(frame_b, frame_c));
  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(frame_b, frame_d));
}

// Tests that a sandboxed iframe (without allow-same-origin) is correctly
// identified as cross-origin to its parent.
TEST_F(AreSameOriginTest, SandboxedIframeIsCrossOrigin) {
  // The structure of the document:
  // root -> frame_a (sandboxed)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";
  NavigateTo(root_url, RenderWithIframes({frame_a_url}, "allow-scripts"),
             {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();

  EXPECT_FALSE(FetchLaterUtil::AreSameOrigin(root, frame_a));
}

// Tests that a sandboxed iframe with the 'allow-same-origin' token is
// correctly identified as same-origin with its parent.
TEST_F(AreSameOriginTest, SandboxedIframeWithAllowSameOriginIsSameOrigin) {
  // The structure of the document:
  // root -> frame_a (sandboxed, allow-same-origin)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";
  NavigateTo(
      root_url,
      RenderWithIframes({frame_a_url}, "allow-scripts allow-same-origin"),
      {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();

  EXPECT_TRUE(FetchLaterUtil::AreSameOrigin(root, frame_a));
}

using ShouldClearDeferredFetchPolicyTest = DeferredFetchPolicyTestBase;

TEST_F(ShouldClearDeferredFetchPolicyTest,
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

  EXPECT_FALSE(FetchLaterUtil::ShouldClearDeferredFetchPolicy(root));
  // Frame A and root are same origin, so Frame A should clear its deferred
  // fetch policy inherited from root.
  EXPECT_TRUE(FetchLaterUtil::ShouldClearDeferredFetchPolicy(frame_a));
  // Frame B and root are same origin, so Frame B should clear its deferred
  // fetch policy inherited from root.
  EXPECT_TRUE(FetchLaterUtil::ShouldClearDeferredFetchPolicy(frame_b));
  // Frame C and root are different origin, so Frame C should keep its owned
  // deferred fetch policy.
  EXPECT_FALSE(FetchLaterUtil::ShouldClearDeferredFetchPolicy(frame_c));
  // Frame D and root are different origin, so Frame D should keep its owned
  // deferred fetch policy.
  EXPECT_FALSE(FetchLaterUtil::ShouldClearDeferredFetchPolicy(frame_d));
}

TEST_F(ShouldClearDeferredFetchPolicyTest, MultipleLevelFrames) {
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

  EXPECT_FALSE(FetchLaterUtil::ShouldClearDeferredFetchPolicy(root));
  // Frame A and root are same origin, so Frame A should clear its deferred
  // fetch policy inherited from root.
  EXPECT_TRUE(FetchLaterUtil::ShouldClearDeferredFetchPolicy(frame_a));
  EXPECT_FALSE(FetchLaterUtil::ShouldClearDeferredFetchPolicy(frame_b));
  EXPECT_FALSE(FetchLaterUtil::ShouldClearDeferredFetchPolicy(frame_c));
  // Frame D and root are different origin, so Frame D should keep its owned
  // deferred fetch policy.
  EXPECT_FALSE(FetchLaterUtil::ShouldClearDeferredFetchPolicy(frame_d));
}

class GetReservedDeferredFetchQuotaTest : public DeferredFetchPolicyTestBase {
 protected:
  // This helper method obtains the available deferred-fetch quota for the
  // `source_frame` when making a request to `url` from `source_frame`.
  [[nodiscard]] uint64_t GetAvailableQuota(Frame* source_frame,
                                           const KURL& url) {
    return FetchLaterUtil::GetAvailableDeferredFetchQuota(source_frame, url);
  }
};

// The input control frame is a top-level frame. The quota is
// `The default of 640 kibibytes, decremented by quota reserved for
// deferred-fetch-minimal`.
TEST_F(GetReservedDeferredFetchQuotaTest, IsTopLevelControlFrame) {
  auto new_request_url = KURL(kMainUrl + "test.html");
  NavigateTo(kMainUrl, "");

  EXPECT_EQ(FetchLaterUtil::GetReservedDeferredFetchQuota(GetMainFrame()),
            kMaxScheduledDeferredBytes - kQuotaReservedForDeferredFetchMinimal);
}

// The input frame is a non top-level frame where its owner frame has its
// deferred-fetch policy cleared.
// This test case should not happen in practice, as normally the input should be
// a control frame, i.e. the same-origin ancestor frame of `frame_a`.
TEST_F(GetReservedDeferredFetchQuotaTest,
       IsNonTopLevelFrameWithOwnerDeferredFetchPolicyDisabled) {
  auto new_request_url = KURL(kMainUrl + "test.html");
  // The structure of the document:
  // root -> frame_a (same-origin)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";

  NavigateTo(root_url, RenderWithIframes({frame_a_url}), {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();
  // `GetContainerDeferredFetchPolicyOnNavigation()` should have been executed
  // when iframe A is loaded, which sets the policy to `kDeferredFetch`. After
  // that, `ShouldClearDeferredFetchPolicy()` leads A to clear its policy.
  // Hence, the final value is `kDisabled.
  EXPECT_EQ(frame_a->Owner()->GetFramePolicy().deferred_fetch_policy,
            mojom::blink::DeferredFetchPolicy::kDisabled);
  EXPECT_EQ(FetchLaterUtil::GetReservedDeferredFetchQuota(frame_a), 0);
}

// The input frame is a non top-level frame where its owner frame has its
// deferred-fetch policy set to `kDeferredFetchMinimal`.
TEST_F(GetReservedDeferredFetchQuotaTest,
       IsNonTopLevelCrossOriginFrameWithOwnerDeferredFetchPolicyMinimal) {
  auto new_request_url = KURL(kMainUrl + "test.html");
  // The structure of the document:
  // root -> frame_a (cross-origin)
  String root_url = kMainUrl;
  String frame_a_url = kCrossSubdomainUrl + "frame-a.html";

  NavigateTo(root_url, RenderWithIframes({frame_a_url}), {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();
  EXPECT_EQ(FetchLaterUtil::GetReservedDeferredFetchQuota(frame_a),
            kMinimalReservedDeferredFetchQuota);
}

class GetAvailableDeferredFetchQuotaTest : public DeferredFetchPolicyTestBase {
 protected:
  // This helper method obtains the available deferred-fetch quota for the
  // `source_frame` when making a request to `url` from `source_frame`.
  [[nodiscard]] uint64_t GetAvailableQuota(Frame* source_frame,
                                           const KURL& url) {
    return FetchLaterUtil::GetAvailableDeferredFetchQuota(source_frame, url);
  }
};

// The main frame origin has a `kMaxPerRequestOriginScheduledDeferredBytes`
// quota for all fetchLater() requests.
TEST_F(GetAvailableDeferredFetchQuotaTest, SingleDocument) {
  auto new_request_url = KURL(kMainUrl + "test.html");
  NavigateTo(kMainUrl, "");

  EXPECT_EQ(GetAvailableQuota(GetMainFrame(), new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
}

// The main frame origin has a `kMaxPerRequestOriginScheduledDeferredBytes`
// quota for all fetchLater() requests.
TEST_F(GetAvailableDeferredFetchQuotaTest, SingleDocumentExistingRequest) {
  auto new_request_url = KURL(kMainUrl + "test.html");
  NavigateTo(kMainUrl, R"HTML(
    <script>fetchLater("https://example.com");</script>
  )HTML");

  EXPECT_EQ(GetAvailableQuota(GetMainFrame(), new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
}

TEST_F(GetAvailableDeferredFetchQuotaTest,
       SingleDocumentSingleSameOriginFrame) {
  auto new_request_url = KURL(kMainUrl + "test.html");
  // The structure of the document:
  // root -> frame_a (same-origin)
  String root_url = kMainUrl;
  String frame_a_url = kMainUrl + "frame-a.html";

  NavigateTo(root_url, RenderWithIframes({frame_a_url}), {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();
  // The main frame has its owned kMaxPerRequestOriginScheduledDeferredBytes
  // quota.
  EXPECT_EQ(GetAvailableQuota(root, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  // Frame A is same-origin and shares the same quota with the main frame.
  EXPECT_EQ(GetAvailableQuota(frame_a, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
}

TEST_F(GetAvailableDeferredFetchQuotaTest,
       SingleDocumentSingleCrossOriginFrame) {
  // The structure of the document:
  // root -> frame_a (cross-origin)
  String root_url = kMainUrl;
  String frame_a_url = kCrossSubdomainUrl + "frame-a.html";

  NavigateTo(root_url, RenderWithIframes({frame_a_url}), {{frame_a_url, ""}});

  auto* root = GetMainFrame();
  auto* frame_a = root->Tree().FirstChild();

  auto new_request_url = KURL(kMainUrl + "test.html");
  // The main frame has its owned kMaxPerRequestOriginScheduledDeferredBytes
  // quota.
  EXPECT_EQ(GetAvailableQuota(root, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  // Frame A is cross-origin and only comes with a small 8KB quota from the
  // "deferred-fetch-minimal" permissions policy.
  EXPECT_EQ(GetAvailableQuota(frame_a, new_request_url),
            kMinimalReservedDeferredFetchQuota);
}

TEST_F(GetAvailableDeferredFetchQuotaTest,
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

  auto new_request_url = KURL(kMainUrl + "test.html");
  // The main frame, Frame A, Frame B have their shared
  // kMaxPerRequestOriginScheduledDeferredBytes quota.
  EXPECT_EQ(GetAvailableQuota(root, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  EXPECT_EQ(GetAvailableQuota(frame_a, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  EXPECT_EQ(GetAvailableQuota(frame_b, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  // Frame C, Frame D is cross-origin and only comes with a small 8KB quota from
  // the "deferred-fetch-minimal" permissions policy.
  EXPECT_EQ(GetAvailableQuota(frame_c, new_request_url),
            kMinimalReservedDeferredFetchQuota);
  EXPECT_EQ(GetAvailableQuota(frame_d, new_request_url),
            kMinimalReservedDeferredFetchQuota);
}

TEST_F(GetAvailableDeferredFetchQuotaTest, MultipleLevelFrames) {
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

  auto new_request_url = KURL(kMainUrl + "test.html");
  // The main frame, Frame A, Frame B have their shared
  // kMaxPerRequestOriginScheduledDeferredBytes quota.
  // See https://github.com/whatwg/fetch/pull/1647/files#r1906538637
  EXPECT_EQ(GetAvailableQuota(root, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  EXPECT_EQ(GetAvailableQuota(frame_a, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  EXPECT_EQ(GetAvailableQuota(frame_b, new_request_url), 0u);
  // Frame C, Frame D is cross-origin and only comes with a small 8KB quota from
  // the "deferred-fetch-minimal" permissions policy.
  EXPECT_EQ(GetAvailableQuota(frame_c, new_request_url),
            kMinimalReservedDeferredFetchQuota);
  EXPECT_EQ(GetAvailableQuota(frame_d, new_request_url),
            kMinimalReservedDeferredFetchQuota);
}

// Tests that multiple sandboxed iframes by default (without allow-same-origin)
// only gets a small 8KB quota from "deferred-fetch-minimal" permissions policy.
// Note that this test doesn't cover the behavior whether child sandboxed frames
// consuming parent frame quota or not, which is covered in WPT.
TEST_F(GetAvailableDeferredFetchQuotaTest, SandboxedIframes) {
  // The structure of the document:
  // root -> 8 sandboxed iframes
  String root_url = kMainUrl;
  Vector<String> frame_urls = {
      kMainUrl + "frame-0.html", kMainUrl + "frame-1.html",
      kMainUrl + "frame-2.html", kMainUrl + "frame-3.html",
      kMainUrl + "frame-4.html", kMainUrl + "frame-5.html",
      kMainUrl + "frame-6.html", kMainUrl + "frame-7.html",
  };
  RequestUrlAndDataType url_and_data = {
      {kMainUrl + "frame-0.html", ""}, {kMainUrl + "frame-1.html", ""},
      {kMainUrl + "frame-2.html", ""}, {kMainUrl + "frame-3.html", ""},
      {kMainUrl + "frame-4.html", ""}, {kMainUrl + "frame-5.html", ""},
      {kMainUrl + "frame-6.html", ""}, {kMainUrl + "frame-7.html", ""},
  };

  NavigateTo(root_url, RenderWithIframes(frame_urls, "allow-scripts"),
             url_and_data);

  auto* root = GetMainFrame();
  auto* frame0 = root->Tree().FirstChild();
  auto* frame1 = frame0->Tree().NextSibling();
  auto* frame2 = frame1->Tree().NextSibling();
  auto* frame3 = frame2->Tree().NextSibling();
  auto* frame4 = frame3->Tree().NextSibling();
  auto* frame5 = frame4->Tree().NextSibling();
  auto* frame6 = frame5->Tree().NextSibling();
  auto* frame7 = frame6->Tree().NextSibling();
  auto new_request_url = KURL(kMainUrl + "test.html");

  // The main frame should have the default quota.
  EXPECT_EQ(GetAvailableQuota(root, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);

  // All child frames are sandboxed and only comes with a small 8KB quota from
  // the "deferred-fetch-minimal" permissions policy.
  EXPECT_EQ(GetAvailableQuota(frame1, new_request_url),
            kMinimalReservedDeferredFetchQuota);
  EXPECT_EQ(GetAvailableQuota(frame2, new_request_url),
            kMinimalReservedDeferredFetchQuota);
  EXPECT_EQ(GetAvailableQuota(frame3, new_request_url),
            kMinimalReservedDeferredFetchQuota);
  EXPECT_EQ(GetAvailableQuota(frame4, new_request_url),
            kMinimalReservedDeferredFetchQuota);
  EXPECT_EQ(GetAvailableQuota(frame5, new_request_url),
            kMinimalReservedDeferredFetchQuota);
  EXPECT_EQ(GetAvailableQuota(frame6, new_request_url),
            kMinimalReservedDeferredFetchQuota);
  EXPECT_EQ(GetAvailableQuota(frame7, new_request_url),
            kMinimalReservedDeferredFetchQuota);
}

// Tests that multiple sandboxed iframes with `allow-same-origin`
// gets shared quota with the root frame.
// Note that this test doesn't cover the behavior whether child sandboxed frames
// consuming parent frame quota or not, which is covered in WPT.
TEST_F(GetAvailableDeferredFetchQuotaTest, SandboxedIframeWithAllowSameOrigin) {
  // The structure of the document:
  // root -> 8 sandboxed iframes (allow-same-origin)
  String root_url = kMainUrl;
  Vector<String> frame_urls = {
      kMainUrl + "frame-0.html", kMainUrl + "frame-1.html",
      kMainUrl + "frame-2.html", kMainUrl + "frame-3.html",
      kMainUrl + "frame-4.html", kMainUrl + "frame-5.html",
      kMainUrl + "frame-6.html", kMainUrl + "frame-7.html",
  };
  RequestUrlAndDataType url_and_data = {
      {kMainUrl + "frame-0.html", ""}, {kMainUrl + "frame-1.html", ""},
      {kMainUrl + "frame-2.html", ""}, {kMainUrl + "frame-3.html", ""},
      {kMainUrl + "frame-4.html", ""}, {kMainUrl + "frame-5.html", ""},
      {kMainUrl + "frame-6.html", ""}, {kMainUrl + "frame-7.html", ""},
  };

  NavigateTo(root_url,
             RenderWithIframes(frame_urls, "allow-scripts allow-same-origin"),
             url_and_data);

  auto* root = GetMainFrame();
  auto* frame0 = root->Tree().FirstChild();
  auto* frame1 = frame0->Tree().NextSibling();
  auto* frame2 = frame1->Tree().NextSibling();
  auto* frame3 = frame2->Tree().NextSibling();
  auto* frame4 = frame3->Tree().NextSibling();
  auto* frame5 = frame4->Tree().NextSibling();
  auto* frame6 = frame5->Tree().NextSibling();
  auto* frame7 = frame6->Tree().NextSibling();
  auto new_request_url = KURL(kMainUrl + "test.html");

  // The main frame and all child sandbox frames have their shared
  // kMaxPerRequestOriginScheduledDeferredBytes quota.
  EXPECT_EQ(GetAvailableQuota(root, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  EXPECT_EQ(GetAvailableQuota(frame0, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  EXPECT_EQ(GetAvailableQuota(frame1, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  EXPECT_EQ(GetAvailableQuota(frame2, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  EXPECT_EQ(GetAvailableQuota(frame3, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  EXPECT_EQ(GetAvailableQuota(frame4, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  EXPECT_EQ(GetAvailableQuota(frame5, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  EXPECT_EQ(GetAvailableQuota(frame6, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
  EXPECT_EQ(GetAvailableQuota(frame7, new_request_url),
            kMaxPerRequestOriginScheduledDeferredBytes);
}

class CalculateRequestSizeTestBase : public testing::Test {
 protected:
  static const KURL RequestURL() {
    return KURL(AtomicString("http://www.example.com"));
  }
  static const KURL RequestURLWithFragment() {
    return KURL(AtomicString("http://www.example.com/#tag"));
  }
  static const KURL ReferrerURL() {
    return KURL(AtomicString("http://example.com"));
  }

  using HeadersType = const Vector<std::pair<const String, const String>>;
  static uint64_t GetHeadersSize(const HeadersType& headers) {
    uint64_t total = 0;
    for (const auto& [key, value] : headers) {
      total += key.length() + value.length();
    }
    return total;
  }
  static uint64_t GetUrlSize(const KURL& url) {
    return FetchLaterUtil::GetUrlLengthWithoutFragment(url);
  }
};

class CalculateGetRequestSizeTest : public CalculateRequestSizeTestBase {
 protected:
  static FetchRequestData* CreateFetchRequestDataForGet(
      const KURL& url,
      const HeadersType& headers,
      const KURL& referrer = KURL("")) {
    auto request = mojom::blink::FetchAPIRequest::New();
    request->method = "GET";
    request->url = url;
    for (const auto& [key, value] : headers) {
      request->headers.insert(key, value);
    }
    request->referrer = mojom::blink::Referrer::New(
        referrer, network::mojom::ReferrerPolicy::kDefault);
    return FetchRequestData::Create(
        /*script_state=*/nullptr, std::move(request),
        FetchRequestData::ForServiceWorkerFetchEvent::kFalse);
  }
};

TEST_F(CalculateGetRequestSizeTest, WithoutHeaders) {
  auto url = RequestURL();
  auto* request = CreateFetchRequestDataForGet(url, {});

  EXPECT_EQ(FetchLaterUtil::CalculateRequestSize(*request), GetUrlSize(url));
}

TEST_F(CalculateGetRequestSizeTest, WithHeaders) {
  auto url = RequestURL();
  HeadersType headers = {
      {"key-1", "value 1"},
      {"key-2", "value 2"},
  };
  auto* request = CreateFetchRequestDataForGet(RequestURL(), headers);

  EXPECT_EQ(FetchLaterUtil::CalculateRequestSize(*request),
            GetUrlSize(url) + GetHeadersSize(headers));
}

TEST_F(CalculateGetRequestSizeTest, WithFragmentURLAndHeaders) {
  auto url = RequestURLWithFragment();
  HeadersType headers = {
      {"key-1", "value 1"},
      {"key-2", "value 2"},
  };
  auto* request = CreateFetchRequestDataForGet(url, headers);

  EXPECT_EQ(FetchLaterUtil::CalculateRequestSize(*request),
            GetUrlSize(url) + GetHeadersSize(headers));
}

TEST_F(CalculateGetRequestSizeTest, WithFragmentURLAndHeadersAndReferrer) {
  auto url = RequestURLWithFragment();
  auto referrer = ReferrerURL();
  HeadersType headers = {
      {"key-1", "value 1"},
      {"key-2", "value 2"},
  };
  auto* request = CreateFetchRequestDataForGet(url, headers, referrer);

  EXPECT_EQ(FetchLaterUtil::CalculateRequestSize(*request),
            GetUrlSize(url) + GetHeadersSize(headers) + GetUrlSize(referrer));
}

class CalculatePostRequestSizeTest : public CalculateRequestSizeTestBase {
 protected:
  static FetchRequestData* CreateFetchRequestDataForPost(
      V8TestingScope& scope,
      const KURL& url,
      const HeadersType& headers,
      std::optional<const String> body,
      const KURL& referrer = KURL("")) {
    auto request = mojom::blink::FetchAPIRequest::New();
    request->method = "POST";
    request->url = url;
    for (const auto& [key, value] : headers) {
      request->headers.insert(key, value);
    }
    if (body.has_value()) {
      ResourceRequestBody request_body(EncodedFormData::Create());
      request_body.FormBody()->AppendData(body->Ascii());
      request->body = std::move(request_body);
    }
    request->referrer = mojom::blink::Referrer::New(
        referrer, network::mojom::ReferrerPolicy::kDefault);
    return FetchRequestData::Create(
        scope.GetScriptState(), std::move(request),
        FetchRequestData::ForServiceWorkerFetchEvent::kFalse);
  }

 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(CalculatePostRequestSizeTest, WithoutBodyWithoutHeaders) {
  V8TestingScope scope;
  const auto url = RequestURL();
  const HeadersType headers = {};
  auto* request =
      CreateFetchRequestDataForPost(scope, url, headers, std::nullopt);

  EXPECT_EQ(FetchLaterUtil::CalculateRequestSize(*request), GetUrlSize(url));
}

TEST_F(CalculatePostRequestSizeTest, WithoutBodyWithHeaders) {
  V8TestingScope scope;
  const auto url = RequestURL();
  const HeadersType headers = {
      {"key-1", "value 1"},
      {"key-2", "value 2"},
  };
  auto* request =
      CreateFetchRequestDataForPost(scope, url, headers, std::nullopt);

  EXPECT_EQ(FetchLaterUtil::CalculateRequestSize(*request),
            GetUrlSize(url) + GetHeadersSize(headers));
}

TEST_F(CalculatePostRequestSizeTest, WithoutBodyWithFragmentURLAndHeaders) {
  V8TestingScope scope;
  const auto url = RequestURLWithFragment();
  const HeadersType headers = {
      {"key-1", "value 1"},
      {"key-2", "value 2"},
  };
  auto* request =
      CreateFetchRequestDataForPost(scope, url, headers, std::nullopt);

  EXPECT_EQ(FetchLaterUtil::CalculateRequestSize(*request),
            GetUrlSize(url) + GetHeadersSize(headers));
}

TEST_F(CalculatePostRequestSizeTest,
       WithoutBodyWithFragmentURLAndHeadersAndReferrer) {
  V8TestingScope scope;
  const auto url = RequestURLWithFragment();
  const HeadersType headers = {
      {"key-1", "value 1"},
      {"key-2", "value 2"},
  };
  auto referrer = ReferrerURL();
  auto* request = CreateFetchRequestDataForPost(scope, url, headers,
                                                std::nullopt, referrer);

  EXPECT_EQ(FetchLaterUtil::CalculateRequestSize(*request),
            GetUrlSize(url) + GetHeadersSize(headers) + GetUrlSize(referrer));
}

TEST_F(CalculatePostRequestSizeTest, WithdHeaders) {
  V8TestingScope scope;
  const auto url = RequestURL();
  const HeadersType headers = {
      {"key-1", "value 1"},
      {"key-2", "value 2"},
  };
  const String body = "test body content";
  auto* request = CreateFetchRequestDataForPost(scope, url, headers, body);

  EXPECT_EQ(FetchLaterUtil::CalculateRequestSize(*request),
            GetUrlSize(url) + GetHeadersSize(headers) + body.length());
}

TEST_F(CalculatePostRequestSizeTest, WithFragmentURLAndHeaders) {
  V8TestingScope scope;
  const auto url = RequestURLWithFragment();
  const HeadersType headers = {
      {"key-1", "value 1"},
      {"key-2", "value 2"},
  };
  const String body = "test body content";
  auto* request = CreateFetchRequestDataForPost(scope, url, headers, body);

  EXPECT_EQ(FetchLaterUtil::CalculateRequestSize(*request),
            GetUrlSize(url) + GetHeadersSize(headers) + body.length());
}

TEST_F(CalculatePostRequestSizeTest, WithFragmentURLAndHeadersAndReferrer) {
  V8TestingScope scope;
  const auto url = RequestURLWithFragment();
  const HeadersType headers = {
      {"key-1", "value 1"},
      {"key-2", "value 2"},
  };
  const String body = "test body content";
  auto referrer = ReferrerURL();
  auto* request =
      CreateFetchRequestDataForPost(scope, url, headers, body, referrer);

  EXPECT_EQ(FetchLaterUtil::CalculateRequestSize(*request),
            GetUrlSize(url) + GetHeadersSize(headers) + body.length() +
                GetUrlSize(referrer));
}

}  // namespace blink
