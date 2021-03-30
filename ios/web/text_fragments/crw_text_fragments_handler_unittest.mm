// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/text_fragments/crw_text_fragments_handler.h"

#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "components/shared_highlighting/core/common/text_fragments_constants.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/web_state/ui/crw_web_view_handler_delegate.h"
#import "ios/web/web_state/web_state_impl.h"
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

class MockWebStateImpl : public web::WebStateImpl {
 public:
  explicit MockWebStateImpl(web::WebState::CreateParams params)
      : web::WebStateImpl(params) {}

  MOCK_METHOD1(ExecuteJavaScript, void(const std::u16string&));
  MOCK_CONST_METHOD0(GetLastCommittedURL, const GURL&());

  base::CallbackListSubscription AddScriptCommandCallback(
      const web::WebState::ScriptCommandCallback& callback,
      const std::string& command_prefix) override {
    last_callback_ = callback;
    last_command_prefix_ = command_prefix;
    return {};
  }

  web::WebState::ScriptCommandCallback last_callback() {
    return last_callback_;
  }
  const std::string last_command_prefix() { return last_command_prefix_; }

 private:
  web::WebState::ScriptCommandCallback last_callback_;
  std::string last_command_prefix_;
};

class CRWTextFragmentsHandlerTest : public web::WebTest {
 protected:
  CRWTextFragmentsHandlerTest() : context_(), feature_list_() {}

  void SetUp() override {
    web::WebState::CreateParams params(GetBrowserState());
    std::unique_ptr<MockWebStateImpl> web_state =
        std::make_unique<MockWebStateImpl>(params);
    web_state_ = web_state.get();
    context_.SetWebState(std::move(web_state));

    mocked_delegate_ =
        OCMStrictProtocolMock(@protocol(CRWWebViewHandlerDelegate));
    OCMStub([mocked_delegate_ webStateImplForWebViewHandler:[OCMArg any]])
        .andReturn((web::WebStateImpl*)web_state_);
  }

  CRWTextFragmentsHandler* CreateDefaultHandler() {
    return CreateHandler(/*has_opener=*/false,
                         /*has_user_gesture=*/true,
                         /*is_same_document=*/false,
                         /*feature_enabled=*/true,
                         /*feature_color_change=*/false);
  }

  CRWTextFragmentsHandler* CreateHandler(bool has_opener,
                                         bool has_user_gesture,
                                         bool is_same_document,
                                         bool feature_enabled,
                                         bool feature_color_change) {
    if (feature_enabled && feature_color_change) {
      feature_list_.InitWithFeatures(
          {web::features::kScrollToTextIOS,
           web::features::kIOSSharedHighlightingColorChange},
          {});
    } else if (feature_enabled) {
      feature_list_.InitAndEnableFeature(web::features::kScrollToTextIOS);
    } else {
      feature_list_.InitAndDisableFeature(web::features::kScrollToTextIOS);
    }
    web_state_->SetHasOpener(has_opener);
    context_.SetHasUserGesture(has_user_gesture);
    context_.SetIsSameDocument(is_same_document);

    return [[CRWTextFragmentsHandler alloc] initWithDelegate:mocked_delegate_];
  }

  void SetLastURL(const GURL& last_url) {
    EXPECT_CALL(*web_state_, GetLastCommittedURL())
        .WillOnce(ReturnRefOfCopy(last_url));
  }

  Referrer GetSearchEngineReferrer() {
    return Referrer(GURL(kSearchEngineURL), web::ReferrerPolicyDefault);
  }

  Referrer GetNonSearchEngineReferrer() {
    return Referrer(GURL(kNonSearchEngineURL), web::ReferrerPolicyDefault);
  }

  void CreateHandlerAndProcessTextFragments() {
    CRWTextFragmentsHandler* handler = CreateDefaultHandler();

    [handler processTextFragmentsWithContext:&context_
                                    referrer:GetSearchEngineReferrer()];
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
  MockWebStateImpl* web_state_;
  base::test::ScopedFeatureList feature_list_;
  id<CRWWebViewHandlerDelegate> mocked_delegate_;
};

// Tests that the handler will execute JavaScript if highlighting is allowed and
// fragments are present.
TEST_F(CRWTextFragmentsHandlerTest, ExecuteJavaScriptSuccess) {
  base::HistogramTester histogram_tester;
  SetLastURL(GURL(kValidFragmentsURL));

  CRWTextFragmentsHandler* handler = CreateDefaultHandler();

  // Set up expectation.
  std::u16string expected_javascript =
      base::UTF8ToUTF16(kScriptForValidFragmentsURL);
  EXPECT_CALL(*web_state_, ExecuteJavaScript(expected_javascript)).Times(1);

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];

