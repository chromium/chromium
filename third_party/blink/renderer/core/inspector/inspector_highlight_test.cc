// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_highlight.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/span.h"

namespace blink {

namespace {

using base::test::ParseJson;
using testing::ByRef;
using testing::Eq;

void AssertValueEqualsJSON(const std::unique_ptr<protocol::Value>& actual_value,
                           const std::string& json_expected) {
  std::string json_actual;
  auto status_to_json = crdtp::json::ConvertCBORToJSON(
      crdtp::SpanFrom(actual_value->Serialize()), &json_actual);
  EXPECT_TRUE(status_to_json.ok());
  base::Value parsed_json_actual = ParseJson(json_actual);
  base::Value parsed_json_expected = ParseJson(json_expected);
  EXPECT_THAT(parsed_json_actual, Eq(ByRef(parsed_json_expected)));
}

}  // namespace

class InspectorHighlightTest : public testing::Test {
 protected:
  void SetUp() override;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void InspectorHighlightTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
}

TEST_F(InspectorHighlightTest, BuildSnapContainerInfoNoSnapAreas) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id="target">test</div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().getElementById("target");
  EXPECT_FALSE(BuildSnapContainerInfo(target));
}

TEST_F(InspectorHighlightTest, BuildSnapContainerInfoSnapAreas) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #snap {
        background-color: white;
        scroll-snap-type: y mandatory;
        overflow-x: hidden;
        overflow-y: scroll;
        width: 150px;
        height: 150px;
      }
      #snap > div {
        width: 75px;
        height: 75px;
        scroll-snap-align: center;
        margin: 10px;
        padding: 10px;
      }
    </style>
    <div id="snap"><div>A</div><div>B</div></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* container = GetDocument().getElementById("snap");
  auto info = BuildSnapContainerInfo(container);
  EXPECT_TRUE(info);

  EXPECT_EQ(2u, info->getArray("snapAreas")->size());
  protocol::ErrorSupport errors;
  std::string expected_container = R"JSON(
    {
      "snapport":["M",8,8,"L",158,8,"L",158,158,"L",8,158,"Z"],
      "paddingBox":["M",8,8,"L",158,8,"L",158,158,"L",8,158,"Z"],
      "snapAreas": [
        {
          "path":["M",18,18,"L",113,18,"L",113,113,"L",18,113,"Z"],
          "borderBox":["M",18,18,"L",113,18,"L",113,113,"L",18,113,"Z"],
          "alignBlock":"center"
        },
        {
          "path":["M",18,123,"L",113,123,"L",113,218,"L",18,218,"Z"],
          "borderBox":["M",18,123,"L",113,123,"L",113,218,"L",18,218,"Z"],
          "alignBlock":"center"
        }
      ]
    }
  )JSON";
  AssertValueEqualsJSON(protocol::ValueConversions<protocol::Value>::fromValue(
                            info.get(), &errors),
                        expected_container);
}

TEST_F(InspectorHighlightTest, BuildSnapContainerInfoTopLevelSnapAreas) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      :root {
        scroll-snap-type: y mandatory;
        overflow-x: hidden;
        overflow-y: scroll;
      }
      div {
        width: 100%;
        height: 100vh;
        scroll-snap-align: start;
      }
    </style>
    <div>A</div><div>B</div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* container = GetDocument().documentElement();
  auto info = BuildSnapContainerInfo(container);
  EXPECT_TRUE(info);

  EXPECT_EQ(2u, info->getArray("snapAreas")->size());
  protocol::ErrorSupport errors;
  std::string expected_container = R"JSON(
    {
      "paddingBox": [ "M", 0, 0, "L", 800, 0, "L", 800, 600, "L", 0, 600, "Z" ],
      "snapAreas": [ {
          "alignBlock": "start",
          "borderBox": [ "M", 8, 0, "L", 792, 0, "L", 792, 600, "L", 8, 600, "Z" ],
          "path": [ "M", 8, 0, "L", 792, 0, "L", 792, 600, "L", 8, 600, "Z" ]
      }, {
          "alignBlock": "start",
          "borderBox": [ "M", 8, 600, "L", 792, 600, "L", 792, 1200, "L", 8, 1200, "Z" ],
          "path": [ "M", 8, 600, "L", 792, 600, "L", 792, 1200, "L", 8, 1200, "Z" ]
      } ],
      "snapport": [ "M", 0, 0, "L", 800, 0, "L", 800, 600, "L", 0, 600, "Z" ]
    }
  )JSON";
  AssertValueEqualsJSON(protocol::ValueConversions<protocol::Value>::fromValue(
                            info.get(), &errors),
                        expected_container);
}

TEST_F(InspectorHighlightTest,
       BuildContainerQueryContainerInfoWithoutDescendants) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #container {
        width: 400px;
        height: 500px;
        container-type: inline-size;
      }
    </style>
    <div id="container"></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* container = GetDocument().getElementById("container");
  auto info = BuildContainerQueryContainerInfo(
      container, InspectorContainerQueryContainerHighlightConfig(), 1.0f);
  EXPECT_TRUE(info);

  protocol::ErrorSupport errors;
  std::string expected_container = R"JSON(
    {
      "containerBorder":["M",8,8,"L",408,8,"L",408,508,"L",8,508,"Z"],
      "containerQueryContainerHighlightConfig": {}
    }
  )JSON";
  AssertValueEqualsJSON(protocol::ValueConversions<protocol::Value>::fromValue(
                            info.get(), &errors),
                        expected_container);
}

