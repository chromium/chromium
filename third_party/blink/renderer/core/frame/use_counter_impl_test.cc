// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/mojom/use_counter/metrics/css_property_id.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace {
const char kExtensionFeaturesHistogramName[] =
    "Blink.UseCounter.Extensions.Features";

const char kExtensionUrl[] = "chrome-extension://dummysite/";

int GetPageVisitsBucketforHistogram(const std::string& histogram_name) {
  if (histogram_name.find("CSS") == std::string::npos) {
    return static_cast<int>(blink::mojom::WebFeature::kPageVisits);
  }
  // For CSS histograms, the page visits bucket should be 1.
  return static_cast<int>(
      blink::mojom::blink::CSSSampleId::kTotalPagesMeasured);
}

}  // namespace

namespace blink {
using WebFeature = mojom::WebFeature;

class UseCounterImplTest : public testing::Test {
 public:
  class DummyLocalFrameClient : public EmptyLocalFrameClient {
   public:
    DummyLocalFrameClient() = default;
    const std::vector<UseCounterFeature>& observed_features() const {
      return observed_features_;
    }

   private:
    void DidObserveNewFeatureUsage(const UseCounterFeature& feature) override {
      observed_features_.push_back(feature);
    }
    std::vector<UseCounterFeature> observed_features_;
  };

  UseCounterImplTest()
      : dummy_(std::make_unique<DummyPageHolder>(
            /* initial_view_size= */ gfx::Size(),
            /* chrome_client= */ nullptr,
            /* local_frame_client= */
            MakeGarbageCollected<DummyLocalFrameClient>())) {
    Page::InsertOrdinaryPageForTesting(&dummy_->GetPage());
  }

  int ToSampleId(CSSPropertyID property) const {
    return static_cast<int>(GetCSSSampleId(property));
  }

  bool IsInternal(CSSPropertyID property) const {
    return CSSProperty::Get(property).IsInternal();
  }

  // Returns all alternative properties. In other words, a set of of all
  // properties marked with 'alternative_of' in css_properties.json5.
  //
  // This is useful for checking whether or not a given CSSPropertyID is an
  // an alternative property.
  HashSet<CSSPropertyID> GetAlternatives() const {
    HashSet<CSSPropertyID> alternatives;

    for (CSSPropertyID property : CSSPropertyIDList()) {
      if (CSSPropertyID alternative_id =
              CSSUnresolvedProperty::Get(property).GetAlternative();
          alternative_id != CSSPropertyID::kInvalid) {
        alternatives.insert(alternative_id);
      }
    }

    for (CSSPropertyID property : kCSSPropertyAliasList) {
      if (CSSPropertyID alternative_id =
              CSSUnresolvedProperty::Get(property).GetAlternative();
          alternative_id != CSSPropertyID::kInvalid) {
        alternatives.insert(alternative_id);
      }
    }

    return alternatives;
  }

 protected:
  LocalFrame* GetFrame() { return &dummy_->GetFrame(); }
  void SetIsViewSource() { dummy_->GetDocument().SetIsViewSource(true); }
  void SetURL(const KURL& url) { dummy_->GetDocument().SetURL(url); }
  Document& GetDocument() { return dummy_->GetDocument(); }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_;
  base::HistogramTester histogram_tester_;

