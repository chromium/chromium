// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/skeleton/skeleton_loader.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class SkeletonLoaderSimTest : public SimTest,
                              public ScopedDeclarativeSkeletonsForTest {
 public:
  SkeletonLoaderSimTest() : ScopedDeclarativeSkeletonsForTest(true) {}

 protected:
  void SetUp() override {
    SimTest::SetUp();

    // Start with a basic page load so we have a valid document structure.
    SimRequest main("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    main.Complete("<html><body></body></html>");
    Compositor().BeginFrame();
  }
};

class SkeletonLoaderTest : public PageTestBase,
                           public ScopedDeclarativeSkeletonsForTest {
 public:
  SkeletonLoaderTest() : ScopedDeclarativeSkeletonsForTest(true) {}

 protected:
  void InsertSkeletonTree(const String& source) {
    ScopedNullExecutionContext execution_context;
    HTMLDocument* skeleton_document =
        HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
    skeleton_document->SetContent(source);
    SkeletonLoader::Ensure(GetDocument())
        .InsertSkeletonTree(*skeleton_document);
  }
  void RemoveSkeletonTree() {
    SkeletonLoader::Ensure(GetDocument()).RemoveSkeletonTree();
  }
};

TEST_F(SkeletonLoaderSimTest, Basic) {
  KURL next_page_url("https://example.com/page.html");
  KURL skeleton_url("https://example.com/skeleton.html");

  SimRequest::Params head_params;
  head_params.response_http_headers = {
      {http_names::kLink, "<https://example.com/skeleton.html>; rel=skeleton"}};
  SimRequest next_page_head_request(next_page_url, "text/html", head_params);
  SimRequest skeleton_request(skeleton_url, "text/html");

  Document& document = GetDocument();
  SkeletonLoader& loader = SkeletonLoader::Ensure(document);

  loader.AddSkeletonPrefetchLink(next_page_url);

  // Check that the Document does not initially have a ::skeleton pseudo-element
  Element* root = document.documentElement();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);

  next_page_head_request.Complete("");
  skeleton_request.Complete("<div>Skeleton</div>");
  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);

  loader.NavigateTo(next_page_url);
  Compositor().BeginFrame();

  // Check that the Document now renders the skeleton
  PseudoElement* skeleton_pseudo = root->GetPseudoElement(kPseudoIdSkeleton);
  ASSERT_TRUE(skeleton_pseudo);
  ShadowRoot* shadow_root = skeleton_pseudo->GetShadowRoot();
  ASSERT_TRUE(shadow_root);
  EXPECT_EQ(shadow_root->innerHTML(),
            "<html><head></head><body><div>Skeleton</div></body></html>");

  // Call CancelNavigation() to remove the skeleton
  loader.CancelNavigation();
  Compositor().BeginFrame();

  // The Document should no longer have a ::skeleton pseudo-element
  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);
}

TEST_F(SkeletonLoaderSimTest, SkeletonLoadsAfterNavigation) {
  KURL next_page_url("https://example.com/page.html");
  KURL skeleton_url("https://example.com/skeleton.html");

  SimRequest::Params head_params;
  head_params.response_http_headers = {
      {http_names::kLink, "<https://example.com/skeleton.html>; rel=skeleton"}};
  SimRequest next_page_head_request(next_page_url, "text/html", head_params);
  SimRequest skeleton_request(skeleton_url, "text/html");

  Document& document = GetDocument();
  SkeletonLoader& loader = SkeletonLoader::Ensure(document);

  loader.AddSkeletonPrefetchLink(next_page_url);

  // Check that the Document does not initially have a ::skeleton pseudo-element
  Element* root = document.documentElement();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);

  loader.NavigateTo(next_page_url);

  // Navigation triggered, but skeleton still loading
  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);

  next_page_head_request.Complete("");
  skeleton_request.Complete("<div>Skeleton</div>");
  Compositor().BeginFrame();

  // Check that the Document now renders the skeleton
  PseudoElement* skeleton_pseudo = root->GetPseudoElement(kPseudoIdSkeleton);
  ASSERT_TRUE(skeleton_pseudo);
  ShadowRoot* shadow_root = skeleton_pseudo->GetShadowRoot();
  ASSERT_TRUE(shadow_root);
  EXPECT_EQ(shadow_root->innerHTML(),
            "<html><head></head><body><div>Skeleton</div></body></html>");
}

TEST_F(SkeletonLoaderSimTest, NoSkeletonLink) {
  KURL next_page_url("https://example.com/page.html");
  SimRequest next_page_head_request(next_page_url, "text/html");

  Document& document = GetDocument();
  SkeletonLoader& loader = SkeletonLoader::Ensure(document);

  loader.AddSkeletonPrefetchLink(next_page_url);

  // Check that the Document does not initially have a ::skeleton pseudo-element
  Element* root = document.documentElement();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);

  next_page_head_request.Complete("");

  loader.NavigateTo(next_page_url);

  // Navigation triggered, no Link header
  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);
}

