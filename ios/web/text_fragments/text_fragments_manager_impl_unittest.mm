// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/text_fragments/text_fragments_manager_impl.h"

#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "components/shared_highlighting/core/common/text_fragments_constants.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::Referrer;
using ::testing::_;
using ::testing::ReturnRefOfCopy;
using shared_highlighting::TextFragmentLinkOpenSource;

namespace {

const char kValidFragmentsURL[] =
    "https://chromium.org#idFrag:~:text=text%201&text=text%202";
const char kScriptForValidFragmentsURL[] =
    "__gCrWeb.textFragments.handleTextFragments([{\"textStart\":\"text "
    "1\"},{\"textStart\":\"text 2\"}], true, null, null)";
const char kScriptForValidFragmentsColorChangeURL[] =
    "__gCrWeb.textFragments.handleTextFragments([{\"textStart\":\"text "
    "1\"},{\"textStart\":\"text 2\"}], true, 'e9d2fd', '000000')";

const char kSingleFragmentURL[] = "https://chromium.org#:~:text=text";
const char kTwoFragmentsURL[] =
    "https://chromium.org#:~:text=text&text=other%20text";

const char kSearchEngineURL[] = "https://google.com";
const char kNonSearchEngineURL[] = "https://notasearchengine.com";

const char kSuccessUkmMetric[] = "Success";
const char kSourceUkmMetric[] = "Source";

}  // namespace

namespace web {

class TextFragmentsManagerImplTest : public WebTest {
 protected:
  TextFragmentsManagerImplTest() : context_(), feature_list_() {}

  void SetUp() override {
    std::unique_ptr<FakeWebState> web_state = std::make_unique<FakeWebState>();
    web_state_ = web_state.get();
    context_.SetWebState(std::move(web_state));
    last_committed_item_.SetReferrer(GetSearchEngineReferrer());
    auto fake_navigation_manager = std::make_unique<FakeNavigationManager>();
    fake_navigation_manager->SetLastCommittedItem(&last_committed_item_);
    web_state_->SetNavigationManager(std::move(fake_navigation_manager));
  }

  TextFragmentsManagerImpl* CreateDefaultManager() {
    return CreateManager(/*has_opener=*/false,
                         /*has_user_gesture=*/true,
                         /*is_same_document=*/false,
                         /*feature_color_change=*/false);
  }

  TextFragmentsManagerImpl* CreateManager(bool has_opener,
                                          bool has_user_gesture,
                                          bool is_same_document,
                                          bool feature_color_change) {
    if (feature_color_change) {
      feature_list_.InitWithFeatures(
          {features::kIOSSharedHighlightingColorChange}, {});
    }
    web_state_->SetHasOpener(has_opener);
    context_.SetHasUserGesture(has_user_gesture);
    context_.SetIsSameDocument(is_same_document);

    TextFragmentsManagerImpl::CreateForWebState(web_state_);
    return TextFragmentsManagerImpl::FromWebState(web_state_);
  }

  void SetLastURL(const GURL& last_url) { web_state_->SetCurrentURL(last_url); }

  Referrer GetSearchEngineReferrer() {
    return Referrer(GURL(kSearchEngineURL), web::ReferrerPolicyDefault);
  }

  Referrer GetNonSearchEngineReferrer() {
    return Referrer(GURL(kNonSearchEngineURL), web::ReferrerPolicyDefault);
  }

  void ValidateLinkOpenedUkm(const ukm::TestAutoSetUkmRecorder& recorder,
                             bool success,
                             TextFragmentLinkOpenSource source) {
    auto entries = recorder.GetEntriesByName(
        ukm::builders::SharedHighlights_LinkOpened::kEntryName);
    ASSERT_EQ(1u, entries.size());
    const ukm::mojom::UkmEntry* entry = entries[0];
    EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
    recorder.ExpectEntryMetric(entry, kSuccessUkmMetric, success);
    recorder.ExpectEntryMetric(entry, kSourceUkmMetric,
                               static_cast<int64_t>(source));
  }

