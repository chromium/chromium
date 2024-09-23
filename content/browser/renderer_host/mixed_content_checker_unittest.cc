// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/mixed_content_checker.h"

#include <memory>
#include <optional>
#include <ostream>
#include <tuple>
#include <vector>

#include "content/public/browser/web_contents.h"
#include "content/public/test/fake_local_frame.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "services/network/public/mojom/source_location.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "url/gurl.h"

namespace content {
namespace {

using ::testing::Eq;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;

// Intercepts the mojo calls of `MixedContentFound()` and
// `ReportBlinkFeatureUsage()` from `rfh` to a LocalFrame in renderer.
class LocalFrameInterceptor : public FakeLocalFrame {
 public:
  explicit LocalFrameInterceptor(RenderFrameHost* rfh)
      : rfh_(static_cast<TestRenderFrameHost*>(rfh)) {
    rfh_->ResetLocalFrame();
    Init(rfh_->GetRemoteAssociatedInterfaces());
  }

  // FakeLocalFrame overrides:
  struct MixedContentResult {
    GURL main_resource_url;
    GURL mixed_content_url;
    bool was_allowed;
    bool had_redirect;
  };
  void MixedContentFound(
      const GURL& main_resource_url,
      const GURL& mixed_content_url,
      blink::mojom::RequestContextType request_context,
      bool was_allowed,
      const GURL& url_before_redirects,
      bool had_redirect,
      network::mojom::SourceLocationPtr source_location) final {
    mixed_content_result_ = MixedContentResult{
        main_resource_url, mixed_content_url, was_allowed, had_redirect};
  }
  void ReportBlinkFeatureUsage(
      const std::vector<blink::mojom::WebFeature>& web_features) final {
    reported_web_features_ = web_features;
  }

  const std::optional<MixedContentResult>& mixed_content_result() const {
    return mixed_content_result_;
  }
  const std::vector<blink::mojom::WebFeature>& reported_web_features() const {
    return reported_web_features_;
  }

  void FlushLocalFrameMessages() { rfh_->FlushLocalFrameMessages(); }

 private:
  raw_ptr<TestRenderFrameHost> rfh_;
  std::vector<blink::mojom::WebFeature> reported_web_features_;
  std::optional<MixedContentResult> mixed_content_result_;
};

// Needed by GTest to display errors.
std::ostream& operator<<(std::ostream& out,
                         const LocalFrameInterceptor::MixedContentResult& m) {
  return out << "\nMixedContentResult {\n"
             << "  main_resource_url: " << m.main_resource_url << "\n"
             << "  mixed_content_url: " << m.mixed_content_url << "\n"
             << "  was_allowed: " << m.was_allowed << "\n"
             << "  had_redirect: " << m.had_redirect << "\n"
             << "}";
}

}  // namespace

// Tests that `content::MixedContentChecker` correctly detects or ignores many
// cases where there is or there is not mixed content, respectively.
// Note: Browser side version of
// `blink::MixedContentCheckerTest::IsMixedContent`.
// Must be kept in sync manually!
// LINT.IfChange
TEST(MixedContentCheckerTest, IsMixedContent) {
  struct TestCase {
    const char* origin;
    const char* target;
    const bool expected_mixed_content;
  } cases[] = {
      {"http://example.com/foo", "http://example.com/foo", false},
      {"http://example.com/foo", "https://example.com/foo", false},
      {"http://example.com/foo", "data:text/html,<p>Hi!</p>", false},
      {"http://example.com/foo", "about:blank", false},
      {"https://example.com/foo", "https://example.com/foo", false},
      {"https://example.com/foo", "wss://example.com/foo", false},
      {"https://example.com/foo", "data:text/html,<p>Hi!</p>", false},
      {"https://example.com/foo", "blob:https://example.com/foo", false},
      {"https://example.com/foo", "filesystem:https://example.com/foo", false},
      {"https://example.com/foo", "http://127.0.0.1/", false},
      {"https://example.com/foo", "http://[::1]/", false},
      {"https://example.com/foo", "http://a.localhost/", false},
      {"https://example.com/foo", "http://localhost/", false},

      {"https://example.com/foo", "http://example.com/foo", true},
      {"https://example.com/foo", "http://google.com/foo", true},
      {"https://example.com/foo", "ws://example.com/foo", true},
      {"https://example.com/foo", "ws://google.com/foo", true},
      {"https://example.com/foo", "http://192.168.1.1/", true},
      {"https://example.com/foo", "blob:http://example.com/foo", true},
      {"https://example.com/foo", "blob:null/foo", true},
      {"https://example.com/foo", "filesystem:http://example.com/foo", true},
      {"https://example.com/foo", "filesystem:null/foo", true},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "Origin: " << test.origin << ", Target: " << test.target
                 << ", Expectation: " << test.expected_mixed_content);
    GURL origin_url(test.origin);
    GURL target_url(test.target);
    EXPECT_EQ(
        test.expected_mixed_content,
        MixedContentChecker::IsMixedContentForTesting(origin_url, target_url));
  }
}
// LINT.ThenChange(third_party/blink/renderer/core/loader/mixed_content_checker_test.cc)

