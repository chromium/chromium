// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_test.h"

#import <memory>

#import "base/notreached.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/dom_distiller/core/extraction_utils.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state.h"
#import "third_party/dom_distiller_js/dom_distiller.pb.h"
#import "third_party/dom_distiller_js/dom_distiller_json_converter.h"

ReaderModeTest::ReaderModeTest() = default;
ReaderModeTest::~ReaderModeTest() = default;

void ReaderModeTest::SetUp() {
  base::FieldTrialParams custom_time_params = {
      {kReaderModeHeuristicPageLoadDelayDurationStringName, "1s"},
      {kReaderModeDistillationTimeoutDurationStringName, "5s"}};
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{kEnableReaderMode, custom_time_params}},
      /*disabled_features=*/{});
  profile_ = TestProfileIOS::Builder().Build();

  web::JavaScriptFeatureManager::FromBrowserState(profile_.get())
      ->ConfigureFeatures({ReaderModeJavaScriptFeature::GetInstance()});
}

std::unique_ptr<web::FakeWebState> ReaderModeTest::CreateWebState() {
  CHECK(profile()) << "SetUp must be called prior to web state creation";

  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());

  // Attach tab helpers
  ReaderModeTabHelper::CreateForWebState(
      web_state.get(), DistillerServiceFactory::GetForProfile(profile()));

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

  if (base::FeatureList::IsEnabled(kEnableReadabilityHeuristic)) {
    AddReadabilityHeuristicResultToFrame(result, web_frame);
  }

  // Set up the fake web frame to return a custom result after executing
  // the DOM distiller Javascript.
  dom_distiller::proto::DomDistillerOptions options;
  std::u16string script =
      base::UTF8ToUTF16(dom_distiller::GetDistillerScriptWithOptions(options));
  dom_distiller::proto::DomDistillerResult distiller_result;
  distiller_result.mutable_distilled_content()->set_html(
      std::move(distilled_content));
  base::Value distiller_result_value =
      dom_distiller::proto::json::DomDistillerResult::WriteToValue(
          std::move(distiller_result));
  distiller_result_values_.push_back(
      std::make_unique<base::Value>(std::move(distiller_result_value)));
  web_frame->AddResultForExecutedJs(distiller_result_values_.back().get(),
                                    script);
  auto* tab_helper = ReaderModeTabHelper::FromWebState(web_state);
  if (!tab_helper) {
    return;
  }
  web_frame->set_call_java_script_function_callback(base::BindRepeating(^{
    // Overrides the result from DOM distiller heuristic with a custom entry.
    tab_helper->HandleReaderModeHeuristicResult(url, result);
  }));
}

void ReaderModeTest::WaitForPageLoadDelayAndRunUntilIdle() {
  // Waits for asynchronous trigger heuristic delay
  // `kReaderModeHeuristicPageLoadDelay` after the page is loaded.
  task_environment_.AdvanceClock(base::Seconds(1));
  task_environment_.RunUntilIdle();
}

bool ReaderModeTest::WaitForAvailableReaderModeContentInWebState(
    web::WebState* web_state) {
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, true, ^{
        return ReaderModeTabHelper::FromWebState(web_state)
                   ->GetReaderModeWebState() != nullptr;
      });
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
  }
}