TEST_F(InspectorHighlightTest,
       BuildContainerQueryContainerInfoWithDescendants) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #container {
        width: 400px;
        height: 500px;
        container-type: inline-size;
      }
      @container (min-width: 100px) {
        .item {
          width: 100px;
          height: 100px;
        }
      }
    </style>
    <div id="container"><div class="item"></div></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  Element* container = GetDocument().getElementById("container");

  LineStyle line_style;
  line_style.color = Color(1, 1, 1);
  InspectorContainerQueryContainerHighlightConfig highlight_config;
  highlight_config.descendant_border = line_style;
  auto info =
      BuildContainerQueryContainerInfo(container, highlight_config, 1.0f);
  EXPECT_TRUE(info);

  protocol::ErrorSupport errors;
  std::string expected_container = R"JSON(
    {
      "containerBorder":["M",8,8,"L",408,8,"L",408,508,"L",8,508,"Z"],
      "containerQueryContainerHighlightConfig": {
        "descendantBorder": {
          "color": "rgb(1, 1, 1)",
          "pattern": ""
        }
      },
      "queryingDescendants": [ {
          "descendantBorder": [ "M", 8, 8, "L", 108, 8, "L", 108, 108, "L", 8, 108, "Z" ]
      } ]
    }
  )JSON";
  AssertValueEqualsJSON(protocol::ValueConversions<protocol::Value>::fromValue(
                            info.get(), &errors),
                        expected_container);
}

TEST_F(InspectorHighlightTest, BuildIsolatedElementInfo) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #element {
        width: 400px;
        height: 500px;
      }
    </style>
    <div id="element"></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* element = GetDocument().getElementById("element");
  auto info = BuildIsolatedElementInfo(
      *element, InspectorIsolationModeHighlightConfig(), 1.0f);
  EXPECT_TRUE(info);

  protocol::ErrorSupport errors;
  std::string expected_isolated_element = R"JSON(
    {
      "bidirectionResizerBorder": [ "M", 408, 508, "L", 428, 508, "L", 428, 528, "L", 408, 528, "Z" ],
      "currentHeight": 500,
      "currentWidth": 400,
      "currentX": 8,
      "currentY": 8,
      "heightResizerBorder": [ "M", 8, 508, "L", 408, 508, "L", 408, 528, "L", 8, 528, "Z" ],
      "isolationModeHighlightConfig": {
          "maskColor": "rgba(0, 0, 0, 0)",
          "resizerColor": "rgba(0, 0, 0, 0)",
          "resizerHandleColor": "rgba(0, 0, 0, 0)"
      },
      "widthResizerBorder": [ "M", 408, 8, "L", 428, 8, "L", 428, 508, "L", 408, 508, "Z" ]
    }
  )JSON";
  AssertValueEqualsJSON(protocol::ValueConversions<protocol::Value>::fromValue(
                            info.get(), &errors),
                        expected_isolated_element);
}

static std::string GetBackgroundColorFromElementInfo(Element* element) {
  EXPECT_TRUE(element);
  auto info = BuildElementInfo(element);
  EXPECT_TRUE(info);
  AppendStyleInfo(element, info.get(), {}, {});

  protocol::ErrorSupport errors;
  auto actual_value = protocol::ValueConversions<protocol::Value>::fromValue(
      info.get(), &errors);
  EXPECT_TRUE(actual_value);

  std::string json_actual;
  auto status_to_json = crdtp::json::ConvertCBORToJSON(
      crdtp::SpanFrom(actual_value->Serialize()), &json_actual);
  EXPECT_TRUE(status_to_json.ok());
  base::Value parsed_json_actual = ParseJson(json_actual);
  auto* style =
      parsed_json_actual.FindKeyOfType("style", base::Value::Type::DICTIONARY);
  EXPECT_TRUE(style);
  auto* backgroundColor = style->FindKeyOfType("background-color-css-text",
                                               base::Value::Type::STRING);
  if (!backgroundColor) {
    backgroundColor =
        style->FindKeyOfType("background-color", base::Value::Type::STRING);
  }
  EXPECT_TRUE(backgroundColor);
  return backgroundColor->GetString();
}

TEST_F(InspectorHighlightTest, BuildElementInfo_Colors) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      div {
        width: 400px;
        height: 500px;
      }
      #lab {
        background-color: lab(100% 0 0);
      }
      #color {
        background-color: color(display-p3 50% 50% 50%);
      }
      #hex {
        background-color: #ff00ff;
      }
      #rgb {
        background-color: rgb(128 128 128);
      }
      #var {
        background-color: Var(--lab);
      }
      :root {
        --lab: lab(20% -10 -10);
      }
    </style>
    <div id="lab"></div>
    <div id="color"></div>
    <div id="hex"></div>
    <div id="rgb"></div>
    <div id="var"></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      GetBackgroundColorFromElementInfo(GetDocument().getElementById("lab")),
      Eq("lab(100 0 0)"));
  EXPECT_THAT(
      GetBackgroundColorFromElementInfo(GetDocument().getElementById("color")),
      Eq("color(display-p3 0.5 0.5 0.5)"));
  EXPECT_THAT(
      GetBackgroundColorFromElementInfo(GetDocument().getElementById("hex")),
      Eq("#FF00FFFF"));
  EXPECT_THAT(
      GetBackgroundColorFromElementInfo(GetDocument().getElementById("rgb")),
      Eq("#808080FF"));
  EXPECT_THAT(
      GetBackgroundColorFromElementInfo(GetDocument().getElementById("var")),
      Eq("lab(20 -10 -10)"));
}

}  // namespace blink