TEST_F(SkeletonLoaderSimTest, SkeletonLinkNotFound) {
  KURL next_page_url("https://example.com/page.html");
  KURL skeleton_url("https://example.com/skeleton.html");

  SimRequest::Params head_params;
  head_params.response_http_headers = {
      {http_names::kLink, "<https://example.com/skeleton.html>; rel=skeleton"}};
  SimRequest next_page_head_request(next_page_url, "text/html", head_params);

  SimRequest::Params skeleton_params;
  skeleton_params.response_http_status = 404;
  SimRequest skeleton_request(skeleton_url, "text/html", skeleton_params);

  Document& document = GetDocument();
  SkeletonLoader& loader = SkeletonLoader::Ensure(document);

  loader.AddSkeletonPrefetchLink(next_page_url);

  // Check that the Document does not initially have a ::skeleton pseudo-element
  Element* root = document.documentElement();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);

  next_page_head_request.Complete("");
  skeleton_request.Complete("");

  loader.NavigateTo(next_page_url);

  // Skeleton request failed, no skeleton should be rendered.
  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);
}

TEST_F(SkeletonLoaderTest, PseudoElementRecalcRoot) {
  Element* root = GetDocument().documentElement();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);

  InsertSkeletonTree(R"HTML(<div>Skeleton</div>)HTML");

  PseudoElement* skeleton_pseudo = root->GetPseudoElement(kPseudoIdSkeleton);
  ASSERT_TRUE(skeleton_pseudo);
  EXPECT_EQ(GetDocument().GetStyleEngine().style_recalc_root_.GetRootNode(),
            skeleton_pseudo);

  RemoveSkeletonTree();

  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);
  EXPECT_EQ(GetDocument().GetStyleEngine().style_recalc_root_.GetRootNode(),
            nullptr);
}

TEST_F(SkeletonLoaderTest, PropagateColorScheme) {
  UpdateAllLifecyclePhasesForTest();

  InsertSkeletonTree(
      R"HTML(<html><style>html { color-scheme: dark }</style>Skeleton</html>)HTML");
  UpdateAllLifecyclePhasesForTest();

  const Element* root = GetDocument().documentElement();
  const PseudoElement* pseudo = root->GetPseudoElement(kPseudoIdSkeleton);
  ASSERT_TRUE(pseudo);
  ASSERT_TRUE(pseudo->GetLayoutObject());
  EXPECT_EQ(pseudo->GetLayoutObject()->Style().UsedColorScheme(),
            blink::mojom::ColorScheme::kDark);
}

TEST_F(SkeletonLoaderTest, PropagateColorSchemeDynamic) {
  UpdateAllLifecyclePhasesForTest();

  InsertSkeletonTree(R"HTML(<html>Skeleton</html>)HTML");
  UpdateAllLifecyclePhasesForTest();

  const Element* root = GetDocument().documentElement();
  const PseudoElement* pseudo = root->GetPseudoElement(kPseudoIdSkeleton);
  ASSERT_TRUE(pseudo);
  ASSERT_TRUE(pseudo->GetLayoutObject());
  EXPECT_EQ(pseudo->GetLayoutObject()->Style().UsedColorScheme(),
            blink::mojom::ColorScheme::kLight);

  const ShadowRoot* shadow_root = pseudo->GetShadowRoot();
  ASSERT_TRUE(shadow_root);
  Element* skeleton_root = To<Element>(shadow_root->firstChild());
  skeleton_root->SetInlineStyleProperty(CSSPropertyID::kColorScheme, "dark");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(pseudo->GetLayoutObject()->Style().UsedColorScheme(),
            blink::mojom::ColorScheme::kDark);
}

TEST_F(SkeletonLoaderTest, InheritInitialPosition) {
  UpdateAllLifecyclePhasesForTest();

  InsertSkeletonTree(
      R"HTML(<html style="position:inherit">Skeleton</html>)HTML");
  UpdateAllLifecyclePhasesForTest();

  const Element* root = GetDocument().documentElement();
  const PseudoElement* pseudo = root->GetPseudoElement(kPseudoIdSkeleton);
  ASSERT_TRUE(pseudo);
  const ShadowRoot* shadow_root = pseudo->GetShadowRoot();
  ASSERT_TRUE(shadow_root);
  HTMLHtmlElement* skeleton_root =
      To<HTMLHtmlElement>(shadow_root->firstChild());
  EXPECT_EQ(skeleton_root->ComputedStyleRef().GetPosition(),
            EPosition::kStatic);
}

}  // namespace blink