  void ValidateNoLinkOpenedUkm(const ukm::TestAutoSetUkmRecorder& recorder) {
    auto entries = recorder.GetEntriesByName(
        ukm::builders::SharedHighlights_LinkOpened::kEntryName);
    EXPECT_EQ(0u, entries.size());
  }

  web::FakeNavigationContext context_;
  FakeWebState* web_state_;
  base::test::ScopedFeatureList feature_list_;
  NavigationItemImpl last_committed_item_;
};

// Tests that the manager will execute JavaScript if highlighting is allowed and
// fragments are present.
TEST_F(TextFragmentsManagerImplTest, ExecuteJavaScriptSuccess) {
  base::HistogramTester histogram_tester;
  SetLastURL(GURL(kValidFragmentsURL));

  TextFragmentsManagerImpl* manager = CreateDefaultManager();

  manager->DidFinishNavigation(web_state_, &context_);

  std::u16string expected_javascript =
      base::UTF8ToUTF16(kScriptForValidFragmentsURL);
  EXPECT_EQ(expected_javascript, web_state_->GetLastExecutedJavascript());

  // Verify that a command callback was added with the right prefix.
  EXPECT_TRUE(web_state_->GetLastAddedCallback());
  EXPECT_EQ("textFragments", web_state_->GetLastCommandPrefix());
}

// Tests that the manager will execute JavaScript with the default colors
// if the IOSSharedHighlightingColorChange flag is enabled, if highlighting
// is allowed and fragments are present.
TEST_F(TextFragmentsManagerImplTest, ExecuteJavaScriptWithColorChange) {
  base::HistogramTester histogram_tester;
  SetLastURL(GURL(kValidFragmentsURL));

  TextFragmentsManagerImpl* manager =
      CreateManager(/*has_opener=*/false,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/false,
                    /*feature_color_change=*/true);

  // Set up expectation.

  manager->DidFinishNavigation(web_state_, &context_);

  std::u16string expected_javascript =
      base::UTF8ToUTF16(kScriptForValidFragmentsColorChangeURL);
  EXPECT_EQ(expected_javascript, web_state_->GetLastExecutedJavascript());

  // Verify that a command callback was added with the right prefix.
  EXPECT_TRUE(web_state_->GetLastAddedCallback());
  EXPECT_EQ("textFragments", web_state_->GetLastCommandPrefix());
}

// Tests that the manager will not execute JavaScript if the WebState has an
// opener.
TEST_F(TextFragmentsManagerImplTest, HasOpenerFragmentsDisallowed) {
  TextFragmentsManagerImpl* manager =
      CreateManager(/*has_opener=*/true,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/false,
                    /*feature_color_change=*/false);

  manager->DidFinishNavigation(web_state_, &context_);

  EXPECT_EQ(std::u16string(), web_state_->GetLastExecutedJavascript());
}

// Tests that the manager will not execute JavaScript if the WebState has no
// user gesture.
TEST_F(TextFragmentsManagerImplTest, NoGestureFragmentsDisallowed) {
  TextFragmentsManagerImpl* manager =
      CreateManager(/*has_opener=*/false,
                    /*has_user_gesture=*/false,
                    /*is_same_document=*/false,
                    /*feature_color_change=*/false);

  manager->DidFinishNavigation(web_state_, &context_);

  EXPECT_EQ(std::u16string(), web_state_->GetLastExecutedJavascript());
}

// Tests that the manager will not execute JavaScript if we navigated on the
// same document.
TEST_F(TextFragmentsManagerImplTest, SameDocumentFragmentsDisallowed) {
  TextFragmentsManagerImpl* manager =
      CreateManager(/*has_opener=*/false,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/true,
                    /*feature_color_change=*/false);

  manager->DidFinishNavigation(web_state_, &context_);

  EXPECT_EQ(std::u16string(), web_state_->GetLastExecutedJavascript());
}

// Tests that the manager will not execute JavaScript if there are no
// fragments on the current URL.
TEST_F(TextFragmentsManagerImplTest, NoFragmentsNoJavaScript) {
  SetLastURL(GURL("https://www.chromium.org/"));

  TextFragmentsManagerImpl* manager =
      CreateManager(/*has_opener=*/false,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/false,
                    /*feature_color_change=*/false);

  manager->DidFinishNavigation(web_state_, &context_);

  EXPECT_EQ(std::u16string(), web_state_->GetLastExecutedJavascript());
}

// Tests that no metrics are recoded for an URL that doesn't contain text
// fragments.
TEST_F(TextFragmentsManagerImplTest, NoMetricsRecordedIfNoFragmentPresent) {
  base::HistogramTester histogram_tester;

  // Set a URL without text fragments.
  SetLastURL(GURL("https://www.chromium.org/"));

  TextFragmentsManagerImpl* manager = CreateDefaultManager();

  manager->DidFinishNavigation(web_state_, &context_);

  // Make sure no metrics were logged.
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 0);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 0);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.MatchRate", 0);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 0);
}

