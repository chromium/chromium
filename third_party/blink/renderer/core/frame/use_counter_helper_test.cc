// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/css_property_id.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace {
const char kExtensionFeaturesHistogramName[] =
    "Blink.UseCounter.Extensions.Features";

const char kExtensionUrl[] = "chrome-extension://dummysite/";

int GetPageVisitsBucketforHistogram(const std::string& histogram_name) {
  if (histogram_name.find("CSS") == std::string::npos)
    return static_cast<int>(blink::mojom::WebFeature::kPageVisits);
  // For CSS histograms, the page visits bucket should be 1.
  return static_cast<int>(
      blink::mojom::blink::CSSSampleId::kTotalPagesMeasured);
}

}  // namespace

namespace blink {
using WebFeature = mojom::WebFeature;

class UseCounterHelperTest : public testing::Test {
 public:
  UseCounterHelperTest() : dummy_(std::make_unique<DummyPageHolder>()) {
    Page::InsertOrdinaryPageForTesting(&dummy_->GetPage());
  }

  int ToSampleId(CSSPropertyID property) const {
    return static_cast<int>(GetCSSSampleId(property));
  }

  bool IsInternal(CSSPropertyID property) const {
    return CSSProperty::Get(property).IsInternal();
  }

 protected:
  LocalFrame* GetFrame() { return &dummy_->GetFrame(); }
  void SetIsViewSource() { dummy_->GetDocument().SetIsViewSource(true); }
  void SetURL(const KURL& url) { dummy_->GetDocument().SetURL(url); }
  Document& GetDocument() { return dummy_->GetDocument(); }

  std::unique_ptr<DummyPageHolder> dummy_;
  HistogramTester histogram_tester_;

  void UpdateAllLifecyclePhases(Document& document) {
    document.View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
  }
};

TEST_F(UseCounterHelperTest, RecordingExtensions) {
  const std::string histogram = kExtensionFeaturesHistogramName;
  constexpr auto item = mojom::WebFeature::kFetch;
  constexpr auto second_item = WebFeature::kFetchBodyStream;
  const std::string url = kExtensionUrl;
  UseCounterHelper::Context context = UseCounterHelper::kExtensionContext;
  int page_visits_bucket = GetPageVisitsBucketforHistogram(histogram);

  UseCounterHelper use_counter0(context, UseCounterHelper::kCommited);

  // Test recording a single (arbitrary) counter
  EXPECT_FALSE(use_counter0.HasRecordedMeasurement(item));
  use_counter0.RecordMeasurement(item, *GetFrame());
  EXPECT_TRUE(use_counter0.HasRecordedMeasurement(item));
  histogram_tester_.ExpectUniqueSample(histogram, static_cast<int>(item), 1);
  // Test that repeated measurements have no effect
  use_counter0.RecordMeasurement(item, *GetFrame());
  histogram_tester_.ExpectUniqueSample(histogram, static_cast<int>(item), 1);

  // Test recording a different sample
  EXPECT_FALSE(use_counter0.HasRecordedMeasurement(second_item));
  use_counter0.RecordMeasurement(second_item, *GetFrame());
  EXPECT_TRUE(use_counter0.HasRecordedMeasurement(second_item));
  histogram_tester_.ExpectBucketCount(histogram, static_cast<int>(item), 1);
  histogram_tester_.ExpectBucketCount(histogram, static_cast<int>(second_item),
                                      1);
  histogram_tester_.ExpectTotalCount(histogram, 2);

  // After a page load, the histograms will be updated, even when the URL
  // scheme is internal
  UseCounterHelper use_counter1(context);
  SetURL(url_test_helpers::ToKURL(url));
  use_counter1.DidCommitLoad(GetFrame());
  histogram_tester_.ExpectBucketCount(histogram, static_cast<int>(item), 1);
  histogram_tester_.ExpectBucketCount(histogram, static_cast<int>(second_item),
                                      1);
  histogram_tester_.ExpectBucketCount(histogram, page_visits_bucket, 1);
  histogram_tester_.ExpectTotalCount(histogram, 3);

  // Now a repeat measurement should get recorded again, exactly once
  EXPECT_FALSE(use_counter1.HasRecordedMeasurement(item));
  use_counter1.RecordMeasurement(item, *GetFrame());
  use_counter1.RecordMeasurement(item, *GetFrame());
  EXPECT_TRUE(use_counter1.HasRecordedMeasurement(item));
  histogram_tester_.ExpectBucketCount(histogram, static_cast<int>(item), 2);
  histogram_tester_.ExpectTotalCount(histogram, 4);
}

TEST_F(UseCounterHelperTest, CSSSelectorPseudoWhere) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoWhere;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
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

TEST_F(UseCounterHelperTest, CSSSelectorPseudoAnyLink) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoAnyLink;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<style>:any-link { color: red; }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest, CSSSelectorPseudoWebkitAnyLink) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoWebkitAnyLink;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<style>:-webkit-any-link { color: red; }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest, CSSTypedOMStylePropertyMap) {
  UseCounterHelper use_counter;
  WebFeature feature = WebFeature::kCSSTypedOMStylePropertyMap;
  EXPECT_FALSE(GetDocument().IsUseCounted(feature));
  GetDocument().CountUse(feature);
  EXPECT_TRUE(GetDocument().IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest, CSSSelectorPseudoIs) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorPseudoIs;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<style>.a+:is(.b, .c+.d) { color: red; }</style>");
  EXPECT_TRUE(document.IsUseCounted(feature));
  EXPECT_FALSE(document.IsUseCounted(WebFeature::kCSSSelectorPseudoWhere));
}