class MixedContentCheckerShouldBlockTestBase
    : public RenderViewHostImplTestHarness,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    NavigateAndCommit(GURL("about:blank"));
  }

  bool for_redirect() const { return GetParam(); }
};

class MixedContentCheckerShouldBlockNavigationTestBase
    : public MixedContentCheckerShouldBlockTestBase {
 protected:
  // Starts a navigation from `source_url` to `target_url`. `from_subframe`
  // tells if the navigation is initiated from the main frame or sub frame of
  // the page of `source_url`.
  // Returns a tuple of:
  //   - a navigation simulator indicating a started navigation request.
  //   - a LocalFrame inspector that collects messages from the navigation
  //     initiator frame.
  std::tuple<std::unique_ptr<NavigationSimulatorImpl>,
             std::unique_ptr<LocalFrameInterceptor>>
  StartNavigation(
      const std::string& source_url,
      const std::string& target_url,
      bool from_subframe = false,
      blink::mojom::InsecureRequestPolicy main_frame_insecure_request_policy =
          blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
      blink::mojom::MixedContentContextType mixed_content_context_type =
          blink::mojom::MixedContentContextType::kBlockable) {
    // Loads the page of `source_url` first.
    NavigateAndCommit(GURL(source_url));

    TestRenderFrameHost* rfh = main_test_rfh();
    rfh->DidEnforceInsecureRequestPolicy(main_frame_insecure_request_policy);
    if (from_subframe) {
      // Navigation request is from a subframe of the page of `source_url`.
      TestRenderFrameHost* subframe = rfh->AppendChild("subframe");
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(source_url + "/subframe"), subframe);
      rfh = subframe;
    }
    // `interceptor` must be constructed before initiating the navigation, such
    // that the remote of `rfh` pointing to a LocalFrame can be replaced.
    auto interceptor = std::make_unique<LocalFrameInterceptor>(rfh);

    std::unique_ptr<NavigationSimulatorImpl> navigation =
        NavigationSimulatorImpl::CreateRendererInitiated(GURL(target_url), rfh);
    navigation->SetReferrer(blink::mojom::Referrer::New(
        rfh->GetLastCommittedURL(),
        network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
    navigation->set_request_context_type(
        blink::mojom::RequestContextType::INTERNAL);
    navigation->set_mixed_content_context_type(mixed_content_context_type);
    // This line will trigger `MixedContentNavigationThrottle`, which underlying
    // also runs the test target `MixedContentChecker::ShouldBlockNavigation()`.
    navigation->Start();
    return std::make_tuple(std::move(navigation), std::move(interceptor));
  }
};

using MixedContentCheckerShouldBlockNavigationTest =
    MixedContentCheckerShouldBlockNavigationTestBase;

INSTANTIATE_TEST_SUITE_P(
    All,
    MixedContentCheckerShouldBlockNavigationTest,
    ::testing::Values(false, true),
    [](const testing::TestParamInfo<
        MixedContentCheckerShouldBlockNavigationTest::ParamType>& info) {
      return info.param ? "ForRedirect" : "ForNonRedirect";
    });

// Main frame navigations cannot be mixed content, no matter the source page is
// secure or not.
TEST_P(MixedContentCheckerShouldBlockNavigationTest,
       ShouldNotBlockNavigationFromInsecureMainFrame) {
  const auto [nav, inspector] =
      StartNavigation("http://source.com", "http://target.com");
  auto checker = MixedContentChecker();

  EXPECT_FALSE(checker.ShouldBlockNavigation(*nav->GetNavigationHandle(),
                                             for_redirect()));
  inspector->FlushLocalFrameMessages();
  EXPECT_THAT(inspector->mixed_content_result(), Eq(std::nullopt));
  EXPECT_THAT(inspector->reported_web_features(), IsEmpty());
}

// Main frame navigations cannot be mixed content, no matter the source page is
// secure or not.
TEST_P(MixedContentCheckerShouldBlockNavigationTest,
       ShouldNotBlockNavigationFromSecureMainFrame) {
  const auto [nav, inspector] =
      StartNavigation("https://source.com", "http://target.com");
  auto checker = MixedContentChecker();

  EXPECT_FALSE(checker.ShouldBlockNavigation(*nav->GetNavigationHandle(),
                                             for_redirect()));
  inspector->FlushLocalFrameMessages();
  EXPECT_THAT(inspector->mixed_content_result(), Eq(std::nullopt));
  EXPECT_THAT(inspector->reported_web_features(), IsEmpty());
}

// Navigates from insecure content is not mixed content.
TEST_P(MixedContentCheckerShouldBlockNavigationTest,
       ShouldNotBlockNavigationFromInsecureSubFrame) {
  const bool from_subframe = true;
  const auto [nav, inspector] =
      StartNavigation("http://source.com", "http://target.com", from_subframe);
  auto checker = MixedContentChecker();

  EXPECT_FALSE(checker.ShouldBlockNavigation(*nav->GetNavigationHandle(),
                                             for_redirect()));
  inspector->FlushLocalFrameMessages();
  EXPECT_THAT(inspector->mixed_content_result(), Eq(std::nullopt));
  EXPECT_THAT(inspector->reported_web_features(), IsEmpty());
}

// Tests to cover MixedContentContextType = kBlockable.
class MixedContentCheckerShouldBlockNavigationWithBlockableContextTest
    : public MixedContentCheckerShouldBlockNavigationTestBase {
 protected:
  blink::mojom::MixedContentContextType mixed_content_context_type() const {
    return blink::mojom::MixedContentContextType::kBlockable;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MixedContentCheckerShouldBlockNavigationWithBlockableContextTest,
    ::testing::Values(false, true),
    [](const testing::TestParamInfo<
        MixedContentCheckerShouldBlockNavigationWithBlockableContextTest::
            ParamType>& info) {
      return info.param ? "ForRedirect" : "ForNonRedirect";
    });

// ShouldBlockNavigation(subframe) => true
// - MixedContentContextType = kBlockable
// - main frame's InsecureRequestPolicy = kLeaveInsecureRequestsAlone
TEST_P(MixedContentCheckerShouldBlockNavigationWithBlockableContextTest,
       ShouldBlockMixedContentNavigationWithPolicyLeaveInsecureRequestAlone) {
  const auto main_frame_insecure_request_policy =
      blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone;
  const bool from_subframe = true;
  const auto [nav, inspector] = StartNavigation(
      "https://source.com", "http://target.com", from_subframe,
      main_frame_insecure_request_policy, mixed_content_context_type());
  auto checker = MixedContentChecker();

  EXPECT_TRUE(checker.ShouldBlockNavigation(*nav->GetNavigationHandle(),
                                            for_redirect()));
  inspector->FlushLocalFrameMessages();
  EXPECT_THAT(
      inspector->mixed_content_result(),
      Optional(FieldsAre(GURL("https://source.com"), GURL("http://target.com"),
                         /*was_allowed=*/false, for_redirect())));
  EXPECT_THAT(
      inspector->reported_web_features(),
      UnorderedElementsAre(blink::mojom::WebFeature::kMixedContentPresent,
                           blink::mojom::WebFeature::kMixedContentBlockable));
}

// ShouldBlockNavigation(subframe) => true
// - MixedContentContextType = kBlockable
// - main frame's InsecureRequestPolicy = kBlockAllMixedContent
TEST_P(MixedContentCheckerShouldBlockNavigationWithBlockableContextTest,
       ShouldBlockMixedContentNavigationWithPolicyBlockAll) {
  const auto main_frame_insecure_request_policy =
      blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent;
  const bool from_subframe = true;
  const auto [nav, inspector] = StartNavigation(
      "https://source.com", "http://target.com", from_subframe,
      main_frame_insecure_request_policy, mixed_content_context_type());
  auto checker = MixedContentChecker();

  EXPECT_TRUE(checker.ShouldBlockNavigation(*nav->GetNavigationHandle(),
                                            for_redirect()));
  inspector->FlushLocalFrameMessages();
  EXPECT_THAT(
      inspector->mixed_content_result(),
      Optional(FieldsAre(GURL("https://source.com"), GURL("http://target.com"),
                         /*was_allowed=*/false, for_redirect())));
  EXPECT_THAT(
      inspector->reported_web_features(),
      UnorderedElementsAre(blink::mojom::WebFeature::kMixedContentPresent,
                           blink::mojom::WebFeature::kMixedContentBlockable));
}

// Tests to cover MixedContentContextType = kOptionallyBlockable.
class MixedContentCheckerShouldBlockNavigationWithOptionallyBlockableContextTest
    : public MixedContentCheckerShouldBlockNavigationTestBase {
 protected:
  blink::mojom::MixedContentContextType mixed_content_context_type() const {
    return blink::mojom::MixedContentContextType::kOptionallyBlockable;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MixedContentCheckerShouldBlockNavigationWithOptionallyBlockableContextTest,
    ::testing::Values(false, true),
    [](const testing::TestParamInfo<
        MixedContentCheckerShouldBlockNavigationWithOptionallyBlockableContextTest::
            ParamType>& info) {
      return info.param ? "ForRedirect" : "ForNonRedirect";
    });

// ShouldBlockNavigation(subframe) => false
// - MixedContentContextType = kOptionallyBlockable
// - main frame's InsecureRequestPolicy = kLeaveInsecureRequestsAlone
TEST_P(
    MixedContentCheckerShouldBlockNavigationWithOptionallyBlockableContextTest,
    ShouldNotBlockMixedContentNavigationWithPolicyLeaveInsecureRequestAlone) {
  const auto main_frame_insecure_request_policy =
      blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone;
  const bool from_subframe = true;
  const auto [nav, inspector] = StartNavigation(
      "https://source.com", "http://target.com", from_subframe,
      main_frame_insecure_request_policy, mixed_content_context_type());
  auto checker = MixedContentChecker();

  EXPECT_FALSE(checker.ShouldBlockNavigation(*nav->GetNavigationHandle(),
                                             for_redirect()));
  inspector->FlushLocalFrameMessages();
  EXPECT_THAT(
      inspector->mixed_content_result(),
      Optional(FieldsAre(GURL("https://source.com"), GURL("http://target.com"),
                         /*was_allowed=*/true, for_redirect())));
  EXPECT_THAT(
      inspector->reported_web_features(),
      UnorderedElementsAre(blink::mojom::WebFeature::kMixedContentPresent,
                           blink::mojom::WebFeature::kMixedContentInternal));
}

// ShouldBlockNavigation(subframe) => true
// - MixedContentContextType = kOptionallyBlockable
// - main frame's InsecureRequestPolicy = kBlockAllMixedContent
TEST_P(
    MixedContentCheckerShouldBlockNavigationWithOptionallyBlockableContextTest,
    ShouldBlockMixedContentNavigationWithPolicyBlockAll) {
  const auto main_frame_insecure_request_policy =
      blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent;
  const bool from_subframe = true;
  const auto [nav, inspector] = StartNavigation(
      "https://source.com", "http://target.com", from_subframe,
      main_frame_insecure_request_policy, mixed_content_context_type());
  auto checker = MixedContentChecker();

  EXPECT_TRUE(checker.ShouldBlockNavigation(*nav->GetNavigationHandle(),
                                            for_redirect()));
  inspector->FlushLocalFrameMessages();
  EXPECT_THAT(
      inspector->mixed_content_result(),
      Optional(FieldsAre(GURL("https://source.com"), GURL("http://target.com"),
                         /*was_allowed=*/false, for_redirect())));
  EXPECT_THAT(
      inspector->reported_web_features(),
      UnorderedElementsAre(blink::mojom::WebFeature::kMixedContentPresent,
                           blink::mojom::WebFeature::kMixedContentInternal));
}

// Tests to cover MixedContentContextType = kShouldBeBlockable.
class MixedContentCheckerShouldBlockNavigationWithShouldBeBlockableContextTest
    : public MixedContentCheckerShouldBlockNavigationTestBase {
 protected:
  blink::mojom::MixedContentContextType mixed_content_context_type() const {
    return blink::mojom::MixedContentContextType::kShouldBeBlockable;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MixedContentCheckerShouldBlockNavigationWithShouldBeBlockableContextTest,
    ::testing::Values(false, true),
    [](const testing::TestParamInfo<
        MixedContentCheckerShouldBlockNavigationWithShouldBeBlockableContextTest::
            ParamType>& info) {
      return info.param ? "ForRedirect" : "ForNonRedirect";
    });

// ShouldBlockNavigation(subframe) => false
// - MixedContentContextType = kShouldBeBlockable
// - main frame's InsecureRequestPolicy = kLeaveInsecureRequestsAlone
TEST_P(
    MixedContentCheckerShouldBlockNavigationWithShouldBeBlockableContextTest,
    ShouldNotBlockMixedContentNavigationWithPolicyLeaveInsecureRequestAlone) {
  const auto main_frame_insecure_request_policy =
      blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone;
  const bool from_subframe = true;
  const auto [nav, inspector] = StartNavigation(
      "https://source.com", "http://target.com", from_subframe,
      main_frame_insecure_request_policy, mixed_content_context_type());
  auto checker = MixedContentChecker();

  EXPECT_FALSE(checker.ShouldBlockNavigation(*nav->GetNavigationHandle(),
                                             for_redirect()));
  inspector->FlushLocalFrameMessages();
  EXPECT_THAT(
      inspector->mixed_content_result(),
      Optional(FieldsAre(GURL("https://source.com"), GURL("http://target.com"),
                         /*was_allowed=*/true, for_redirect())));
  EXPECT_THAT(
      inspector->reported_web_features(),
      UnorderedElementsAre(blink::mojom::WebFeature::kMixedContentPresent,
                           blink::mojom::WebFeature::kMixedContentInternal));
}

// ShouldBlockNavigation(subframe) => true
// - MixedContentContextType = kShouldBeBlockable
// - main frame's InsecureRequestPolicy = kBlockAllMixedContent
TEST_P(MixedContentCheckerShouldBlockNavigationWithShouldBeBlockableContextTest,
       ShouldBlockMixedContentNavigationWithPolicyBlockAll) {
  const auto main_frame_insecure_request_policy =
      blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent;
  const bool from_subframe = true;
  const auto [nav, inspector] = StartNavigation(
      "https://source.com", "http://target.com", from_subframe,
      main_frame_insecure_request_policy, mixed_content_context_type());
  auto checker = MixedContentChecker();

  EXPECT_TRUE(checker.ShouldBlockNavigation(*nav->GetNavigationHandle(),
                                            for_redirect()));
  inspector->FlushLocalFrameMessages();
  EXPECT_THAT(
      inspector->mixed_content_result(),
      Optional(FieldsAre(GURL("https://source.com"), GURL("http://target.com"),
                         /*was_allowed=*/false, for_redirect())));
  EXPECT_THAT(
      inspector->reported_web_features(),
      UnorderedElementsAre(blink::mojom::WebFeature::kMixedContentPresent,
                           blink::mojom::WebFeature::kMixedContentInternal));
}

class MixedContentCheckerShouldBlockFetchKeepAliveTestBase
    : public MixedContentCheckerShouldBlockTestBase {
 protected:
  // Prepares a frame that loads `source_url`.
  // `from_subframe` tells if the frame is a main frame or sub frame of the page
  // of `source_url`.
  // Returns a tuple of:
  //   - a RenderFrameHostImpl representing the prepared frame.
  //   - a LocalFrame inspector that collects messages from the prepared frame.
  std::tuple<RenderFrameHostImpl*, std::unique_ptr<LocalFrameInterceptor>>
  PrepareFrame(
      const std::string& source_url,
      bool from_subframe = false,
      blink::mojom::InsecureRequestPolicy main_frame_insecure_request_policy =
          blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone) {
    // Loads the page of `source_url` first.
    NavigateAndCommit(GURL(source_url));

    TestRenderFrameHost* rfh = main_test_rfh();
    rfh->DidEnforceInsecureRequestPolicy(main_frame_insecure_request_policy);
    if (from_subframe) {
      // Request is from a subframe of the page of `source_url`.
      TestRenderFrameHost* subframe = rfh->AppendChild("subframe");
      rfh = static_cast<TestRenderFrameHost*>(
          NavigationSimulator::NavigateAndCommitFromDocument(
              GURL(source_url + "/subframe"), subframe));
    }
    auto interceptor = std::make_unique<LocalFrameInterceptor>(rfh);
    return std::make_tuple(rfh, std::move(interceptor));
  }

  // Expects no report to renderer no matter blocking happens or not.
  void ExpectNoReportToRenderer(LocalFrameInterceptor* inspector) {
    inspector->FlushLocalFrameMessages();
    EXPECT_THAT(inspector->mixed_content_result(), Eq(std::nullopt));
    EXPECT_THAT(inspector->reported_web_features(), IsEmpty());
  }
};

using MixedContentCheckerShouldBlockFetchKeepAliveTest =
    MixedContentCheckerShouldBlockFetchKeepAliveTestBase;

INSTANTIATE_TEST_SUITE_P(
    All,
    MixedContentCheckerShouldBlockFetchKeepAliveTest,
    ::testing::Values(false, true),
    [](const testing::TestParamInfo<
        MixedContentCheckerShouldBlockFetchKeepAliveTest::ParamType>& info) {
      return info.param ? "ForRedirect" : "ForNonRedirect";
    });

// Loading insecure url from insecure main frame should not be blocked.
TEST_P(MixedContentCheckerShouldBlockFetchKeepAliveTest,
       ShouldNotBlockInsecureFetchFromInsecureMainFrame) {
  const GURL url("http://target.com");
  const auto [rfh, inspector] = PrepareFrame("http://source.com");

  EXPECT_FALSE(
      MixedContentChecker::ShouldBlockFetchKeepAlive(rfh, url, for_redirect()));

  ExpectNoReportToRenderer(inspector.get());
}

// Loading insecure url from insecure subframe should not be blocked.
TEST_P(MixedContentCheckerShouldBlockFetchKeepAliveTest,
       ShouldNotBlockInsecureFetchFromInsecureSubFrame) {
  const bool from_subframe = true;
  const GURL url("http://target.com");
  const auto [rfh, inspector] =
      PrepareFrame("http://source.com", from_subframe);

  EXPECT_FALSE(
      MixedContentChecker::ShouldBlockFetchKeepAlive(rfh, url, for_redirect()));

  ExpectNoReportToRenderer(inspector.get());
}

// Loading insecure url from secure main/sub frame should be blocked, where the
// frame's InsecureRequestPolicy = kLeaveInsecureRequestsAlone.
TEST_P(
    MixedContentCheckerShouldBlockFetchKeepAliveTest,
    ShouldBlockInsecureFetchFromSecureFrameWithPolicyLeaveInsecureRequestAlone) {
  const auto main_frame_insecure_request_policy =
      blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone;
  const GURL url("http://target.com");
  {
    const bool from_subframe = false;
    const auto [rfh, inspector] =
        PrepareFrame("https://source.com", from_subframe,
                     main_frame_insecure_request_policy);

    EXPECT_TRUE(MixedContentChecker::ShouldBlockFetchKeepAlive(rfh, url,
                                                               for_redirect()));

    ExpectNoReportToRenderer(inspector.get());
  }
  {
    const bool from_subframe = true;
    const auto [rfh, inspector] =
        PrepareFrame("https://source.com", from_subframe,
                     main_frame_insecure_request_policy);

    EXPECT_TRUE(MixedContentChecker::ShouldBlockFetchKeepAlive(rfh, url,
                                                               for_redirect()));

    ExpectNoReportToRenderer(inspector.get());
  }
}

// Loading insecure url from secure main/sub frame should be blocked, where the
// frame's InsecureRequestPolicy = kBlockAllMixedContent.
TEST_P(MixedContentCheckerShouldBlockFetchKeepAliveTest,
       ShouldBlockInsecureFetchFromSecureFrameWithPolicyBlockAllMixedContent) {
  const auto main_frame_insecure_request_policy =
      blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent;
  const GURL url("http://target.com");
  {
    const bool from_subframe = false;
    const auto [rfh, inspector] =
        PrepareFrame("https://source.com", from_subframe,
                     main_frame_insecure_request_policy);

    EXPECT_TRUE(MixedContentChecker::ShouldBlockFetchKeepAlive(rfh, url,
                                                               for_redirect()));

    ExpectNoReportToRenderer(inspector.get());
  }
  {
    const bool from_subframe = true;
    const auto [rfh, inspector] =
        PrepareFrame("https://source.com", from_subframe,
                     main_frame_insecure_request_policy);

    EXPECT_TRUE(MixedContentChecker::ShouldBlockFetchKeepAlive(rfh, url,
                                                               for_redirect()));

    ExpectNoReportToRenderer(inspector.get());
  }
}

}  // namespace content
