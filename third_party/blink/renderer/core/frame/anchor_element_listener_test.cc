// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/anchor_element_listener.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class AnchorElementListenerTest : public SimTest {};

TEST_F(AnchorElementListenerTest, ValidHref) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(
      "<a id='anchor1' href='https://anchor1.com/'>example</a>");
  auto* anchor_element_listener_ =
      MakeGarbageCollected<AnchorElementListener>();
  auto* anchor_element =
      DynamicTo<HTMLAnchorElement>(GetDocument().getElementById("anchor1"));
  KURL URL =
      anchor_element_listener_->GetHrefEligibleForPreloading(*anchor_element);
  KURL expected_url = KURL("https://anchor1.com/");
  EXPECT_FALSE(URL.IsEmpty());
  EXPECT_EQ(expected_url, URL);
}

TEST_F(AnchorElementListenerTest, InvalidHref) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete("<a id='anchor1' href='about:blank'>example</a>");
  auto* anchor_element_listener_ =
      MakeGarbageCollected<AnchorElementListener>();
  auto* anchor_element =
      DynamicTo<HTMLAnchorElement>(GetDocument().getElementById("anchor1"));
  EXPECT_TRUE(
      anchor_element_listener_->GetHrefEligibleForPreloading(*anchor_element)
          .IsEmpty());
}

TEST_F(AnchorElementListenerTest, OneAnchorElementCheck) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(
      "<a id='anchor1' href='https://anchor1.com/'>example</a>");
  auto* anchor_element_listener_ =
      MakeGarbageCollected<AnchorElementListener>();
  auto* element = GetDocument().getElementById("anchor1");
  auto* anchor_element =
      anchor_element_listener_->FirstAnchorElementIncludingSelf(element);
  auto* expected_anchor =
      DynamicTo<HTMLAnchorElement>(GetDocument().getElementById("anchor1"));
  EXPECT_EQ(expected_anchor, anchor_element);
}

TEST_F(AnchorElementListenerTest, NestedAnchorElementCheck) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(
      "<a id='anchor1' href='https://anchor1.com/'><a id='anchor2' "
      "href='https://anchor2.com/'></a></a>");
  auto* anchor_element_listener_ =
      MakeGarbageCollected<AnchorElementListener>();
  auto* element = GetDocument().getElementById("anchor2");
  auto* anchor_element =
      anchor_element_listener_->FirstAnchorElementIncludingSelf(element);
  auto* expected_anchor =
      DynamicTo<HTMLAnchorElement>(GetDocument().getElementById("anchor2"));
  EXPECT_EQ(expected_anchor, anchor_element);
}

TEST_F(AnchorElementListenerTest, NestedDivAnchorElementCheck) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(
      "<a id='anchor1' href='https://anchor1.com/'><div "
      "id='div1id'></div></a>");
  auto* anchor_element_listener_ =
      MakeGarbageCollected<AnchorElementListener>();
  auto* element = GetDocument().getElementById("div1id");
  auto* anchor_element =
      anchor_element_listener_->FirstAnchorElementIncludingSelf(element);
  auto* expected_anchor =
      DynamicTo<HTMLAnchorElement>(GetDocument().getElementById("anchor1"));
  EXPECT_EQ(expected_anchor, anchor_element);
}

TEST_F(AnchorElementListenerTest, MultipleNestedAnchorElementCheck) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(
      "<a id='anchor1' href='https://anchor1.com/'><p id='paragraph1id'><div "
      "id='div1id'><div id='div2id'></div></div></p></a>");
  auto* anchor_element_listener_ =
      MakeGarbageCollected<AnchorElementListener>();
  auto* element = GetDocument().getElementById("div2id");
  auto* anchor_element =
      anchor_element_listener_->FirstAnchorElementIncludingSelf(element);
  auto* expected_anchor =
      DynamicTo<HTMLAnchorElement>(GetDocument().getElementById("anchor1"));
  EXPECT_EQ(expected_anchor, anchor_element);
}

TEST_F(AnchorElementListenerTest, NoAnchorElementCheck) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete("<div id='div1id'></div>");
  auto* anchor_element_listener_ =
      MakeGarbageCollected<AnchorElementListener>();
  auto* element = GetDocument().getElementById("div1id");
  auto* anchor_element =
      anchor_element_listener_->FirstAnchorElementIncludingSelf(element);
  EXPECT_EQ(nullptr, anchor_element);
}

}  // namespace blink