TEST_F(UseCounterHelperTest, CSSContainLayoutNonPositionedDescendants) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSContainLayoutPositionedDescendants;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='contain: layout;'>"
      "</div>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest, CSSContainLayoutAbsolutelyPositionedDescendants) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSContainLayoutPositionedDescendants;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='contain: layout;'>"
      "  <div style='position: absolute;'></div>"
      "</div>");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest,
       CSSContainLayoutAbsolutelyPositionedDescendantsAlreadyContainingBlock) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSContainLayoutPositionedDescendants;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='position: relative; contain: layout;'>"
      "  <div style='position: absolute;'></div>"
      "</div>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest, CSSContainLayoutFixedPositionedDescendants) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSContainLayoutPositionedDescendants;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='contain: layout;'>"
      "  <div style='position: fixed;'></div>"
      "</div>");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest,
       CSSContainLayoutFixedPositionedDescendantsAlreadyContainingBlock) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSContainLayoutPositionedDescendants;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='transform: translateX(100px); contain: layout;'>"
      "  <div style='position: fixed;'></div>"
      "</div>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest, CSSGridLayoutPercentageColumnIndefiniteWidth) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kGridRowTrackPercentIndefiniteHeight;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='display: inline-grid; grid-template-columns: 50%;'>"
      "</div>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest, CSSGridLayoutPercentageRowIndefiniteHeight) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kGridRowTrackPercentIndefiniteHeight;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='display: inline-grid; grid-template-rows: 50%;'>"
      "</div>");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest, CSSFlexibleBox) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSFlexibleBox;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='display: flex;'>flexbox</div>");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest, CSSFlexibleBoxInline) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSFlexibleBox;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString(
      "<div style='display: inline-flex;'>flexbox</div>");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest, CSSFlexibleBoxButton) {
  // LayoutButton is a subclass of LayoutFlexibleBox, however we don't want it
  // to be counted as usage of flexboxes as it's an implementation detail.
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSFlexibleBox;
  EXPECT_FALSE(document.IsUseCounted(feature));
  document.documentElement()->SetInnerHTMLFromString("<button>button</button>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));
}

class DeprecationTest : public testing::Test {
 public:
  DeprecationTest()
      : dummy_(std::make_unique<DummyPageHolder>()),
        deprecation_(dummy_->GetPage().GetDeprecation()),
        use_counter_(dummy_->GetDocument().Loader()->GetUseCounterHelper()) {
    Page::InsertOrdinaryPageForTesting(&dummy_->GetPage());
  }

 protected:
  LocalFrame* GetFrame() { return &dummy_->GetFrame(); }

  std::unique_ptr<DummyPageHolder> dummy_;
  Deprecation& deprecation_;
  UseCounterHelper& use_counter_;
};

TEST_F(DeprecationTest, InspectorDisablesDeprecation) {
  // The specific feature we use here isn't important.
  WebFeature feature = WebFeature::kCSSDeepCombinator;
  CSSPropertyID property = CSSPropertyID::kFontWeight;

  EXPECT_FALSE(deprecation_.IsSuppressed(property));

  deprecation_.MuteForInspector();
  Deprecation::WarnOnDeprecatedProperties(GetFrame(), property);
  EXPECT_FALSE(deprecation_.IsSuppressed(property));
  Deprecation::CountDeprecation(dummy_->GetDocument(), feature);
  EXPECT_FALSE(use_counter_.HasRecordedMeasurement(feature));

  deprecation_.MuteForInspector();
  Deprecation::WarnOnDeprecatedProperties(GetFrame(), property);
  EXPECT_FALSE(deprecation_.IsSuppressed(property));
  Deprecation::CountDeprecation(dummy_->GetDocument(), feature);
  EXPECT_FALSE(use_counter_.HasRecordedMeasurement(feature));

  deprecation_.UnmuteForInspector();
  Deprecation::WarnOnDeprecatedProperties(GetFrame(), property);
  EXPECT_FALSE(deprecation_.IsSuppressed(property));
  Deprecation::CountDeprecation(dummy_->GetDocument(), feature);
  EXPECT_FALSE(use_counter_.HasRecordedMeasurement(feature));

  deprecation_.UnmuteForInspector();
  Deprecation::WarnOnDeprecatedProperties(GetFrame(), property);
  // TODO: use the actually deprecated property to get a deprecation message.
  EXPECT_FALSE(deprecation_.IsSuppressed(property));
  Deprecation::CountDeprecation(dummy_->GetDocument(), feature);
  EXPECT_TRUE(use_counter_.HasRecordedMeasurement(feature));
}

TEST_F(UseCounterHelperTest, CSSUnknownNamespacePrefixInSelector) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSUnknownNamespacePrefixInSelector;
  EXPECT_FALSE(document.IsUseCounted(feature));

  document.documentElement()->SetInnerHTMLFromString(R"HTML(
    <style>
      @namespace svg url(http://www.w3.org/2000/svg);
      svg|a {}
      a {}
    </style>
  )HTML");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));

  document.documentElement()->SetInnerHTMLFromString("<style>foo|a {}</style>");
  UpdateAllLifecyclePhases(document);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest, CSSSelectorHostContextInLiveProfile) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorHostContextInLiveProfile;

  document.body()->SetInnerHTMLFromString(R"HTML(
    <div id="parent">
      <div id="host"></div>
    </div>
  )HTML");

  Element* host = document.getElementById("host");
  ASSERT_TRUE(host);
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));

  shadow_root.SetInnerHTMLFromString(R"HTML(
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

TEST_F(UseCounterHelperTest, CSSSelectorHostContextInSnapshotProfile) {
  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Page::InsertOrdinaryPageForTesting(&dummy_page_holder->GetPage());
  Document& document = dummy_page_holder->GetDocument();
  WebFeature feature = WebFeature::kCSSSelectorHostContextInSnapshotProfile;

  document.body()->SetInnerHTMLFromString(R"HTML(
    <div id="parent">
      <div id="host"></div>
    </div>
  )HTML");

  Element* host = document.getElementById("host");
  ASSERT_TRUE(host);
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));

  shadow_root.SetInnerHTMLFromString("<span></span>");
  UpdateAllLifecyclePhases(document);
  EXPECT_FALSE(document.IsUseCounted(feature));

  Element* span = shadow_root.QuerySelector(":host-context(#parent) span");
  EXPECT_TRUE(span);
  EXPECT_TRUE(document.IsUseCounted(feature));
}

TEST_F(UseCounterHelperTest, UniqueCSSSampleIds) {
  HashSet<int> ids;

  for (CSSPropertyID property : CSSPropertyIDList()) {
    if (IsInternal(property))
      continue;
    EXPECT_FALSE(ids.Contains(ToSampleId(property)));
    ids.insert(ToSampleId(property));
  }

  for (CSSPropertyID property : kCSSPropertyAliasList) {
    EXPECT_FALSE(ids.Contains(ToSampleId(property)));
    ids.insert(ToSampleId(property));
  }
}

TEST_F(UseCounterHelperTest, MaximumCSSSampleId) {
  int max_sample_id = 0;

  for (CSSPropertyID property : CSSPropertyIDList()) {
    if (IsInternal(property))
      continue;
    max_sample_id = std::max(max_sample_id, ToSampleId(property));
  }

  for (CSSPropertyID property : kCSSPropertyAliasList)
    max_sample_id = std::max(max_sample_id, ToSampleId(property));

  EXPECT_EQ(static_cast<int>(mojom::blink::CSSSampleId::kMaxValue),
            max_sample_id);
}

}  // namespace blink