  void UpdateAllLifecyclePhases(Document& document) {
    document.View()->UpdateAllLifecyclePhasesForTest();
  }
};

class UseCounterImplBrowserReportTest
    : public UseCounterImplTest,
      public ::testing::WithParamInterface</* URL */ const char*> {};

INSTANTIATE_TEST_SUITE_P(All,
                         UseCounterImplBrowserReportTest,
                         ::testing::Values("chrome-extension://dummysite/",
                                           "file://dummyfile",
                                           "data:;base64,",
                                           "ftp://ftp.dummy/dummy.txt",
                                           "http://foo.com",
                                           "https://bar.com"));

// UseCounter should not send events to browser when handling page with
// Non HTTP Family URLs, as these events will be discarded on the browser side
// in |MetricsWebContentsObserver::DoesTimingUpdateHaveError|.
TEST_P(UseCounterImplBrowserReportTest, ReportOnlyHTTPFamily) {
  KURL url = url_test_helpers::ToKURL(GetParam());
  SetURL(url);
  UseCounterImpl use_counter;
  use_counter.DidCommitLoad(GetFrame());

  // Count every feature types in UseCounterFeatureType.
  use_counter.Count(mojom::WebFeature::kFetch, GetFrame());
  use_counter.Count(CSSPropertyID::kHeight,
                    UseCounterImpl::CSSPropertyType::kDefault, GetFrame());
  use_counter.Count(CSSPropertyID::kHeight,
                    UseCounterImpl::CSSPropertyType::kAnimation, GetFrame());

  auto* dummy_client =
      static_cast<UseCounterImplBrowserReportTest::DummyLocalFrameClient*>(
          GetFrame()->Client());

  EXPECT_EQ(!dummy_client->observed_features().empty(),
            url.ProtocolIsInHTTPFamily());
}

TEST_F(UseCounterImplTest, RecordingExtensions) {
  const std::string histogram = kExtensionFeaturesHistogramName;
  constexpr auto item = mojom::WebFeature::kFetch;
  constexpr auto second_item = WebFeature::kFetchBodyStream;
  const std::string url = kExtensionUrl;
  CommonSchemeRegistry::RegisterURLSchemeAsExtension("chrome-extension");
  UseCounterImpl::Context context = UseCounterImpl::kExtensionContext;
  int page_visits_bucket = GetPageVisitsBucketforHistogram(histogram);

  UseCounterImpl use_counter0(context, UseCounterImpl::kCommited);

  // Test recording a single (arbitrary) counter
  EXPECT_FALSE(use_counter0.IsCounted(item));
  use_counter0.Count(item, GetFrame());
  EXPECT_TRUE(use_counter0.IsCounted(item));
  histogram_tester_.ExpectUniqueSample(histogram, static_cast<int>(item), 1);
  // Test that repeated measurements have no effect
  use_counter0.Count(item, GetFrame());
  histogram_tester_.ExpectUniqueSample(histogram, static_cast<int>(item), 1);

  // Test recording a different sample
  EXPECT_FALSE(use_counter0.IsCounted(second_item));
  use_counter0.Count(second_item, GetFrame());
  EXPECT_TRUE(use_counter0.IsCounted(second_item));
  histogram_tester_.ExpectBucketCount(histogram, static_cast<int>(item), 1);
  histogram_tester_.ExpectBucketCount(histogram, static_cast<int>(second_item),
                                      1);
  histogram_tester_.ExpectTotalCount(histogram, 2);

  // After a page load, the histograms will be updated, even when the URL
  // scheme is internal
  UseCounterImpl use_counter1(context);
  SetURL(url_test_helpers::ToKURL(url));
  use_counter1.DidCommitLoad(GetFrame());
  histogram_tester_.ExpectBucketCount(histogram, static_cast<int>(item), 1);
  histogram_tester_.ExpectBucketCount(histogram, static_cast<int>(second_item),
                                      1);
  histogram_tester_.ExpectBucketCount(histogram, page_visits_bucket, 1);
  histogram_tester_.ExpectTotalCount(histogram, 3);

  // Now a repeat measurement should get recorded again, exactly once
  EXPECT_FALSE(use_counter1.IsCounted(item));
  use_counter1.Count(item, GetFrame());
  use_counter1.Count(item, GetFrame());
  EXPECT_TRUE(use_counter1.IsCounted(item));
  histogram_tester_.ExpectBucketCount(histogram, static_cast<int>(item), 2);
  histogram_tester_.ExpectTotalCount(histogram, 4);
  CommonSchemeRegistry::RemoveURLSchemeAsExtensionForTest("chrome-extension");
}

TEST_F(UseCounterImplTest, CSSSelectorPseudoWhere) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoWhere;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>.a+:where(.b, .c+.d) { color: red; }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSSelectorPseudoIs));
}

