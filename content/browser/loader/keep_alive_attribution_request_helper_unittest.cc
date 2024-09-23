// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_attribution_request_helper.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/registration_eligibility.mojom-shared.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_background_registrations_id.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_suitable_context.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "net/http/http_response_headers.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class KeepAliveAttributionRequestHelperTestPeer {
 public:
  static BackgroundRegistrationsId GetHelperId(
      KeepAliveAttributionRequestHelper& helper) {
    return helper.id_;
  }
};

namespace {

using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::RegistrationEligibility;
using ::network::mojom::AttributionReportingEligibility;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Property;
using ::testing::Return;

constexpr char kRegisterSourceJson[] =
    R"json({"destination":"https://destination.example"})json";
constexpr char kRegisterTriggerJson[] = R"json({ })json";

using attribution_reporting::kAttributionReportingRegisterOsSourceHeader;
using attribution_reporting::kAttributionReportingRegisterOsTriggerHeader;
using attribution_reporting::kAttributionReportingRegisterSourceHeader;
using attribution_reporting::kAttributionReportingRegisterTriggerHeader;

class KeepAliveAttributionRequestHelperTest : public RenderViewHostTestHarness {
 public:
  KeepAliveAttributionRequestHelperTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_api_state_setting_(
            AttributionOsLevelManager::ScopedApiStateForTesting(
                AttributionOsLevelManager::ApiState::kEnabled)) {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kKeepAliveInBrowserMigration,
         blink::features::kAttributionReportingInBrowserMigration,
         network::features::kAttributionReportingCrossAppWeb},
        {});
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    auto mock_manager = std::make_unique<MockAttributionManager>();
    auto data_host_manager =
        std::make_unique<AttributionDataHostManagerImpl>(mock_manager.get());
    mock_manager->SetDataHostManager(std::move(data_host_manager));
    mock_attribution_manager_ = mock_manager.get();

    OverrideAttributionManager(std::move(mock_manager));

    test_web_contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
  }

  void TearDown() override {
    // Avoids dangling ref to `mock_attribution_manager_`.
    mock_attribution_manager_ = nullptr;
    OverrideAttributionManager(nullptr);
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  TestWebContents* test_web_contents() {
    return static_cast<TestWebContents*>(web_contents());
  }

  MockAttributionManager* mock_attribution_manager() {
    return mock_attribution_manager_;
  }

  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }

  std::unique_ptr<KeepAliveAttributionRequestHelper> CreateValidHelper(
      const GURL& reporting_url,
      AttributionReportingEligibility eligibility =
          AttributionReportingEligibility::kEventSourceOrTrigger,
      const std::optional<base::UnguessableToken>& attribution_src_token =
          std::nullopt,
      const GURL& context_url = GURL("https://secure_source.com")) {
    test_web_contents()->NavigateAndCommit(context_url);

    auto helper = KeepAliveAttributionRequestHelper::CreateIfNeeded(
        eligibility, reporting_url, attribution_src_token,
        "devtools-request-id",
        *AttributionSuitableContext::Create(
            test_web_contents()->GetPrimaryMainFrame()->GetGlobalId()));

    CHECK(helper);

    return helper;
  }

 private:
  void OverrideAttributionManager(std::unique_ptr<AttributionManager> manager) {
    static_cast<StoragePartitionImpl*>(
        browser_context()->GetDefaultStoragePartition())
        ->OverrideAttributionManagerForTesting(std::move(manager));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting_;

  raw_ptr<MockAttributionManager> mock_attribution_manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(KeepAliveAttributionRequestHelperTest, SingleResponse) {
  const GURL source_url = GURL("https://secure_source.com");
  const GURL reporting_url("https://report.test");
  auto helper = CreateValidHelper(
      reporting_url, AttributionReportingEligibility::kEventSourceOrTrigger,
      /*attribution_src_token=*/std::nullopt, source_url);

  EXPECT_CALL(
      *mock_attribution_manager(),
      HandleSource(
          AllOf(ImpressionOriginIs(*SuitableOrigin::Create(source_url)),
                ReportingOriginIs(*SuitableOrigin::Create(reporting_url))),
          test_web_contents()->GetPrimaryMainFrame()->GetGlobalId()))
      .Times(1);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  helper->OnReceiveResponse(headers.get());

  // Wait for parsing to complete
  task_environment()->FastForwardBy(base::TimeDelta());
}

TEST_F(KeepAliveAttributionRequestHelperTest, NavigationSource) {
  const std::optional<base::UnguessableToken> attribution_src_token =
      base::UnguessableToken(blink::AttributionSrcToken());
  const GURL reporting_url("https://report.test");
  auto helper = CreateValidHelper(
      reporting_url, AttributionReportingEligibility::kNavigationSource,
      attribution_src_token);

  // HandleSource won't be called given that we haven't setup an actual
  // navigation necessary for the registration to complete. The fact that it
  // isn't called confirms that the token is being used.
  EXPECT_CALL(*mock_attribution_manager(), HandleSource).Times(0);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);
  helper->OnReceiveResponse(headers.get());

  task_environment()->FastForwardBy(base::TimeDelta());
}