// Tests that no metrics are recoded for an URL that doesn't contain text
// fragments, even if it contains a fragment id
TEST_F(TextFragmentsManagerImplTest,
       NoMetricsRecordedIfNoFragmentPresentWithFragmentId) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Set a URL without text fragments, but with an id fragment.
  SetLastURL(GURL("https://www.chromium.org/#FragmentID"));

  TextFragmentsManagerImpl* manager = CreateDefaultManager();

  manager->DidFinishNavigation(web_state_, &context_);

  // Make sure no metrics were logged.
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 0);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 0);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.MatchRate", 0);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 0);
}

// Tests that the LinkSource metric is recorded properly when the link comes
// from a search engine.
TEST_F(TextFragmentsManagerImplTest, LinkSourceMetricSearchEngine) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SetLastURL(GURL(kValidFragmentsURL));

  TextFragmentsManagerImpl* manager = CreateDefaultManager();

  manager->DidFinishNavigation(web_state_, &context_);

  histogram_tester.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 1,
                                      1);
}

// Tests that the LinkSource metric is recorded properly when the link doesn't
// come from a search engine.
TEST_F(TextFragmentsManagerImplTest, LinkSourceMetricNonSearchEngine) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SetLastURL(GURL(kValidFragmentsURL));

  TextFragmentsManagerImpl* manager = CreateDefaultManager();

  last_committed_item_.SetReferrer(GetNonSearchEngineReferrer());
  manager->DidFinishNavigation(web_state_, &context_);

  histogram_tester.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 0,
                                      1);
}

// Tests that the SelectorCount metric is recorded properly when a single
// selector is present.
TEST_F(TextFragmentsManagerImplTest, SelectorCountMetricSingleSelector) {
  base::HistogramTester histogram_tester;
  SetLastURL(GURL(kSingleFragmentURL));

  TextFragmentsManagerImpl* manager = CreateDefaultManager();

  manager->DidFinishNavigation(web_state_, &context_);

  histogram_tester.ExpectUniqueSample("TextFragmentAnchor.SelectorCount", 1, 1);
}

// Tests that the SelectorCount metric is recorded properly when two selectors
// are present.
TEST_F(TextFragmentsManagerImplTest, SelectorCountMetricTwoSelectors) {
  base::HistogramTester histogram_tester;
  SetLastURL(GURL(kTwoFragmentsURL));

  TextFragmentsManagerImpl* manager = CreateDefaultManager();

  manager->DidFinishNavigation(web_state_, &context_);

  histogram_tester.ExpectUniqueSample("TextFragmentAnchor.SelectorCount", 2, 1);
}