/*
 * Counter-specific tests
 *
 * NOTE: Most individual UseCounters don't need dedicated test cases.  They are
 * "tested" by analyzing the data they generate including on some known pages.
 * Feel free to add tests for counters where the triggering logic is
 * non-trivial, but it's not required. Manual analysis is necessary to trust the
 * data anyway, real-world pages are full of edge-cases and surprises that you
 * won't find in unit testing anyway.
 */

TEST_F(UseCounterImplTest, CSSSelectorPseudoAnyLink) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoAnyLink;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>:any-link { color: red; }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, CSSSelectorPseudoWebkitAnyLink) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoWebkitAnyLink;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>:-webkit-any-link { color: red; }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, CSSTypedOMStylePropertyMap) {
  UseCounterImpl use_counter;
  WebFeature feature = WebFeature::kCSSTypedOMStylePropertyMap;
  EXPECT_FALSE(GetDocument().IsUseCounted(feature));
  GetDocument().CountUse(feature);
  EXPECT_TRUE(GetDocument().IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, CSSSelectorPseudoIs) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoIs;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>.a+:is(.b, .c+.d) { color: red; }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSSelectorPseudoWhere));
}

TEST_F(UseCounterImplTest, CSSSelectorPseudoDir) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoDir;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>:dir(ltr) { color: red; }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, CSSSelectorNthChildOfSelector) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorNthChildOfSelector;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>.a:nth-child(3) { color: red; }</style>");
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<style>.a:nth-child(3 of .b) { color: red; }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, CSSGridLayoutPercentageColumnIndefiniteWidth) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kGridRowTrackPercentIndefiniteHeight;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<div style='display: inline-grid; grid-template-columns: 50%;'>"
      "</div>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, CSSFlexibleBox) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSFlexibleBox;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<div style='display: flex;'>flexbox</div>");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, CSSFlexibleBoxInline) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSFlexibleBox;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML(
      "<div style='display: inline-flex;'>flexbox</div>");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, CSSFlexibleBoxButton) {
  // LayoutButton is a subclass of LayoutFlexibleBox, however we don't want
  // it to be counted as usage of flexboxes as it's an implementation detail.
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSFlexibleBox;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->setInnerHTML("<button>button</button>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, HTMLRootContained) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kHTMLRootContained;
  EXPECT_FALSE(document.IsUseCounted(feature));

  document.documentElement()->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                                     "none");
  document.documentElement()->SetInlineStyleProperty(CSSPropertyID::kContain,
                                                     "paint");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));

  document.documentElement()->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                                     "block");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, HTMLBodyContained) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kHTMLBodyContained;
  EXPECT_FALSE(document.IsUseCounted(feature));

  document.body()->SetInlineStyleProperty(CSSPropertyID::kDisplay, "none");
  document.body()->SetInlineStyleProperty(CSSPropertyID::kContain, "paint");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));

  document.body()->SetInlineStyleProperty(CSSPropertyID::kDisplay, "block");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

class DeprecationTest : public testing::Test {
 public:
  DeprecationTest()
      : dummy_(std::make_unique<DummyPageHolder>()),
        deprecation_(dummy_->GetPage().GetDeprecation()),
        use_counter_(dummy_->GetDocument().Loader()->GetUseCounter()) {
    Page::InsertOrdinaryPageForTesting(&dummy_->GetPage());
  }

 protected:
  LocalFrame* GetFrame() { return &dummy_->GetFrame(); }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_;
  Deprecation& deprecation_;
  UseCounterImpl& use_counter_;
};

TEST_F(DeprecationTest, InspectorDisablesDeprecation) {
  // The specific feature we use here isn't important.
  WebFeature feature =
      WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton;

  deprecation_.MuteForInspector();
  Deprecation::CountDeprecation(GetFrame()->DomWindow(), feature);
  EXPECT_FALSE(use_counter_.IsCounted(feature));

  deprecation_.MuteForInspector();
  Deprecation::CountDeprecation(GetFrame()->DomWindow(), feature);
  EXPECT_FALSE(use_counter_.IsCounted(feature));

  deprecation_.UnmuteForInspector();
  Deprecation::CountDeprecation(GetFrame()->DomWindow(), feature);
  EXPECT_FALSE(use_counter_.IsCounted(feature));

  deprecation_.UnmuteForInspector();
  Deprecation::CountDeprecation(GetFrame()->DomWindow(), feature);
  EXPECT_TRUE(use_counter_.IsCounted(feature));
}

