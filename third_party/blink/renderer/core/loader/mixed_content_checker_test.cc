// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/mock_content_security_notifier.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/mixed_content.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// Tests that `blink::MixedContentChecker::IsMixedContent` correctly detects or
// ignores many cases where there is or there is not mixed content respectively.
// Note: Renderer side version of
// `content::MixedContentCheckerTest::IsMixedContent`.
// Must be kept in sync manually!
// LINT.IfChange
TEST(MixedContentCheckerTest, IsMixedContent) {
  test::TaskEnvironment task_environment;
  struct TestCase {
    const char* origin;
    const char* target;
    bool expectation;
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
      {"https://example.com/foo", "http://8.8.8.8/", true},
      {"https://example.com/foo", "blob:http://example.com/foo", true},
      {"https://example.com/foo", "blob:null/foo", true},
      {"https://example.com/foo", "filesystem:http://example.com/foo", true},
      {"https://example.com/foo", "filesystem:null/foo", true},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "Origin: " << test.origin << ", Target: " << test.target
                 << ", Expectation: " << test.expectation);
    KURL origin_url(NullURL(), test.origin);
    scoped_refptr<const SecurityOrigin> security_origin(
        SecurityOrigin::Create(origin_url));
    KURL target_url(NullURL(), test.target);
    EXPECT_EQ(test.expectation, MixedContentChecker::IsMixedContent(
                                    security_origin.get(), target_url));
  }
}
// LINT.ThenChange(content/browser/renderer_host/mixed_content_checker_unittest.cc)

TEST(MixedContentCheckerTest, ContextTypeForInspector) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(gfx::Size(1, 1));
  dummy_page_holder->GetFrame().Loader().CommitNavigation(
      WebNavigationParams::CreateWithEmptyHTMLForTesting(
          KURL("http://example.test")),
      nullptr /* extra_data */);
  blink::test::RunPendingTasks();

  ResourceRequest not_mixed_content("https://example.test/foo.jpg");
  not_mixed_content.SetRequestContext(mojom::blink::RequestContextType::SCRIPT);
  EXPECT_EQ(mojom::blink::MixedContentContextType::kNotMixedContent,
            MixedContentChecker::ContextTypeForInspector(
                &dummy_page_holder->GetFrame(), not_mixed_content));

  dummy_page_holder->GetFrame().Loader().CommitNavigation(
      WebNavigationParams::CreateWithEmptyHTMLForTesting(
          KURL("https://example.test")),
      nullptr /* extra_data */);
  blink::test::RunPendingTasks();

  EXPECT_EQ(mojom::blink::MixedContentContextType::kNotMixedContent,
            MixedContentChecker::ContextTypeForInspector(
                &dummy_page_holder->GetFrame(), not_mixed_content));

  ResourceRequest blockable_mixed_content("http://example.test/foo.jpg");
  blockable_mixed_content.SetRequestContext(
      mojom::blink::RequestContextType::SCRIPT);
  EXPECT_EQ(mojom::blink::MixedContentContextType::kBlockable,
            MixedContentChecker::ContextTypeForInspector(
                &dummy_page_holder->GetFrame(), blockable_mixed_content));

  ResourceRequest optionally_blockable_mixed_content(
      "http://example.test/foo.jpg");
  blockable_mixed_content.SetRequestContext(
      mojom::blink::RequestContextType::IMAGE);
  EXPECT_EQ(mojom::blink::MixedContentContextType::kOptionallyBlockable,
            MixedContentChecker::ContextTypeForInspector(
                &dummy_page_holder->GetFrame(), blockable_mixed_content));
}