// Tests that the AmbiguousMatch and MatchRate success metrics are recorded
// properly in a variety of cases.
TEST_F(TextFragmentsManagerImplTest,
       DidReceiveJavaScriptResponseSuccessMetrics) {
  SetLastURL(GURL(kTwoFragmentsURL));
  TextFragmentsManagerImpl* manager = CreateDefaultManager();

  manager->DidFinishNavigation(web_state_, &context_);

  auto maybe_callback = web_state_->GetLastAddedCallback();
  ASSERT_TRUE(maybe_callback);
  web::WebState::ScriptCommandCallback parse_function = maybe_callback.value();
  auto fake_main_frame = web::FakeWebFrame::Create(
      /*frame_id=*/"", /*is_main_frame=*/true, GURL());

  // 100% rate case.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    base::DictionaryValue js_response = base::DictionaryValue();
    js_response.SetKey("command", base::Value("textFragments.response"));
    js_response.SetDoublePath("result.fragmentsCount", 2);
    js_response.SetDoublePath("result.successCount", 2);

    parse_function.Run(js_response, GURL("https://text.com"),
                       /*interacted=*/true, fake_main_frame.get());

    histogram_tester.ExpectUniqueSample("TextFragmentAnchor.AmbiguousMatch", 0,
                                        1);
    histogram_tester.ExpectUniqueSample("TextFragmentAnchor.MatchRate", 100, 1);

    ValidateLinkOpenedUkm(ukm_recorder, /*success=*/true,
                          TextFragmentLinkOpenSource::kSearchEngine);
  }

  // 50% rate case.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    base::DictionaryValue js_response = base::DictionaryValue();
    js_response.SetKey("command", base::Value("textFragments.response"));
    js_response.SetDoublePath("result.fragmentsCount", 6);
    js_response.SetDoublePath("result.successCount", 3);

    parse_function.Run(js_response, GURL("https://text.com"),
                       /*interacted=*/true, fake_main_frame.get());

    histogram_tester.ExpectUniqueSample("TextFragmentAnchor.AmbiguousMatch", 1,
                                        1);
    histogram_tester.ExpectUniqueSample("TextFragmentAnchor.MatchRate", 50, 1);

    ValidateLinkOpenedUkm(ukm_recorder, /*success=*/false,
                          TextFragmentLinkOpenSource::kSearchEngine);
  }

  // 0% rate case.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    base::DictionaryValue js_response = base::DictionaryValue();
    js_response.SetKey("command", base::Value("textFragments.response"));
    js_response.SetDoublePath("result.fragmentsCount", 2);
    js_response.SetDoublePath("result.successCount", 0);

    parse_function.Run(js_response, GURL("https://text.com"),
                       /*interacted=*/true, fake_main_frame.get());

    histogram_tester.ExpectUniqueSample("TextFragmentAnchor.AmbiguousMatch", 1,
                                        1);
    histogram_tester.ExpectUniqueSample("TextFragmentAnchor.MatchRate", 0, 1);

    ValidateLinkOpenedUkm(ukm_recorder, /*success=*/false,
                          TextFragmentLinkOpenSource::kSearchEngine);
  }

  // Invalid values case - negative numbers.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    base::DictionaryValue js_response = base::DictionaryValue();
    js_response.SetKey("command", base::Value("textFragments.response"));
    js_response.SetDoublePath("result.fragmentsCount", -3);
    js_response.SetDoublePath("result.successCount", 4);

    parse_function.Run(js_response, GURL("https://text.com"),
                       /*interacted=*/true, fake_main_frame.get());

    histogram_tester.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 0);
    histogram_tester.ExpectTotalCount("TextFragmentAnchor.MatchRate", 0);

    ValidateNoLinkOpenedUkm(ukm_recorder);
  }

  // Invalid values case - not numbers.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    base::DictionaryValue js_response = base::DictionaryValue();
    js_response.SetKey("command", base::Value("textFragments.response"));
    js_response.SetStringPath("result.fragmentsCount", "a weird value");
    js_response.SetDoublePath("result.successCount", 4);

    parse_function.Run(js_response, GURL("https://text.com"),
                       /*interacted=*/true, fake_main_frame.get());

    histogram_tester.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 0);
    histogram_tester.ExpectTotalCount("TextFragmentAnchor.MatchRate", 0);

    ValidateNoLinkOpenedUkm(ukm_recorder);
  }
}

}  // namespace web