TEST_F(KeepAliveAttributionRequestHelperTest, ExtraResponsesAreIgnored) {
  const GURL reporting_url("https://report.test");
  auto helper = CreateValidHelper(reporting_url);

  EXPECT_CALL(*mock_attribution_manager(), HandleSource).Times(1);

  auto headers_1 = base::MakeRefCounted<net::HttpResponseHeaders>("");

  headers_1->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);

  helper->OnReceiveResponse(headers_1.get());

  auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");

  headers_2->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  // This a valid response, however the helper processed a response, i.e.,
  // OnReceiveResponse was previously called. As such, this response should be
  // ignored.
  helper->OnReceiveResponse(headers_2.get());

  // Wait for parsing to complete
  task_environment()->FastForwardBy(base::TimeDelta());
}

TEST_F(KeepAliveAttributionRequestHelperTest,
       UnexpectedResponsesWillBeIgnored) {
  const GURL reporting_url("https://report.test");
  auto helper = CreateValidHelper(reporting_url);

  // The second response should be ignored as a helper can only
  // process a single response.
  EXPECT_CALL(*mock_attribution_manager(), HandleSource).Times(1);

  auto headers_1 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_1->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  helper->OnReceiveResponse(headers_1.get());

  auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");

  headers_2->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  helper->OnReceiveResponse(headers_2.get());

  // Wait for parsing to complete
  task_environment()->FastForwardBy(base::TimeDelta());
}

TEST_F(KeepAliveAttributionRequestHelperTest, NoAttributionHeader) {
  const GURL reporting_url("https://report.test");
  auto helper = CreateValidHelper(reporting_url);

  EXPECT_CALL(*mock_attribution_manager(), HandleSource).Times(0);

  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader("random-header", kRegisterSourceJson);

  helper->OnReceiveResponse(headers.get());

  // Wait for parsing even if none is expected to avoid false positive.
  task_environment()->FastForwardBy(base::TimeDelta());
}

TEST_F(KeepAliveAttributionRequestHelperTest, Cleanup) {
  const GURL reporting_url("https://report.test");

  auto helper = CreateValidHelper(reporting_url);

  AttributionDataHostManager* host =
      mock_attribution_manager()->GetDataHostManager();
  CHECK(host);

  BackgroundRegistrationsId background_id =
      KeepAliveAttributionRequestHelperTestPeer::GetHelperId(*helper);

  const auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader(kAttributionReportingRegisterSourceHeader,
                     kRegisterSourceJson);

  const auto attempt_to_register_data = [&]() -> bool {
    return host->NotifyBackgroundRegistrationData(background_id, &(*headers),
                                                  reporting_url);
  };

  // Call twice to show that we can call multiple times to register multiple
  // headers.
  ASSERT_TRUE(attempt_to_register_data());
  ASSERT_TRUE(attempt_to_register_data());

  // reset without having received a response.
  helper.reset();
  task_environment()->FastForwardBy(base::TimeDelta());

  // The registration should have been completed upon the helper being reset.
  // Once completed, it is not longer possible to register headers with the id.
  EXPECT_FALSE(attempt_to_register_data());
}

