// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/shadow_root_v0.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

namespace {

bool HasSelectorForIdInShadow(Element* host, const AtomicString& id) {
  DCHECK(host);
  return host->GetShadowRoot()->V0().EnsureSelectFeatureSet().HasSelectorForId(
      id);
}

bool HasSelectorForClassInShadow(Element* host,
                                 const AtomicString& class_name) {
  DCHECK(host);
  return host->GetShadowRoot()
      ->V0()
      .EnsureSelectFeatureSet()
      .HasSelectorForClass(class_name);
}

bool HasSelectorForAttributeInShadow(Element* host,
                                     const AtomicString& attribute_name) {
  DCHECK(host);
  return host->GetShadowRoot()
      ->V0()
      .EnsureSelectFeatureSet()
      .HasSelectorForAttribute(attribute_name);
}

class ShadowDOMV0Test : public SimTest {};

TEST_F(ShadowDOMV0Test, FeatureSetId) {
  LoadURL("about:blank");
  auto* host = GetDocument().CreateRawElement(html_names::kDivTag);
  auto* content = GetDocument().CreateRawElement(html_names::kContentTag);
  content->setAttribute("select", "#foo");
  host->CreateV0ShadowRootForTesting().AppendChild(content);
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "host"));
  content->setAttribute("select", "#bar");
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "foo"));
  content->setAttribute("select", "");
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "foo"));
}

TEST_F(ShadowDOMV0Test, FeatureSetClassName) {
  LoadURL("about:blank");
  auto* host = GetDocument().CreateRawElement(html_names::kDivTag);
  auto* content = GetDocument().CreateRawElement(html_names::kContentTag);
  content->setAttribute("select", ".foo");
  host->CreateV0ShadowRootForTesting().AppendChild(content);
  EXPECT_TRUE(HasSelectorForClassInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "host"));
  content->setAttribute("select", ".bar");
  EXPECT_TRUE(HasSelectorForClassInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "foo"));
  content->setAttribute("select", "");
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "foo"));
}

TEST_F(ShadowDOMV0Test, FeatureSetAttributeName) {
  LoadURL("about:blank");
  auto* host = GetDocument().CreateRawElement(html_names::kDivTag);
  auto* content = GetDocument().CreateRawElement(html_names::kContentTag);
  content->setAttribute("select", "div[foo]");
  host->CreateV0ShadowRootForTesting().AppendChild(content);
  EXPECT_TRUE(HasSelectorForAttributeInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "host"));
  content->setAttribute("select", "div[bar]");
  EXPECT_TRUE(HasSelectorForAttributeInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "foo"));
  content->setAttribute("select", "");
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "foo"));
}

TEST_F(ShadowDOMV0Test, FeatureSetMultipleSelectors) {
  LoadURL("about:blank");
  auto* host = GetDocument().CreateRawElement(html_names::kDivTag);
  auto* content = GetDocument().CreateRawElement(html_names::kContentTag);
  content->setAttribute("select", "#foo,.bar,div[baz]");
  host->CreateV0ShadowRootForTesting().AppendChild(content);
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "baz"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "foo"));
  EXPECT_TRUE(HasSelectorForClassInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "baz"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "bar"));
  EXPECT_TRUE(HasSelectorForAttributeInShadow(host, "baz"));
}

TEST_F(ShadowDOMV0Test, FeatureSetSubtree) {
  LoadURL("about:blank");
  auto* host = GetDocument().CreateRawElement(html_names::kDivTag);
  host->CreateV0ShadowRootForTesting().SetInnerHTMLFromString(R"HTML(
    <div>
      <div></div>
      <content select='*'></content>
      <div>
        <content select='div[foo=piyo]'></content>
      </div>
    </div>
  )HTML");
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForClassInShadow(host, "foo"));
  EXPECT_TRUE(HasSelectorForAttributeInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForAttributeInShadow(host, "piyo"));
}

TEST_F(ShadowDOMV0Test, FeatureSetMultipleShadowRoots) {
  LoadURL("about:blank");
  auto* host = GetDocument().CreateRawElement(html_names::kDivTag);
  auto& host_shadow = host->CreateV0ShadowRootForTesting();
  host_shadow.SetInnerHTMLFromString("<content select='#foo'></content>");
  auto* child = GetDocument().CreateRawElement(html_names::kDivTag);
  auto& child_root = child->CreateV0ShadowRootForTesting();
  auto* child_content = GetDocument().CreateRawElement(html_names::kContentTag);
  child_content->setAttribute("select", "#bar");
  child_root.AppendChild(child_content);
  host_shadow.AppendChild(child);
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "foo"));
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "bar"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "baz"));
  child_content->setAttribute("select", "#baz");
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "foo"));
  EXPECT_FALSE(HasSelectorForIdInShadow(host, "bar"));
  EXPECT_TRUE(HasSelectorForIdInShadow(host, "baz"));
}

TEST_F(ShadowDOMV0Test, ReattachNonDistributedElements) {
  LoadURL("about:blank");

  auto* host = GetDocument().CreateRawElement(html_names::kDivTag);
  auto* inner_host = GetDocument().CreateRawElement(html_names::kDivTag);
  auto* span = GetDocument().CreateRawElement(html_names::kSpanTag);

  GetDocument().body()->appendChild(host);
  host->appendChild(inner_host);
  inner_host->appendChild(span);

  GetDocument().View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);

  host->CreateV0ShadowRootForTesting();
  inner_host->CreateV0ShadowRootForTesting();
  inner_host->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                     CSSValueID::kInlineBlock);
  span->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kBlock);

  GetDocument().View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);

  EXPECT_FALSE(span->NeedsReattachLayoutTree());
}

TEST_F(ShadowDOMV0Test, DetachLayoutTreeOnShadowRootCreation) {
  LoadURL("about:blank");

  auto* host = GetDocument().CreateRawElement(html_names::kDivTag);
  host->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kContents);
  auto* span = GetDocument().CreateRawElement(html_names::kSpanTag);
  host->appendChild(span);
  GetDocument().body()->appendChild(host);
  GetDocument().View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);

  EXPECT_TRUE(span->GetLayoutObject());

  host->CreateV0ShadowRootForTesting();

  EXPECT_FALSE(span->GetLayoutObject());
}

}  // namespace

}  // namespace blink
