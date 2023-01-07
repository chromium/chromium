// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/text_fragments/text_fragments_manager_impl.h"

#import <functional>

#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/shared_highlighting/core/common/fragment_directives_constants.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
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
using ::testing::Eq;
using shared_highlighting::TextFragmentLinkOpenSource;

namespace {

const char kValidFragmentsURL[] =
    "https://chromium.org#idFrag:~:text=text%201&text=text%202";

const char kSingleFragmentURL[] = "https://chromium.org#:~:text=text";
const char kTwoFragmentsURL[] =
    "https://chromium.org#:~:text=text&text=other%20text";
const char kFragmentsRemovedURL[] = "https://chromium.org";

const char kSearchEngineURL[] = "https://google.com";
const char kNonSearchEngineURL[] = "https://notasearchengine.com";

const char kSuccessUkmMetric[] = "Success";
const char kSourceUkmMetric[] = "Source";

class MockJSFeature : public web::TextFragmentsJavaScriptFeature {
 public:
  MOCK_METHOD(void,
              ProcessTextFragments,
              (web::WebState * web_state,
               base::Value parsed_fragments,
               std::string background_color_hex_rgb,
               std::string foreground_color_hex_rgb),
              (override));
  MOCK_METHOD(void,
              RemoveHighlights,
              (web::WebState * web_state, const GURL& new_url),
              (override));
};

base::Value ValueForTestURL() {
  base::Value val(base::Value::Type::LIST);

  base::Value text1(base::Value::Type::DICTIONARY);
  text1.SetStringKey("textStart", "text 1");
  base::Value text2(base::Value::Type::DICTIONARY);
  text2.SetStringKey("textStart", "text 2");

  val.Append(std::move(text1));
  val.Append(std::move(text2));

  return val;
}

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
    auto fake_web_frames_manager = std::make_unique<FakeWebFramesManager>();
    web_state_->SetWebFramesManager(std::move(fake_web_frames_manager));
  }

  TextFragmentsManagerImpl* CreateDefaultManager() {
    return CreateManager(/*has_opener=*/false,
                         /*has_user_gesture=*/true,
                         /*is_same_document=*/false,
                         /*feature_color_change=*/false,
                         /*add_web_frame=*/true);
  }

  TextFragmentsManagerImpl* CreateManager(bool has_opener,
                                          bool has_user_gesture,
                                          bool is_same_document,
                                          bool feature_color_change,
                                          bool add_web_frame) {
    if (!feature_color_change) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kIOSSharedHighlightingColorChange});
    }
    web_state_->SetHasOpener(has_opener);
    context_.SetHasUserGesture(has_user_gesture);
    context_.SetIsSameDocument(is_same_document);

    TextFragmentsManagerImpl::CreateForWebState(web_state_);
    auto* manager = TextFragmentsManagerImpl::FromWebState(web_state_);
    manager->SetJSFeatureForTesting(&feature_);
    if (add_web_frame) {
      AddMainWebFrame(manager);
    }
    return manager;
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

  void AddMainWebFrame(TextFragmentsManagerImpl* fragments_mgr) {
    FakeWebFramesManager* frames_mgr =
        static_cast<FakeWebFramesManager*>(web_state_->GetWebFramesManager());
    frames_mgr->AddWebFrame(
        FakeWebFrame::CreateMainWebFrame(GURL("https://chromium.org")));
    fragments_mgr->WebFrameDidBecomeAvailable(web_state_,
                                              frames_mgr->GetMainWebFrame());
  }

  MockJSFeature feature_;
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

  base::Value expected = ValueForTestURL();
  EXPECT_CALL(feature_,
              ProcessTextFragments(web_state_, Eq(std::ref(expected)), "", ""));

  TextFragmentsManagerImpl* manager = CreateDefaultManager();
  manager->DidFinishNavigation(web_state_, &context_);
}

// Tests that JS still executes even if the main WebFrame isn't yet available
// when the navigation finishes.
TEST_F(TextFragmentsManagerImplTest, ExecuteJavaScriptDelayedWebFrame) {
  base::HistogramTester histogram_tester;
  SetLastURL(GURL(kValidFragmentsURL));

  base::Value expected = ValueForTestURL();
  EXPECT_CALL(feature_,
              ProcessTextFragments(web_state_, Eq(std::ref(expected)), "", ""));

  TextFragmentsManagerImpl* manager =
      CreateManager(/*has_opener=*/false,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/false,
                    /*feature_color_change=*/false,
                    /*add_web_frame=*/false);
  manager->DidFinishNavigation(web_state_, &context_);
  AddMainWebFrame(manager);
}