TEST_F(UseCounterImplTest, CSSUnknownNamespacePrefixInSelector) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSUnknownNamespacePrefixInSelector;
  EXPECT_FALSE(document.IsUseCounted(feature));

  document.documentElement()->setInnerHTML(R"HTML(
    <style>
      @namespace svg url(http://www.w3.org/2000/svg);
      svg|a {}
      a {}
    </style>
  )HTML");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));

  document.documentElement()->setInnerHTML("<style>foo|a {}</style>");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, CSSSelectorHostContextInLiveProfile) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorHostContextInLiveProfile;

  document.body()->setInnerHTML(R"HTML(
    <div id="parent">
      <div id="host"></div>
    </div>
  )HTML");

  Element* host = document.getElementById(AtomicString("host"));
  ASSERT_TRUE(host);
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));

  shadow_root.setInnerHTML(R"HTML(
      <style>
        :host-context(#parent) span {
          color: green
        }
      </style>
      <span></span>
  )HTML");

  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, CSSSelectorHostContextInSnapshotProfile) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorHostContextInSnapshotProfile;

  document.body()->setInnerHTML(R"HTML(
    <div id="parent">
      <div id="host"></div>
    </div>
  )HTML");

  Element* host = document.getElementById(AtomicString("host"));
  ASSERT_TRUE(host);
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));

  shadow_root.setInnerHTML("<span></span>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));

  Element* span =
      shadow_root.QuerySelector(AtomicString(":host-context(#parent) span"));
  EXPECT_TRUE(span);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, UniqueCSSSampleIds) {
  HashSet<int> ids;

  HashSet<CSSPropertyID> alternatives = GetAlternatives();

  for (CSSPropertyID property : CSSPropertyIDList()) {
    if (IsInternal(property)) {
      continue;
    }
    if (alternatives.Contains(property)) {
      // Alternative properties should use the same CSSSampleId as the
      // corresponding main property.
      continue;
    }
    EXPECT_FALSE(ids.Contains(ToSampleId(property)));
    ids.insert(ToSampleId(property));
  }

  for (CSSPropertyID property : kCSSPropertyAliasList) {
    if (alternatives.Contains(property)) {
      // Alternative properties should use the same CSSSampleId as the
      // corresponding main property.
      continue;
    }
    EXPECT_FALSE(ids.Contains(ToSampleId(property)));
    ids.insert(ToSampleId(property));
  }
}

TEST_F(UseCounterImplTest, MaximumCSSSampleId) {
  int max_sample_id = 0;

  for (CSSPropertyID property : CSSPropertyIDList()) {
    if (IsInternal(property)) {
      continue;
    }
    max_sample_id = std::max(max_sample_id, ToSampleId(property));
  }

  for (CSSPropertyID property : kCSSPropertyAliasList) {
    max_sample_id = std::max(max_sample_id, ToSampleId(property));
  }

  EXPECT_EQ(static_cast<int>(mojom::blink::CSSSampleId::kMaxValue),
            max_sample_id);
}

