// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_test.h"

#import <memory>

#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/dom_distiller/core/extraction_utils.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/language/ios/browser/language_detection_java_script_feature.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/fake_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_scroll_anchor_java_script_feature.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_client.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/web_state.h"
#import "third_party/dom_distiller_js/dom_distiller.pb.h"
#import "third_party/dom_distiller_js/dom_distiller_json_converter.h"

namespace {

std::unique_ptr<KeyedService> BuildSafeBrowsingClient(ProfileIOS* profile) {
  return std::make_unique<FakeSafeBrowsingClient>(
      GetApplicationContext()->GetLocalState());
}

std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    ProfileIOS* profile) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}

}  // namespace

ReaderModeTest::ReaderModeTest()
    : language_detection_model_(
          std::make_unique<language_detection::LanguageDetectionModel>()) {}

ReaderModeTest::~ReaderModeTest() = default;

void ReaderModeTest::SetUp() {
  scoped_feature_list_.InitAndEnableFeature(kEnableReaderModeInUS);
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      OptimizationGuideServiceFactory::GetInstance(),
      OptimizationGuideServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(SafeBrowsingClientFactory::GetInstance(),
                            base::BindOnce(&BuildSafeBrowsingClient));
  builder.AddTestingFactory(
      feature_engagement::TrackerFactory::GetInstance(),
      base::BindRepeating(&BuildFeatureEngagementMockTracker));
  profile_ = std::move(builder).Build();

  // Ensure that kOfferTranslateEnabled is enabled.
  profile_->GetPrefs()->SetBoolean(translate::prefs::kOfferTranslateEnabled,
                                   true);

  web::test::OverrideJavaScriptFeatures(
      profile_.get(),
      {ReaderModeJavaScriptFeature::GetInstance(),
       ReaderModeScrollAnchorJavaScriptFeature::GetInstance(),
       language::LanguageDetectionJavaScriptFeature::GetInstance()});
}

void ReaderModeTest::TearDown() {
  scoped_feature_list_.Reset();
  PlatformTest::TearDown();
}

std::unique_ptr<web::FakeWebState> ReaderModeTest::CreateWebState() {
  CHECK(profile()) << "SetUp must be called prior to web state creation";

  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());

  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  web_state->SetNavigationManager(std::move(fake_navigation_manager));

  // Attach tab helpers
  ReaderModeTabHelper::CreateForWebState(
      web_state.get(), DistillerServiceFactory::GetForProfile(profile()));
  SnapshotSourceTabHelper::CreateForWebState(web_state.get());
  OverlayRequestQueue::CreateForWebState(web_state.get());
  InfoBarManagerImpl::CreateForWebState(web_state.get());
  InfobarOverlayRequestInserter::CreateForWebState(
      web_state.get(), &FakeInfobarOverlayRequestFactory);

  return web_state;
}

void ReaderModeTest::EnableReaderMode(web::WebState* web_state,
                                      ReaderModeAccessPoint access_point) {
  ReaderModeTabHelper::FromWebState(web_state)->ActivateReader(access_point);
}

void ReaderModeTest::DisableReaderMode(web::WebState* web_state) {
  ReaderModeTabHelper::FromWebState(web_state)->DeactivateReader();
}

void ReaderModeTest::LoadWebpage(web::FakeWebState* web_state,
                                 const GURL& url) {
  web::FakeNavigationContext navigation_context;
  navigation_context.SetHasCommitted(true);
  web_state->OnNavigationStarted(&navigation_context);
  web_state->LoadSimulatedRequest(url, @"<html><body>Content</body></html>");
  web_state->OnNavigationFinished(&navigation_context);
}

void ReaderModeTest::SetReaderModeState(web::FakeWebState* web_state,
                                        const GURL& url,
                                        ReaderModeHeuristicResult result,
                                        std::string distilled_content) {
  // Set up the fake web frame to return a custom result after executing
  // the heuristic Javascript callback.
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(url);
  main_frame->set_browser_state(profile_.get());
  auto web_frame = main_frame.get();

  // Set up the fake web frames manager.
  auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
  web::FakeWebFramesManager* web_frames_manager = frames_manager.get();
  web_state->SetWebFramesManager(std::make_unique<web::FakeWebFramesManager>());
  web_state->SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                 std::move(frames_manager));
  web_frames_manager->AddWebFrame(std::move(main_frame));

  // Language detection tab helper requires the web frames manager for isolated
  // world content.
  language::IOSLanguageDetectionTabHelper::CreateForWebState(
      web_state, /*url_language_histogram=*/nullptr, &language_detection_model_,
      profile_->GetPrefs());

  if (base::FeatureList::IsEnabled(kEnableReadabilityHeuristic)) {
    AddReadabilityHeuristicResultToFrame(result, web_frame);
  }

  // Set up the fake web frame to return a custom result after executing
  // the Readability Javascript.
  std::u16string readability_script =
      base::UTF8ToUTF16(dom_distiller::GetReadabilityDistillerScript());
  base::DictValue readability_result;
  readability_result.Set("content", distilled_content);
  readability_result.Set("title", "fake title");
  distiller_result_values_.push_back(
      std::make_unique<base::Value>(std::move(readability_result)));
  web_frame->AddResultForExecutedJs(distiller_result_values_.back().get(),
                                    readability_script);

  web_frame->SetJavaScriptFunctionCallback(
      "readerMode.retrieveDOMFeatures",
      base::BindRepeating(&ReaderModeTest::OnDomFeaturesRetrieved,
                          weak_ptr_factory_.GetWeakPtr(),
                          web_state->GetWeakPtr(), web_frame->AsWeakPtr(),
                          result));
}