TEST(MixedContentCheckerTest, HandleCertificateError) {
  test::TaskEnvironment task_environment;
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(
      gfx::Size(1, 1), nullptr, MakeGarbageCollected<EmptyLocalFrameClient>());

  KURL main_resource_url(NullURL(), "https://example.test");
  KURL displayed_url(NullURL(), "https://example-displayed.test");
  KURL ran_url(NullURL(), "https://example-ran.test");

  // Set up the mock content security notifier.
  testing::StrictMock<MockContentSecurityNotifier> mock_notifier;
  mojo::Remote<mojom::blink::ContentSecurityNotifier> notifier_remote;
  notifier_remote.Bind(mock_notifier.BindNewPipeAndPassRemote());

  dummy_page_holder->GetFrame().GetDocument()->SetURL(main_resource_url);
  ResourceResponse response1(ran_url);
  EXPECT_CALL(mock_notifier, NotifyContentWithCertificateErrorsRan()).Times(1);
  MixedContentChecker::HandleCertificateError(
      response1, mojom::blink::RequestContextType::SCRIPT,
      MixedContent::CheckModeForPlugin::kLax, *notifier_remote);

  ResourceResponse response2(displayed_url);
  mojom::blink::RequestContextType request_context =
      mojom::blink::RequestContextType::IMAGE;
  ASSERT_EQ(
      mojom::blink::MixedContentContextType::kOptionallyBlockable,
      MixedContent::ContextTypeFromRequestContext(
          request_context, MixedContentChecker::DecideCheckModeForPlugin(
                               dummy_page_holder->GetFrame().GetSettings())));
  EXPECT_CALL(mock_notifier, NotifyContentWithCertificateErrorsDisplayed())
      .Times(1);
  MixedContentChecker::HandleCertificateError(
      response2, request_context, MixedContent::CheckModeForPlugin::kLax,
      *notifier_remote);

  notifier_remote.FlushForTesting();
}

TEST(MixedContentCheckerTest, DetectMixedForm) {
  test::TaskEnvironment task_environment;
  KURL main_resource_url(NullURL(), "https://example.test/");
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(
      gfx::Size(1, 1), nullptr, MakeGarbageCollected<EmptyLocalFrameClient>());
  dummy_page_holder->GetFrame().Loader().CommitNavigation(
      WebNavigationParams::CreateWithEmptyHTMLForTesting(main_resource_url),
      nullptr /* extra_data */);
  blink::test::RunPendingTasks();

  KURL http_form_action_url(NullURL(), "http://example-action.test/");
  KURL https_form_action_url(NullURL(), "https://example-action.test/");
  KURL javascript_form_action_url(NullURL(), "javascript:void(0);");
  KURL mailto_form_action_url(NullURL(), "mailto:action@example-action.test");

  // mailto and http are non-secure form targets.
  EXPECT_TRUE(MixedContentChecker::IsMixedFormAction(
      &dummy_page_holder->GetFrame(), http_form_action_url,
      ReportingDisposition::kSuppressReporting));
  EXPECT_FALSE(MixedContentChecker::IsMixedFormAction(
      &dummy_page_holder->GetFrame(), https_form_action_url,
      ReportingDisposition::kSuppressReporting));
  EXPECT_FALSE(MixedContentChecker::IsMixedFormAction(
      &dummy_page_holder->GetFrame(), javascript_form_action_url,
      ReportingDisposition::kSuppressReporting));
  EXPECT_TRUE(MixedContentChecker::IsMixedFormAction(
      &dummy_page_holder->GetFrame(), mailto_form_action_url,
      ReportingDisposition::kSuppressReporting));
}