  // Verify that a command callback was added with the right prefix.
  EXPECT_NE(web::WebState::ScriptCommandCallback(),
            web_state_->last_callback());
  EXPECT_EQ("textFragments", web_state_->last_command_prefix());
}

// Tests that the handler will execute JavaScript with the default colors
// if the IOSSharedHighlightingColorChange flag is enabled, if highlighting
// is allowed and fragments are present.
TEST_F(CRWTextFragmentsHandlerTest, ExecuteJavaScriptWithColorChange) {
  base::HistogramTester histogram_tester;
  SetLastURL(GURL(kValidFragmentsURL));

  CRWTextFragmentsHandler* handler =
      CreateHandler(/*has_opener=*/false,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/false,
                    /*feature_enabled=*/true,
                    /*feature_color_change=*/true);

  // Set up expectation.
  std::u16string expected_javascript =
      base::UTF8ToUTF16(kScriptForValidFragmentsColorChangeURL);
  EXPECT_CALL(*web_state_, ExecuteJavaScript(expected_javascript)).Times(1);

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];

  // Verify that a command callback was added with the right prefix.
  EXPECT_NE(web::WebState::ScriptCommandCallback(),
            web_state_->last_callback());
  EXPECT_EQ("textFragments", web_state_->last_command_prefix());
}

// Tests that the handler will not execute JavaScript if the scroll to text
// feature is disabled.
TEST_F(CRWTextFragmentsHandlerTest, FeatureDisabledFragmentsDisallowed) {
  CRWTextFragmentsHandler* handler =
      CreateHandler(/*has_opener=*/false,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/false,
                    /*feature_enabled=*/false,
                    /*feature_color_change=*/false);

  EXPECT_CALL(*web_state_, ExecuteJavaScript(_)).Times(0);
  EXPECT_CALL(*web_state_, GetLastCommittedURL()).Times(0);

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];

  // Verify that no callback was set when the flag is disabled.
  EXPECT_EQ(web::WebState::ScriptCommandCallback(),
            web_state_->last_callback());
}

// Tests that the handler will not execute JavaScript if the WebState has an
// opener.
TEST_F(CRWTextFragmentsHandlerTest, HasOpenerFragmentsDisallowed) {
  CRWTextFragmentsHandler* handler =
      CreateHandler(/*has_opener=*/true,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/false,
                    /*feature_enabled=*/true,
                    /*feature_color_change=*/false);

  EXPECT_CALL(*web_state_, ExecuteJavaScript(_)).Times(0);
  EXPECT_CALL(*web_state_, GetLastCommittedURL()).Times(0);

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];
}

// Tests that the handler will not execute JavaScript if the WebState has no
// user gesture.
TEST_F(CRWTextFragmentsHandlerTest, NoGestureFragmentsDisallowed) {
  CRWTextFragmentsHandler* handler =
      CreateHandler(/*has_opener=*/false,
                    /*has_user_gesture=*/false,
                    /*is_same_document=*/false,
                    /*feature_enabled=*/true,
                    /*feature_color_change=*/false);

  EXPECT_CALL(*web_state_, ExecuteJavaScript(_)).Times(0);
  EXPECT_CALL(*web_state_, GetLastCommittedURL()).Times(0);

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];
}

// Tests that the handler will not execute JavaScript if we navigated on the
// same document.
TEST_F(CRWTextFragmentsHandlerTest, SameDocumentFragmentsDisallowed) {
  CRWTextFragmentsHandler* handler =
      CreateHandler(/*has_opener=*/false,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/true,
                    /*feature_enabled=*/true,
                    /*feature_color_change=*/false);

  EXPECT_CALL(*web_state_, ExecuteJavaScript(_)).Times(0);
  EXPECT_CALL(*web_state_, GetLastCommittedURL()).Times(0);

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];
}

// Tests that the handler will not execute JavaScript if there are no
// fragments on the current URL.
TEST_F(CRWTextFragmentsHandlerTest, NoFragmentsNoJavaScript) {
  SetLastURL(GURL("https://www.chromium.org/"));

  CRWTextFragmentsHandler* handler =
      CreateHandler(/*has_opener=*/false,
                    /*has_user_gesture=*/true,
                    /*is_same_document=*/false,
                    /*feature_enabled=*/true,
                    /*feature_color_change=*/false);

  EXPECT_CALL(*web_state_, ExecuteJavaScript(_)).Times(0);

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];
}