TEST_F(KeepAliveAttributionRequestHelperTest, RedirectChain) {
  // 0 won't have any header
  const GURL reporting_url_0("https://report-0.test");
  EXPECT_CALL(
      *mock_attribution_manager(),
      HandleSource(ReportingOriginIs(*SuitableOrigin::Create(reporting_url_0)),
                   _))
      .Times(0);

  // 1 will have an empty header
  const GURL reporting_url_1("https://report-1.test");
  EXPECT_CALL(
      *mock_attribution_manager(),
      HandleSource(ReportingOriginIs(*SuitableOrigin::Create(reporting_url_1)),
                   _))
      .Times(0);

  // 2 will register a source
  const GURL reporting_url_2("https://report-2.test");
  EXPECT_CALL(
      *mock_attribution_manager(),
      HandleSource(ReportingOriginIs(*SuitableOrigin::Create(reporting_url_2)),
                   _))
      .Times(1);

  // 3 will register a trigger
  const GURL reporting_url_3("https://report-3.test");
  EXPECT_CALL(*mock_attribution_manager(),
              HandleTrigger(Property(&AttributionTrigger::reporting_origin,
                                     *SuitableOrigin::Create(reporting_url_3)),
                            _))
      .Times(1);

  // 4 is not suitable, so it's response should be ignored.
  const GURL reporting_url_4("http://report-4.test");

  // 5 will register a source.
  const GURL reporting_url_5("https://report-5.test");
  EXPECT_CALL(
      *mock_attribution_manager(),
      HandleSource(ReportingOriginIs(*SuitableOrigin::Create(reporting_url_5)),
                   _))
      .Times(1);

  // 6 will register an OS source
  const GURL reporting_url_6("https://report-6.test");
  // 7 will register an OS trigger
  const GURL reporting_url_7("https://report-7.test");
  EXPECT_CALL(*mock_attribution_manager(), HandleOsRegistration).Times(2);

  auto helper = CreateValidHelper(reporting_url_0);

  helper->OnReceiveRedirect(/*headers=*/nullptr, reporting_url_1);

  auto headers_1 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  helper->OnReceiveRedirect(headers_1.get(), reporting_url_2);

  auto headers_2 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_2->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  helper->OnReceiveRedirect(headers_2.get(), reporting_url_3);

  auto headers_3 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_3->SetHeader(kAttributionReportingRegisterTriggerHeader,
                       kRegisterTriggerJson);
  helper->OnReceiveRedirect(headers_3.get(), reporting_url_4);

  auto headers_4 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_4->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  helper->OnReceiveRedirect(headers_4.get(), reporting_url_5);

  auto headers_5 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_5->SetHeader(kAttributionReportingRegisterSourceHeader,
                       kRegisterSourceJson);
  helper->OnReceiveRedirect(headers_5.get(), reporting_url_6);

  auto headers_6 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_6->SetHeader(kAttributionReportingRegisterOsSourceHeader,
                       R"("https://r.test/x")");
  helper->OnReceiveRedirect(headers_6.get(), reporting_url_7);

  auto headers_7 = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers_7->SetHeader(kAttributionReportingRegisterOsTriggerHeader,
                       R"("https://r.test/x")");
  helper->OnReceiveResponse(headers_7.get());

  // Wait for parsing to complete
  task_environment()->FastForwardBy(base::TimeDelta());
}

TEST_F(KeepAliveAttributionRequestHelperTest, HelperNotNeeded) {
  const GURL reporting_url("https://report.test");

  {  // insecure context origin
    const GURL source_url("http://insecure.test");
    test_web_contents()->NavigateAndCommit(source_url);

    auto context = AttributionSuitableContext::Create(
        test_web_contents()->GetPrimaryMainFrame()->GetGlobalId());
    EXPECT_FALSE(context.has_value());
  }

  {  // ineligible request - kEmpty
    const GURL source_url("https://secure.test");
    test_web_contents()->NavigateAndCommit(source_url);
    auto context = AttributionSuitableContext::Create(
        test_web_contents()->GetPrimaryMainFrame()->GetGlobalId());
    ASSERT_TRUE(context.has_value());
    auto helper = KeepAliveAttributionRequestHelper::CreateIfNeeded(
        AttributionReportingEligibility::kEmpty, reporting_url,
        /*attribution_src_token=*/std::nullopt, "devtools-request-id",
        context.value());
    EXPECT_EQ(helper, nullptr);
  }

  {  // kAttributionReportingInBrowserMigration disabled
    scoped_feature_list().Reset();
    scoped_feature_list().InitAndEnableFeature(
        blink::features::kKeepAliveInBrowserMigration);
    const GURL source_url("https://secure.test");
    test_web_contents()->NavigateAndCommit(source_url);

    auto context = AttributionSuitableContext::Create(
        test_web_contents()->GetPrimaryMainFrame()->GetGlobalId());
    ASSERT_TRUE(context.has_value());
    auto helper = KeepAliveAttributionRequestHelper::CreateIfNeeded(
        AttributionReportingEligibility::kEventSourceOrTrigger, reporting_url,
        /*attribution_src_token=*/std::nullopt, "devtools-request-id",
        context.value());
    EXPECT_EQ(helper, nullptr);
  }
}

TEST_F(KeepAliveAttributionRequestHelperTest, Eligibility) {
  const struct {
    AttributionReportingEligibility eligibility;
    bool can_register_source;
    bool can_register_trigger;
  } kTestCases[] = {
      {
          .eligibility = AttributionReportingEligibility::kUnset,
          .can_register_source = false,
          .can_register_trigger = true,
      },
      {
          .eligibility = AttributionReportingEligibility::kTrigger,
          .can_register_source = false,
          .can_register_trigger = true,
      },
      {
          .eligibility = AttributionReportingEligibility::kEventSource,
          .can_register_source = true,
          .can_register_trigger = false,
      },
      {
          .eligibility = AttributionReportingEligibility::kNavigationSource,
          .can_register_source = true,
          .can_register_trigger = false,
      },
      {
          .eligibility = AttributionReportingEligibility::kEventSourceOrTrigger,
          .can_register_source = true,
          .can_register_trigger = true,
      },
  };

  for (const auto& test_case : kTestCases) {
    const GURL reporting_url_0("https://report-source." +
                               base::ToString(test_case.eligibility));
    const GURL reporting_url_1("https://report-trigger." +
                               base::ToString(test_case.eligibility));
    EXPECT_CALL(
        *mock_attribution_manager(),
        HandleSource(
            ReportingOriginIs(*SuitableOrigin::Create(reporting_url_0)), _))
        .Times(test_case.can_register_source);
    EXPECT_CALL(
        *mock_attribution_manager(),
        HandleTrigger(Property(&AttributionTrigger::reporting_origin,
                               *SuitableOrigin::Create(reporting_url_1)),
                      _))
        .Times(test_case.can_register_trigger);

    auto helper = CreateValidHelper(reporting_url_0, test_case.eligibility);

    auto headers_0 = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers_0->SetHeader(kAttributionReportingRegisterSourceHeader,
                         kRegisterSourceJson);
    helper->OnReceiveRedirect(headers_0.get(), reporting_url_1);

    auto headers_1 = base::MakeRefCounted<net::HttpResponseHeaders>("");
    headers_1->SetHeader(kAttributionReportingRegisterTriggerHeader,
                         kRegisterTriggerJson);
    helper->OnReceiveResponse(headers_1.get());
  }

  // Wait for parsing to complete
  task_environment()->FastForwardBy(base::TimeDelta());
}

}  // namespace
}  // namespace content