TEST(MixedContentCheckerTest, DetectMixedFavicon) {
  test::TaskEnvironment task_environment;
  KURL main_resource_url("https://example.test/");
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(
      gfx::Size(1, 1), nullptr, MakeGarbageCollected<EmptyLocalFrameClient>());
  dummy_page_holder->GetFrame().Loader().CommitNavigation(
      WebNavigationParams::CreateWithEmptyHTMLForTesting(main_resource_url),
      nullptr /* extra_data */);
  blink::test::RunPendingTasks();
  dummy_page_holder->GetFrame().GetSettings()->SetAllowRunningOfInsecureContent(
      false);

  KURL http_favicon_url("http://example.test/favicon.png");
  KURL https_favicon_url("https://example.test/favicon.png");
  KURL http_ip_address_favicon_url("http://8.8.8.8/favicon.png");
  KURL http_local_ip_address_favicon_url("http://127.0.0.1/favicon.png");
  KURL http_ip_address_audio_url("http://8.8.8.8/test.mp3");

  // Set up the mock content security notifier.
  testing::StrictMock<MockContentSecurityNotifier> mock_notifier;
  mojo::Remote<mojom::blink::ContentSecurityNotifier> notifier_remote;
  notifier_remote.Bind(mock_notifier.BindNewPipeAndPassRemote());

  // Test that a mixed content favicon is correctly blocked.
  EXPECT_TRUE(MixedContentChecker::ShouldBlockFetch(
      &dummy_page_holder->GetFrame(), mojom::blink::RequestContextType::FAVICON,
      network::mojom::blink::IPAddressSpace::kPublic, http_favicon_url,
      ResourceRequest::RedirectStatus::kNoRedirect, http_favicon_url, String(),
      ReportingDisposition::kSuppressReporting, *notifier_remote));

  // Test that a secure favicon is not blocked.
  EXPECT_FALSE(MixedContentChecker::ShouldBlockFetch(
      &dummy_page_holder->GetFrame(), mojom::blink::RequestContextType::FAVICON,
      network::mojom::blink::IPAddressSpace::kPublic, https_favicon_url,
      ResourceRequest::RedirectStatus::kNoRedirect, https_favicon_url, String(),
      ReportingDisposition::kSuppressReporting, *notifier_remote));

  EXPECT_TRUE(MixedContentChecker::ShouldBlockFetch(
      &dummy_page_holder->GetFrame(), mojom::blink::RequestContextType::FAVICON,
      network::mojom::blink::IPAddressSpace::kPublic,
      http_ip_address_favicon_url, ResourceRequest::RedirectStatus::kNoRedirect,
      http_ip_address_favicon_url, String(),
      ReportingDisposition::kSuppressReporting, *notifier_remote));

  EXPECT_FALSE(MixedContentChecker::ShouldBlockFetch(
      &dummy_page_holder->GetFrame(), mojom::blink::RequestContextType::FAVICON,
      network::mojom::blink::IPAddressSpace::kPublic,
      http_local_ip_address_favicon_url,
      ResourceRequest::RedirectStatus::kNoRedirect,
      http_local_ip_address_favicon_url, String(),
      ReportingDisposition::kSuppressReporting, *notifier_remote));
}

TEST(MixedContentCheckerTest, DetectUpgradeableMixedContent) {
  test::TaskEnvironment task_environment;
  KURL main_resource_url("https://example.test/");
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(
      gfx::Size(1, 1), nullptr, MakeGarbageCollected<EmptyLocalFrameClient>());
  dummy_page_holder->GetFrame().Loader().CommitNavigation(
      WebNavigationParams::CreateWithEmptyHTMLForTesting(main_resource_url),
      nullptr /* extra_data */);
  blink::test::RunPendingTasks();
  dummy_page_holder->GetFrame().GetSettings()->SetAllowRunningOfInsecureContent(
      false);

  KURL http_ip_address_audio_url("http://8.8.8.8/test.mp3");

  // Set up the mock content security notifier.
  testing::StrictMock<MockContentSecurityNotifier> mock_notifier;
  mojo::Remote<mojom::blink::ContentSecurityNotifier> notifier_remote;
  notifier_remote.Bind(mock_notifier.BindNewPipeAndPassRemote());

  const bool blocked = MixedContentChecker::ShouldBlockFetch(
      &dummy_page_holder->GetFrame(), mojom::blink::RequestContextType::AUDIO,
      network::mojom::blink::IPAddressSpace::kPublic, http_ip_address_audio_url,
      ResourceRequest::RedirectStatus::kNoRedirect, http_ip_address_audio_url,
      String(), ReportingDisposition::kSuppressReporting, *notifier_remote);

#if (BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)) && \
    BUILDFLAG(ENABLE_CAST_RECEIVER)
  // Mixed Content from an insecure IP address is not blocked for Fuchsia Cast
  // Receivers.
  EXPECT_FALSE(blocked);
#else
  EXPECT_TRUE(blocked);
#endif  // (BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)) &&
        // BUILDFLAG(ENABLE_CAST_RECEIVER)
}

class TestFetchClientSettingsObject : public FetchClientSettingsObject {
 public:
  const KURL& GlobalObjectUrl() const override { return url; }
  HttpsState GetHttpsState() const override { return HttpsState::kModern; }
  mojom::blink::InsecureRequestPolicy GetInsecureRequestsPolicy()
      const override {
    return mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone;
  }