// Tests that the manager will execute JavaScript with the default colors
// if the IOSSharedHighlightingColorChange flag is enabled, if highlighting
// is allowed and fragments are present.
TEST_F(TextFragmentsManagerImplTest, ExecuteJavaScriptWithColorChange) {
  base::HistogramTester histogram_tester;
  SetLastURL(GURL(kValidFragmentsURL));

  base::Value expected = ValueForTestURL();
  EXPECT_CALL(feature_, ProcessTextFragments(web_state_, Eq(std::ref(expected)),
                                             "e9d2fd", "000000"));

  TextFragmentsManagerImpl* manager =
      CreateManager(/*has_opener=*/false,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/false,
                    /*feature_color_change=*/true,
                    /*add_web_frame=*/true);
  manager->DidFinishNavigation(web_state_, &context_);
}

// Tests that the manager will not execute JavaScript if the WebState has an
// opener.
TEST_F(TextFragmentsManagerImplTest, HasOpenerFragmentsDisallowed) {
  TextFragmentsManagerImpl* manager =
      CreateManager(/*has_opener=*/true,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/false,
                    /*feature_color_change=*/false,
                    /*add_web_frame=*/true);

  EXPECT_CALL(feature_, ProcessTextFragments(_, _, _, _)).Times(0);
  manager->DidFinishNavigation(web_state_, &context_);
}

// Tests that the manager will not execute JavaScript if the WebState has no
// user gesture.
TEST_F(TextFragmentsManagerImplTest, NoGestureFragmentsDisallowed) {
  TextFragmentsManagerImpl* manager =
      CreateManager(/*has_opener=*/false,
                    /*has_user_gesture=*/false,
                    /*is_same_document=*/false,
                    /*feature_color_change=*/false,
                    /*add_web_frame=*/true);

  EXPECT_CALL(feature_, ProcessTextFragments(_, _, _, _)).Times(0);
  manager->DidFinishNavigation(web_state_, &context_);
}

// Tests that the manager will not execute JavaScript if we navigated on the
// same document.
TEST_F(TextFragmentsManagerImplTest, SameDocumentFragmentsDisallowed) {
  TextFragmentsManagerImpl* manager =
      CreateManager(/*has_opener=*/false,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/true,
                    /*feature_color_change=*/false,
                    /*add_web_frame=*/true);

  EXPECT_CALL(feature_, ProcessTextFragments(_, _, _, _)).Times(0);
  manager->DidFinishNavigation(web_state_, &context_);
}

// Tests that the manager will not execute JavaScript if there are no
// fragments on the current URL.
TEST_F(TextFragmentsManagerImplTest, NoFragmentsNoJavaScript) {
  SetLastURL(GURL("https://www.chromium.org/"));

  TextFragmentsManagerImpl* manager =
      CreateManager(/*has_opener=*/false,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/false,
                    /*feature_color_change=*/false,
                    /*add_web_frame=*/true);

  EXPECT_CALL(feature_, ProcessTextFragments(_, _, _, _)).Times(0);
  manager->DidFinishNavigation(web_state_, &context_);
}

// Tests that the manager will not execute JavaScript if there are no
// text fragments on the current URL, even if it contains a fragment id.
TEST_F(TextFragmentsManagerImplTest, IdFragmentNoJavaScript) {
  SetLastURL(GURL("https://www.chromium.org/#fragmentId"));

  TextFragmentsManagerImpl* manager =
      CreateManager(/*has_opener=*/false,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/false,
                    /*feature_color_change=*/false,
                    /*add_web_frame=*/true);

  EXPECT_CALL(feature_, ProcessTextFragments(_, _, _, _)).Times(0);
  manager->DidFinishNavigation(web_state_, &context_);
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
TEST_F(TextFragmentsManagerImplTest, OnProcessingCompleteSuccessMetrics) {
  SetLastURL(GURL(kTwoFragmentsURL));
  TextFragmentsManagerImpl* manager = CreateDefaultManager();
  manager->DidFinishNavigation(web_state_, &context_);

  // 100% rate case.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    manager->OnProcessingComplete(2, 2);

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

    manager->OnProcessingComplete(3, 6);

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

    manager->OnProcessingComplete(0, 2);

    histogram_tester.ExpectUniqueSample("TextFragmentAnchor.AmbiguousMatch", 1,
                                        1);
    histogram_tester.ExpectUniqueSample("TextFragmentAnchor.MatchRate", 0, 1);

    ValidateLinkOpenedUkm(ukm_recorder, /*success=*/false,
                          TextFragmentLinkOpenSource::kSearchEngine);
  }
}

TEST_F(TextFragmentsManagerImplTest, ClickRemovesHighlights) {
  SetLastURL(GURL(kSingleFragmentURL));
  TextFragmentsManagerImpl* manager = CreateDefaultManager();
  GURL fragments_removed_gurl(kFragmentsRemovedURL);
  EXPECT_CALL(feature_, RemoveHighlights(_, fragments_removed_gurl));
  manager->OnClick();
}

}  // namespace web