void ReaderModeTest::WaitForPageLoadDelayAndRunUntilIdle() {
  // Waits for asynchronous trigger heuristic delay
  // `kReaderModeHeuristicPageLoadDelay` after the page is loaded.
  task_environment_.AdvanceClock(base::Seconds(1));
  task_environment_.RunUntilIdle();
}

bool ReaderModeTest::WaitForAvailableReaderModeContentInWebState(
    web::WebState* web_state) {
  // For the Reader mode WebState to be ready, distillation must complete
  // (JavaScript completion) and the distilled content must be loaded in to the
  // Reader mode WebState (page load).
  constexpr base::TimeDelta timeout =
      base::test::ios::kWaitForJSCompletionTimeout +
      base::test::ios::kWaitForPageLoadTimeout;
  return base::test::ios::WaitUntilConditionOrTimeout(
      timeout, true, ^{
        return ReaderModeTabHelper::FromWebState(web_state)
                   ->GetReaderModeWebState() != nullptr;
      });
}

void ReaderModeTest::OnDomFeaturesRetrieved(
    base::WeakPtr<web::WebState> weak_web_state,
    base::WeakPtr<web::WebFrame> weak_web_frame,
    ReaderModeHeuristicResult result) {
  web::WebState* web_state = weak_web_state.get();
  web::WebFrame* web_frame = weak_web_frame.get();
  if (!web_state || !web_frame) {
    return;
  }

  auto* tab_helper = ReaderModeTabHelper::FromWebState(web_state);
  if (!tab_helper) {
    return;
  }

  tab_helper->HandleReaderModeHeuristicResult(result);
}

void ReaderModeTest::AddReadabilityHeuristicResultToFrame(
    ReaderModeHeuristicResult result,
    web::FakeWebFrame* web_frame) {
  std::u16string readability_heuristic_script =
      base::UTF8ToUTF16(dom_distiller::GetReadabilityTriggeringScript());
  switch (result) {
    case ReaderModeHeuristicResult::kReaderModeEligible:
      readability_heuristic_value_ = std::make_unique<base::Value>(true);
      break;
    case ReaderModeHeuristicResult::kMalformedResponse:
      readability_heuristic_value_ = std::make_unique<base::Value>();
      break;
    case ReaderModeHeuristicResult::kReaderModeNotEligibleContentAndLength:
      readability_heuristic_value_ = std::make_unique<base::Value>(false);
      break;
    case ReaderModeHeuristicResult::kReaderModeNotEligibleContentOnly:
    case ReaderModeHeuristicResult::kReaderModeNotEligibleContentLength:
    case ReaderModeHeuristicResult::
        kReaderModeNotEligibleOptimizationGuideIneligible:
    case ReaderModeHeuristicResult::
        kReaderModeNotEligibleOptimizationGuideUnknown:
      NOTREACHED();
  }
  web_frame->AddResultForExecutedJs(readability_heuristic_value_.get(),
                                    readability_heuristic_script);
}

// static
std::string ReaderModeTest::TestParametersReaderModeHeuristicResultToString(
    testing::TestParamInfo<ReaderModeHeuristicResult> info) {
  switch (info.param) {
    case ReaderModeHeuristicResult::kMalformedResponse:
      return "MalformedResponse";
    case ReaderModeHeuristicResult::kReaderModeEligible:
      return "ReaderModeEligible";
    case ReaderModeHeuristicResult::kReaderModeNotEligibleContentOnly:
      return "ReaderModeNotEligibleContentOnly";
    case ReaderModeHeuristicResult::kReaderModeNotEligibleContentLength:
      return "ReaderModeNotEligibleContentLength";
    case ReaderModeHeuristicResult::kReaderModeNotEligibleContentAndLength:
      return "ReaderModeNotEligibleContentAndLength";
    case ReaderModeHeuristicResult::
        kReaderModeNotEligibleOptimizationGuideIneligible:
      return "ReaderModeNotEligibleOptimizationGuideIneligible";
    case ReaderModeHeuristicResult::
        kReaderModeNotEligibleOptimizationGuideUnknown:
      return "ReaderModeNotEligibleOptimizationGuideUnknown";
  }
}