  // These are not used in test, but need to be implemented since they are pure
  // virtual.
  const KURL& BaseUrl() const override { return url; }
  const SecurityOrigin* GetSecurityOrigin() const override { return nullptr; }
  network::mojom::ReferrerPolicy GetReferrerPolicy() const override {
    return network::mojom::ReferrerPolicy::kAlways;
  }
  const String GetOutgoingReferrer() const override { return ""; }
  AllowedByNosniff::MimeTypeCheck MimeTypeCheckForClassicWorkerScript()
      const override {
    return AllowedByNosniff::MimeTypeCheck::kStrict;
  }
  const InsecureNavigationsSet& GetUpgradeInsecureNavigationsSet()
      const override {
    return set;
  }

 private:
  const KURL url = KURL("https://example.test");
  const InsecureNavigationsSet set;
};

TEST(MixedContentCheckerTest,
     NotAutoupgradedMixedContentHasUpgradeIfInsecureSet) {
  test::TaskEnvironment task_environment;
  ResourceRequest request;
  request.SetUrl(KURL("https://example.test"));
  request.SetRequestContext(mojom::blink::RequestContextType::AUDIO);
  TestFetchClientSettingsObject* settings =
      MakeGarbageCollected<TestFetchClientSettingsObject>();
  // Used to get a non-null document.
  DummyPageHolder holder;

  MixedContentChecker::UpgradeInsecureRequest(
      request, settings, holder.GetDocument().GetExecutionContext(),
      mojom::RequestContextFrameType::kTopLevel, nullptr);

  EXPECT_FALSE(request.IsAutomaticUpgrade());
  EXPECT_TRUE(request.UpgradeIfInsecure());
}

TEST(MixedContentCheckerTest, AutoupgradedMixedContentHasUpgradeIfInsecureSet) {
  test::TaskEnvironment task_environment;
  ResourceRequest request;
  request.SetUrl(KURL("http://example.test"));
  request.SetRequestContext(mojom::blink::RequestContextType::AUDIO);
  TestFetchClientSettingsObject* settings =
      MakeGarbageCollected<TestFetchClientSettingsObject>();
  // Used to get a non-null document.
  DummyPageHolder holder;

  MixedContentChecker::UpgradeInsecureRequest(
      request, settings, holder.GetDocument().GetExecutionContext(),
      mojom::RequestContextFrameType::kTopLevel, nullptr);

  EXPECT_TRUE(request.IsAutomaticUpgrade());
  EXPECT_TRUE(request.UpgradeIfInsecure());
}

TEST(MixedContentCheckerTest,
     AutoupgradeMixedContentWithLiteralLocalIpAddress) {
  test::TaskEnvironment task_environment;
  ResourceRequest request;
  request.SetUrl(KURL("http://127.0.0.1/"));
  request.SetRequestContext(mojom::blink::RequestContextType::AUDIO);
  TestFetchClientSettingsObject* settings =
      MakeGarbageCollected<TestFetchClientSettingsObject>();
  // Used to get a non-null document.
  DummyPageHolder holder;

  MixedContentChecker::UpgradeInsecureRequest(
      request, settings, holder.GetDocument().GetExecutionContext(),
      mojom::RequestContextFrameType::kTopLevel, nullptr);

  EXPECT_FALSE(request.IsAutomaticUpgrade());
  EXPECT_FALSE(request.UpgradeIfInsecure());
}

TEST(MixedContentCheckerTest,
     NotAutoupgradeMixedContentWithLiteralNonLocalIpAddress) {
  test::TaskEnvironment task_environment;
  ResourceRequest request;
  request.SetUrl(KURL("http://8.8.8.8/"));
  request.SetRequestContext(mojom::blink::RequestContextType::AUDIO);
  TestFetchClientSettingsObject* settings =
      MakeGarbageCollected<TestFetchClientSettingsObject>();
  // Used to get a non-null document.
  DummyPageHolder holder;

  MixedContentChecker::UpgradeInsecureRequest(
      request, settings, holder.GetDocument().GetExecutionContext(),
      mojom::RequestContextFrameType::kTopLevel, nullptr);

  EXPECT_FALSE(request.IsAutomaticUpgrade());
  EXPECT_FALSE(request.UpgradeIfInsecure());
}

}  // namespace blink
