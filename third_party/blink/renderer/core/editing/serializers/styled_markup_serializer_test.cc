// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/serializers/styled_markup_serializer.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

// Returns the first mismatching index in |input1| and |input2|.
static size_t Mismatch(const std::string& input1, const std::string& input2) {
  size_t index = 0;
  for (auto char1 : input1) {
    if (index == input2.size() || char1 != input2[index])
      return index;
    ++index;
  }
  return input1.size();
}

// This is smoke test of |StyledMarkupSerializer|. Full testing will be done
// in web tests.
class StyledMarkupSerializerTest : public EditingTestBase {
 protected:
  template <typename Strategy>
  std::string Serialize(AnnotateForInterchange = kDoNotAnnotateForInterchange);

  template <typename Strategy>
  std::string SerializePart(
      const PositionTemplate<Strategy>& start,
      const PositionTemplate<Strategy>& end,
      AnnotateForInterchange = kDoNotAnnotateForInterchange);
};

template <typename Strategy>
std::string StyledMarkupSerializerTest::Serialize(
    AnnotateForInterchange should_annotate) {
  PositionTemplate<Strategy> start = PositionTemplate<Strategy>(
      GetDocument().body(), PositionAnchorType::kBeforeChildren);
  PositionTemplate<Strategy> end = PositionTemplate<Strategy>(
      GetDocument().body(), PositionAnchorType::kAfterChildren);
  return CreateMarkup(start, end, should_annotate).Utf8();
}

template <typename Strategy>
std::string StyledMarkupSerializerTest::SerializePart(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end,
    AnnotateForInterchange should_annotate) {
  return CreateMarkup(start, end, should_annotate).Utf8();
}