TEST_F(UseCounterImplTest, CSSMarkerPseudoElementUA) {
  // Check that UA styles for list markers are not counted.
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kHasMarkerPseudoElement;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.body()->setInnerHTML(R"HTML(
    <style>
      li::before {
        content: "[before]";
        display: list-item;
      }
    </style>
    <ul>
      <li style="list-style: decimal outside"></li>
      <li style="list-style: decimal inside"></li>
      <li style="list-style: disc outside"></li>
      <li style="list-style: disc inside"></li>
      <li style="list-style: '- ' outside"></li>
      <li style="list-style: '- ' inside"></li>
      <li style="list-style: linear-gradient(blue, cyan) outside"></li>
      <li style="list-style: linear-gradient(blue, cyan) inside"></li>
      <li style="list-style: none outside"></li>
      <li style="list-style: none inside"></li>
    </ul>
  )HTML");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, CSSMarkerPseudoElementAuthor) {
  // Check that author styles for list markers are counted.
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kHasMarkerPseudoElement;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.body()->setInnerHTML(R"HTML(
    <style>
      li::marker {
        color: blue;
      }
    </style>
    <ul>
      <li></li>
    </ul>
  )HTML");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterImplTest, BackgroundClip) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();

  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipBorder));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipContent));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipPadding));

  document.documentElement()->setInnerHTML(
      "<style>html{background-clip: border-box;}</style>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipBorder));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipContent));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipPadding));

  document.documentElement()->setInnerHTML(
      "<style>html{background-clip: content-box;}</style>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipBorder));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipContent));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipPadding));

  document.documentElement()->setInnerHTML(
      "<style>html{background-clip: padding-box;}</style>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipBorder));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipContent));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipPadding));

  document.documentElement()->setInnerHTML(
      "<style>html{-webkit-background-clip: border-box;}</style>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipBorder));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipContent));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipPadding));

  document.documentElement()->setInnerHTML(
      "<style>html{-webkit-background-clip: content-box;}</style>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipBorder));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipContent));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipPadding));

  document.documentElement()->setInnerHTML(
      "<style>html{-webkit-background-clip: padding-box;}</style>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipBorder));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipContent));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipPadding));

  document.documentElement()->setInnerHTML(
      "<style>html{-webkit-background-clip: text;}</style>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipBorder));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipContent));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipPadding));

  // We dropped the support for keywords without suffix.
  document.documentElement()->setInnerHTML(
      "<style>html{-webkit-background-clip: border;}</style>");
  UpdateAllLifecyclePhases(document);
  if (RuntimeEnabledFeatures::CSSBackgroundClipUnprefixEnabled()) {
    EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipBorder));
  } else {
    EXPECT_TRUE(document.IsUseCounted(WebFeature::kCSSBackgroundClipBorder));
  }
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipContent));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipPadding));

  document.ClearUseCounterForTesting(WebFeature::kCSSBackgroundClipBorder);
  document.documentElement()->setInnerHTML(
      "<style>html{-webkit-background-clip: content;}</style>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipBorder));
  if (RuntimeEnabledFeatures::CSSBackgroundClipUnprefixEnabled()) {
    EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipContent));
  } else {
    EXPECT_TRUE(document.IsUseCounted(WebFeature::kCSSBackgroundClipContent));
  }
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipPadding));

  document.ClearUseCounterForTesting(WebFeature::kCSSBackgroundClipContent);
  document.documentElement()->setInnerHTML(
      "<style>html{-webkit-background-clip: padding;}</style>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipBorder));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipContent));
  if (RuntimeEnabledFeatures::CSSBackgroundClipUnprefixEnabled()) {
    EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSBackgroundClipPadding));
  } else {
    EXPECT_TRUE(document.IsUseCounted(WebFeature::kCSSBackgroundClipPadding));
  }
}

TEST_F(UseCounterImplTest, H1UserAgentFontSizeInSectionApplied) {
  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kH1UserAgentFontSizeInSectionApplied;

  EXPECT_FALSE(document.IsUseCounted(feature));

  document.documentElement()->setInnerHTML("<h1></h1>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature))
      << "Not inside sectioning element";

  document.documentElement()->setInnerHTML(R"HTML(
      <article><h1 style="font-size: 10px"></h1></article>
  )HTML");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature))
      << "Inside sectioning element with author font-size";

  document.documentElement()->setInnerHTML(R"HTML(
      <article><h1></h1></article>
  )HTML");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature))
      << "Inside sectioning element with UA font-size";
}

}  // namespace blink