// Tests that any timing issue which would call the handle after it got closed
// would not crash the app.
TEST_F(CRWTextFragmentsHandlerTest, PostCloseInvokeDoesNotCrash) {
  // Reset the mock.
  mocked_delegate_ =
      OCMStrictProtocolMock(@protocol(CRWWebViewHandlerDelegate));
  OCMStub([mocked_delegate_ webStateImplForWebViewHandler:[OCMArg any]])
      .andReturn((web::WebStateImpl*)nullptr);

  CRWTextFragmentsHandler* handler = CreateDefaultHandler();

  [handler close];

  EXPECT_CALL(*web_state_, ExecuteJavaScript(_)).Times(0);
  EXPECT_CALL(*web_state_, GetLastCommittedURL()).Times(0);

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];
}

// Tests that no metrics are recoded for an URL that doesn't contain text
// fragments.
TEST_F(CRWTextFragmentsHandlerTest, NoMetricsRecordedIfNoFragmentPresent) {
  base::HistogramTester histogram_tester;

  // Set a URL without text fragments.
  SetLastURL(GURL("https://www.chromium.org/"));

  CRWTextFragmentsHandler* handler = CreateDefaultHandler();

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];

  // Make sure no metrics were logged.
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 0);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 0);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.MatchRate", 0);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 0);
}

// Tests that no metrics are recoded for an URL that doesn't contain text
// fragments, even if it contains a fragment id
TEST_F(CRWTextFragmentsHandlerTest,
       NoMetricsRecordedIfNoFragmentPresentWithFragmentId) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Set a URL without text fragments, but with an id fragment.
  SetLastURL(GURL("https://www.chromium.org/#FragmentID"));

  CRWTextFragmentsHandler* handler = CreateDefaultHandler();

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];

  // Make sure no metrics were logged.
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 0);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 0);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.MatchRate", 0);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 0);
}

// Tests that the LinkSource metric is recorded properly when the link comes
// from a search engine.
TEST_F(CRWTextFragmentsHandlerTest, LinkSourceMetricSearchEngine) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SetLastURL(GURL(kValidFragmentsURL));

  CRWTextFragmentsHandler* handler = CreateDefaultHandler();

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];

  histogram_tester.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 1,
                                      1);
}

// Tests that the LinkSource metric is recorded properly when the link doesn't
// come from a search engine.
TEST_F(CRWTextFragmentsHandlerTest, LinkSourceMetricNonSearchEngine) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  SetLastURL(GURL(kValidFragmentsURL));

  CRWTextFragmentsHandler* handler = CreateDefaultHandler();

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetNonSearchEngineReferrer()];

  histogram_tester.ExpectUniqueSample("TextFragmentAnchor.LinkOpenSource", 0,
                                      1);
}

// Tests that the SelectorCount metric is recorded properly when a single
// selector is present.
TEST_F(CRWTextFragmentsHandlerTest, SelectorCountMetricSingleSelector) {
  base::HistogramTester histogram_tester;
  SetLastURL(GURL(kSingleFragmentURL));

  CRWTextFragmentsHandler* handler = CreateDefaultHandler();

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];

  histogram_tester.ExpectUniqueSample("TextFragmentAnchor.SelectorCount", 1, 1);
}

// Tests that the SelectorCount metric is recorded properly when two selectors
// are present.
TEST_F(CRWTextFragmentsHandlerTest, SelectorCountMetricTwoSelectors) {
  base::HistogramTester histogram_tester;
  SetLastURL(GURL(kTwoFragmentsURL));

  CRWTextFragmentsHandler* handler = CreateDefaultHandler();

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];

  histogram_tester.ExpectUniqueSample("TextFragmentAnchor.SelectorCount", 2, 1);
}

// Tests that the AmbiguousMatch and MatchRate success metrics are recorded
// properly in a variety of cases.
TEST_F(CRWTextFragmentsHandlerTest,
       DidReceiveJavaScriptResponseSuccessMetrics) {
  SetLastURL(GURL(kTwoFragmentsURL));
  CRWTextFragmentsHandler* handler = CreateDefaultHandler();

  [handler processTextFragmentsWithContext:&context_
                                  referrer:GetSearchEngineReferrer()];

  web::WebState::ScriptCommandCallback parse_function =
      web_state_->last_callback();
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