TEST_F(StyledMarkupSerializerTest, TextOnly) {
  const char* body_content = "Hello world!";
  SetBodyContent(body_content);
  const char* expected_result =
      "<span style=\"display: inline !important; float: none;\">Hello "
      "world!</span>";
  EXPECT_EQ(expected_result, Serialize<EditingStrategy>());
  EXPECT_EQ(expected_result, Serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, BlockFormatting) {
  const char* body_content = "<div>Hello world!</div>";
  SetBodyContent(body_content);
  EXPECT_EQ(body_content, Serialize<EditingStrategy>());
  EXPECT_EQ(body_content, Serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlInput) {
  const char* body_content = "<input value='foo'>";
  SetBodyContent(body_content);
  const char* expected_result = "<input value=\"foo\">";
  EXPECT_EQ(expected_result, Serialize<EditingStrategy>());
  EXPECT_EQ(expected_result, Serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlInputRange) {
  const char* body_content = "<input type=range>";
  SetBodyContent(body_content);
  const char* expected_result = "<input type=\"range\">";
  EXPECT_EQ(expected_result, Serialize<EditingStrategy>());
  EXPECT_EQ(expected_result, Serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlSelect) {
  const char* body_content =
      "<select><option value=\"1\">one</option><option "
      "value=\"2\">two</option></select>";
  SetBodyContent(body_content);
  EXPECT_EQ(body_content, Serialize<EditingStrategy>());
  EXPECT_EQ(body_content, Serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlTextArea) {
  const char* body_content = "<textarea>foo bar</textarea>";
  SetBodyContent(body_content);
  const char* expected_result = "<textarea></textarea>";
  EXPECT_EQ(expected_result, Serialize<EditingStrategy>())
      << "contents of TEXTAREA element should not be appeared.";
  EXPECT_EQ(expected_result, Serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, HeadingFormatting) {
  const char* body_content = "<h4>Hello world!</h4>";
  SetBodyContent(body_content);
  EXPECT_EQ(body_content, Serialize<EditingStrategy>());
  EXPECT_EQ(body_content, Serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, InlineFormatting) {
  const char* body_content = "<b>Hello world!</b>";
  SetBodyContent(body_content);
  EXPECT_EQ(body_content, Serialize<EditingStrategy>());
  EXPECT_EQ(body_content, Serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, Mixed) {
  const char* body_content = "<i>foo<b>bar</b>baz</i>";
  SetBodyContent(body_content);
  EXPECT_EQ(body_content, Serialize<EditingStrategy>());
  EXPECT_EQ(body_content, Serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeDistributeOrder) {
  const char* body_content =
      "<p id=\"host\">00<b id=\"one\">11</b><b id=\"two\">22</b>33</p>";
  const char* shadow_content =
      "<a><content select=#two></content><content select=#one></content></a>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  EXPECT_EQ("<p id=\"host\"><b id=\"one\">11</b><b id=\"two\">22</b></p>",
            Serialize<EditingStrategy>())
      << "00 and 33 aren't appeared since they aren't distributed.";
  EXPECT_EQ(
      "<p id=\"host\"><a><b id=\"two\">22</b><b id=\"one\">11</b></a></p>",
      Serialize<EditingInFlatTreeStrategy>())
      << "00 and 33 aren't appeared since they aren't distributed.";
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeInput) {
  const char* body_content =
      "<p id=\"host\">00<b id=\"one\">11</b><b id=\"two\"><input "
      "value=\"22\"></b>33</p>";
  const char* shadow_content =
      "<a><content select=#two></content><content select=#one></content></a>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  EXPECT_EQ(
      "<p id=\"host\"><b id=\"one\">11</b><b id=\"two\"><input "
      "value=\"22\"></b></p>",
      Serialize<EditingStrategy>())
      << "00 and 33 aren't appeared since they aren't distributed.";
  EXPECT_EQ(
      "<p id=\"host\"><a><b id=\"two\"><input value=\"22\"></b><b "
      "id=\"one\">11</b></a></p>",
      Serialize<EditingInFlatTreeStrategy>())
      << "00 and 33 aren't appeared since they aren't distributed.";
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeNested) {
  const char* body_content =
      "<p id=\"host\">00<b id=\"one\">11</b><b id=\"two\">22</b>33</p>";
  const char* shadow_content1 =
      "<a><content select=#two></content><b id=host2></b><content "
      "select=#one></content></a>";
  const char* shadow_content2 = "NESTED";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root1 = SetShadowContent(shadow_content1, "host");
  CreateShadowRootForElementWithIDAndSetInnerHTML(*shadow_root1, "host2",
                                                  shadow_content2);

  EXPECT_EQ("<p id=\"host\"><b id=\"one\">11</b><b id=\"two\">22</b></p>",
            Serialize<EditingStrategy>())
      << "00 and 33 aren't appeared since they aren't distributed.";
  EXPECT_EQ(
      "<p id=\"host\"><a><b id=\"two\">22</b><b id=\"host2\">NESTED</b><b "
      "id=\"one\">11</b></a></p>",
      Serialize<EditingInFlatTreeStrategy>())
      << "00 and 33 aren't appeared since they aren't distributed.";
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeInterchangedNewline) {
  const char* body_content = "<a id=host><b id=one>1</b></a>";
  const char* shadow_content = "<content select=#one></content><div><br></div>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  std::string result_from_dom_tree =
      Serialize<EditingStrategy>(kAnnotateForInterchange);
  std::string result_from_flat_tree =
      Serialize<EditingInFlatTreeStrategy>(kAnnotateForInterchange);
  size_t mismatched_index =
      Mismatch(result_from_dom_tree, result_from_flat_tree);

  // Note: We check difference between DOM tree result and flat tree
  // result, because results contain "style" attribute and this test
  // doesn't care about actual value of "style" attribute.
  EXPECT_EQ("/a>", result_from_dom_tree.substr(mismatched_index));
  EXPECT_EQ("div><br></div></a><br class=\"Apple-interchange-newline\">",
            result_from_flat_tree.substr(mismatched_index));
}

TEST_F(StyledMarkupSerializerTest, StyleDisplayNone) {
  const char* body_content = "<b>00<i style='display:none'>11</i>22</b>";
  SetBodyContent(body_content);
  const char* expected_result = "<b>0022</b>";
  EXPECT_EQ(expected_result, Serialize<EditingStrategy>());
  EXPECT_EQ(expected_result, Serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, StyleDisplayNoneAndNewLines) {
  const char* body_content = "<div style='display:none'>11</div>\n\n";
  SetBodyContent(body_content);
  EXPECT_EQ("", Serialize<EditingStrategy>());
  EXPECT_EQ("", Serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeStyle) {
  const char* body_content =
      "<p id='host' style='color: red'><span style='font-weight: bold;'><span "
      "id='one'>11</span></span></p>\n";
  SetBodyContent(body_content);
  Element* one = GetDocument().getElementById("one");
  auto* text = To<Text>(one->firstChild());
  Position start_dom(text, 0);
  Position end_dom(text, 2);
  const std::string& serialized_dom = SerializePart<EditingStrategy>(
      start_dom, end_dom, kAnnotateForInterchange);

  body_content =
      "<p id='host' style='color: red'>00<span id='one'>11</span>22</p>\n";
  const char* shadow_content =
      "<span style='font-weight: bold'><content select=#one></content></span>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  one = GetDocument().getElementById("one");
  text = To<Text>(one->firstChild());
  PositionInFlatTree start_ict(text, 0);
  PositionInFlatTree end_ict(text, 2);
  const std::string& serialized_ict = SerializePart<EditingInFlatTreeStrategy>(
      start_ict, end_ict, kAnnotateForInterchange);

  EXPECT_EQ(serialized_dom, serialized_ict);
}

TEST_F(StyledMarkupSerializerTest, AcrossShadow) {
  const char* body_content =
      "<p id='host1'>[<span id='one'>11</span>]</p><p id='host2'>[<span "
      "id='two'>22</span>]</p>";
  SetBodyContent(body_content);
  Element* one = GetDocument().getElementById("one");
  Element* two = GetDocument().getElementById("two");
  Position start_dom(To<Text>(one->firstChild()), 0);
  Position end_dom(To<Text>(two->firstChild()), 2);
  const std::string& serialized_dom = SerializePart<EditingStrategy>(
      start_dom, end_dom, kAnnotateForInterchange);

  body_content =
      "<p id='host1'><span id='one'>11</span></p><p id='host2'><span "
      "id='two'>22</span></p>";
  const char* shadow_content1 = "[<content select=#one></content>]";
  const char* shadow_content2 = "[<content select=#two></content>]";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content1, "host1");
  SetShadowContent(shadow_content2, "host2");
  one = GetDocument().getElementById("one");
  two = GetDocument().getElementById("two");
  PositionInFlatTree start_ict(To<Text>(one->firstChild()), 0);
  PositionInFlatTree end_ict(To<Text>(two->firstChild()), 2);
  const std::string& serialized_ict = SerializePart<EditingInFlatTreeStrategy>(
      start_ict, end_ict, kAnnotateForInterchange);

  EXPECT_EQ(serialized_dom, serialized_ict);
}

TEST_F(StyledMarkupSerializerTest, AcrossInvisibleElements) {
  const char* body_content =
      "<span id='span1' style='display: none'>11</span><span id='span2' "
      "style='display: none'>22</span>";
  SetBodyContent(body_content);
  Element* span1 = GetDocument().getElementById("span1");
  Element* span2 = GetDocument().getElementById("span2");
  Position start_dom = Position::FirstPositionInNode(*span1);
  Position end_dom = Position::LastPositionInNode(*span2);
  EXPECT_EQ("", SerializePart<EditingStrategy>(start_dom, end_dom));
  PositionInFlatTree start_ict =
      PositionInFlatTree::FirstPositionInNode(*span1);
  PositionInFlatTree end_ict = PositionInFlatTree::LastPositionInNode(*span2);
  EXPECT_EQ("", SerializePart<EditingInFlatTreeStrategy>(start_ict, end_ict));
}

TEST_F(StyledMarkupSerializerTest, DisplayContentsStyle) {
  const char* body_content = "1<span style='display: contents'>2</span>3";
  const char* expected_result =
      "<span style=\"display: inline !important; float: none;\">1</span><span "
      "style=\"display: contents;\">2</span><span style=\"display: inline "
      "!important; float: none;\">3</span>";
  SetBodyContent(body_content);
  EXPECT_EQ(expected_result, Serialize<EditingStrategy>());
  EXPECT_EQ(expected_result, Serialize<EditingInFlatTreeStrategy>());
}

}  // namespace blink
