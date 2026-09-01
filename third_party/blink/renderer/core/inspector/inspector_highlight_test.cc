// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_highlight.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/span.h"

namespace blink {

namespace {

using base::test::ParseJson;
using testing::ByRef;
using testing::Eq;
using testing::UnorderedElementsAre;

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

std::string SerializeToJson(const protocol::Serializable& value) {
  std::string json;
  auto status_to_json =
      crdtp::json::ConvertCBORToJSON(crdtp::SpanFrom(value.Serialize()), &json);
  EXPECT_TRUE(status_to_json.ok());
  return json;
}

class ScaledChromeClient final : public EmptyChromeClient {
 public:
  explicit ScaledChromeClient(float window_to_viewport_scale)
      : window_to_viewport_scale_(window_to_viewport_scale) {}

  float WindowToViewportScalar(LocalFrame*,
                               const float scalar_value) const override {
    return scalar_value * window_to_viewport_scale_;
  }

 private:
  const float window_to_viewport_scale_;
};

}  // namespace

class InspectorHighlightTest : public testing::Test {
 protected:
  void SetUp() override;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  test::TaskEnvironment task_environment_;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void InspectorHighlightTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
}

TEST_F(InspectorHighlightTest, BuildSnapContainerInfoNoSnapAreas) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="target">test</div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_FALSE(BuildSnapContainerInfo(target));
}

TEST_F(InspectorHighlightTest, BuildSnapContainerInfoSnapAreas) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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
  Element* container = GetDocument().getElementById(AtomicString("snap"));
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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
  Element* container = GetDocument().getElementById(AtomicString("container"));
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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

  Element* container = GetDocument().getElementById(AtomicString("container"));

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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #element {
        width: 400px;
        height: 500px;
      }
    </style>
    <div id="element"></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* element = GetDocument().getElementById(AtomicString("element"));
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
  AXContext ax_context(element->GetDocument(), ui::kAXModeBasic);
  element->GetDocument().View()->UpdateAllLifecyclePhasesForTest();
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
  base::DictValue parsed_json_actual = ParseJson(json_actual).TakeDict();
  auto* style = parsed_json_actual.FindDict("style");
  EXPECT_TRUE(style);
  auto* background_color = style->FindString("background-color-css-text");
  if (!background_color) {
    background_color = style->FindString("background-color");
  }
  EXPECT_TRUE(background_color);
  return std::move(*background_color);
}

TEST_F(InspectorHighlightTest, BuildElementInfo_Colors) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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
  EXPECT_THAT(GetBackgroundColorFromElementInfo(
                  GetDocument().getElementById(AtomicString("lab"))),
              Eq("lab(100 0 0)"));
  EXPECT_THAT(GetBackgroundColorFromElementInfo(
                  GetDocument().getElementById(AtomicString("color"))),
              Eq("color(display-p3 0.5 0.5 0.5)"));
  EXPECT_THAT(GetBackgroundColorFromElementInfo(
                  GetDocument().getElementById(AtomicString("hex"))),
              Eq("#FF00FFFF"));
  EXPECT_THAT(GetBackgroundColorFromElementInfo(
                  GetDocument().getElementById(AtomicString("rgb"))),
              Eq("#808080FF"));
  EXPECT_THAT(GetBackgroundColorFromElementInfo(
                  GetDocument().getElementById(AtomicString("var"))),
              Eq("lab(20 -10 -10)"));
}

TEST_F(InspectorHighlightTest, GridLineNames) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
    #grid {
      display: grid;
      grid-template-columns: [a] 1fr [b] 1fr [c] 1fr;
      grid-template-rows: [d] 1fr [e] 1fr [f] 1fr;
    }
    #subgrid {
      display: grid;
      grid-column: 1 / 4;
      grid-row: 1 / 4;
      grid-template-columns: subgrid [a_sub] [b_sub] [c_sub];
      grid-template-rows: subgrid [d_sub] [e_sub] [f_sub];
    }
    </style>
    <div id="grid">
      <div id="subgrid">
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
      </div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Node* subgrid = GetDocument().getElementById(AtomicString("subgrid"));
  EXPECT_TRUE(subgrid);
  auto info =
      InspectorGridHighlight(subgrid, InspectorHighlight::DefaultGridConfig());
  EXPECT_TRUE(info);

  auto GetLineNames = [](protocol::ListValue* row_or_column_list) {
    Vector<String> ret;
    for (wtf_size_t i = 0; i < row_or_column_list->size(); ++i) {
      protocol::DictionaryValue* current_value =
          static_cast<protocol::DictionaryValue*>(row_or_column_list->at(i));

      String string_value;
      EXPECT_TRUE(current_value->getString("name", &string_value));
      ret.push_back(string_value);
    }
    return ret;
  };

  EXPECT_THAT(GetLineNames(info->getArray("rowLineNameOffsets")),
              UnorderedElementsAre("d", "d_sub", "e", "e_sub", "f", "f_sub"));
  EXPECT_THAT(GetLineNames(info->getArray("columnLineNameOffsets")),
              UnorderedElementsAre("a", "a_sub", "b", "b_sub", "c", "c_sub"));
}

TEST_F(InspectorHighlightTest, GridAreaNames) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
    #grid {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      grid-template-rows: 1fr 1fr 1fr;
      grid-template-areas:
            "a a a"
            "b b b"
            "c c c";
    }
    #subgrid {
      display: grid;
      grid-column: 1 / 4;
      grid-row: 1 / 4;
      grid-template-columns: subgrid;
      grid-template-rows: subgrid;
      grid-template-areas:
            "d d d"
            "e e e"
            "f f f";
    }
    </style>
    <div id="grid">
      <div id="subgrid">
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
        <div class="griditem"></div>
      </div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  auto CompareAreaNames = [](protocol::DictionaryValue* area_names,
                             Vector<String>& expected_names) -> void {
    for (String& name : expected_names) {
      EXPECT_TRUE(area_names->get(name));
    }
  };

  Node* grid = GetDocument().getElementById(AtomicString("grid"));
  EXPECT_TRUE(grid);
  auto grid_info =
      InspectorGridHighlight(grid, InspectorHighlight::DefaultGridConfig());
  EXPECT_TRUE(grid_info);
  protocol::DictionaryValue* grid_area_names =
      grid_info->getObject("areaNames");
  EXPECT_EQ(grid_area_names->size(), 3u);

  Vector<String> expected_grid_area_names = {"a", "b", "c"};
  CompareAreaNames(grid_area_names, expected_grid_area_names);

  Node* subgrid = GetDocument().getElementById(AtomicString("subgrid"));
  EXPECT_TRUE(subgrid);
  auto subgrid_info =
      InspectorGridHighlight(subgrid, InspectorHighlight::DefaultGridConfig());
  EXPECT_TRUE(subgrid_info);

  protocol::DictionaryValue* subgrid_area_names =
      subgrid_info->getObject("areaNames");
  EXPECT_EQ(subgrid_area_names->size(), 6u);

  Vector<String> expected_subgrid_area_names = {"a", "b", "c", "d", "e", "f"};
  CompareAreaNames(subgrid_area_names, expected_subgrid_area_names);
}

TEST_F(InspectorHighlightTest, GridAreaNamesRTL) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
    body {
      margin: 0;
      padding: 0;
    }
    #grid {
      display: grid;
      direction: rtl;
      width: 400px;
      height: 300px;
      grid-template-columns: 100px 200px;
      grid-template-rows: 150px 150px;
      grid-template-areas:
            "a b"
            "c d";
    }
    </style>
    <div id="grid"></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  Node* grid = GetDocument().getElementById(AtomicString("grid"));
  EXPECT_TRUE(grid);
  auto grid_info =
      InspectorGridHighlight(grid, InspectorHighlight::DefaultGridConfig());
  EXPECT_TRUE(grid_info);
  protocol::DictionaryValue* grid_area_names =
      grid_info->getObject("areaNames");
  EXPECT_TRUE(grid_area_names);
  EXPECT_EQ(grid_area_names->size(), 4u);

  protocol::ListValue* area_a = grid_area_names->getArray("a");
  EXPECT_TRUE(area_a);
  EXPECT_EQ(SerializeToJson(*area_a),
            "[\"M\",400,0,\"L\",300,0,\"L\",300,150,\"L\",400,150,\"Z\"]");

  protocol::ListValue* area_b = grid_area_names->getArray("b");
  EXPECT_TRUE(area_b);
  EXPECT_EQ(SerializeToJson(*area_b),
            "[\"M\",300,0,\"L\",100,0,\"L\",100,150,\"L\",300,150,\"Z\"]");

  protocol::ListValue* area_c = grid_area_names->getArray("c");
  EXPECT_TRUE(area_c);
  EXPECT_EQ(SerializeToJson(*area_c),
            "[\"M\",400,150,\"L\",300,150,\"L\",300,300,\"L\",400,300,\"Z\"]");

  protocol::ListValue* area_d = grid_area_names->getArray("d");
  EXPECT_TRUE(area_d);
  EXPECT_EQ(SerializeToJson(*area_d),
            "[\"M\",300,150,\"L\",100,150,\"L\",100,300,\"L\",300,300,\"Z\"]");

  // Also test RTL grid with gaps and multi-track spanning area.
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
    body {
      margin: 0;
      padding: 0;
    }
    #grid2 {
      display: grid;
      direction: rtl;
      width: 400px;
      height: 300px;
      grid-gap: 20px 10px;
      grid-template-columns: 100px 100px 100px;
      grid-template-rows: 100px 100px;
      grid-template-areas:
            "header header header"
            "sidebar main main";
    }
    </style>
    <div id="grid2"></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  Node* grid2 = GetDocument().getElementById(AtomicString("grid2"));
  EXPECT_TRUE(grid2);
  auto grid2_info =
      InspectorGridHighlight(grid2, InspectorHighlight::DefaultGridConfig());
  EXPECT_TRUE(grid2_info);
  protocol::DictionaryValue* grid2_area_names =
      grid2_info->getObject("areaNames");
  EXPECT_TRUE(grid2_area_names);
  EXPECT_EQ(grid2_area_names->size(), 3u);

  // Total tracks width = 100 + 10 + 100 + 10 + 100 = 320px.
  // rtl_offset = 400 - 320 = 80px.
  // Col 0: 300..400 (x right=400, left=300)
  // Gap: 290..300
  // Col 1: 190..290 (x right=290, left=190)
  // Gap: 180..190
  // Col 2: 80..180 (x right=180, left=80)
  // Row 0: y top=0, bottom=100
  // Row 1: y top=120, bottom=220

  // "header": cols 0..3 (spans all 3 cols, x right=400, left=80), row 0 (y 0..100)
  protocol::ListValue* area_header = grid2_area_names->getArray("header");
  EXPECT_TRUE(area_header);
  EXPECT_EQ(SerializeToJson(*area_header),
            "[\"M\",400,0,\"L\",80,0,\"L\",80,100,\"L\",400,100,\"Z\"]");

  // "sidebar": col 0 (x right=400, left=300), row 1 (y 120..220)
  protocol::ListValue* area_sidebar = grid2_area_names->getArray("sidebar");
  EXPECT_TRUE(area_sidebar);
  EXPECT_EQ(SerializeToJson(*area_sidebar),
            "[\"M\",400,120,\"L\",300,120,\"L\",300,220,\"L\",400,220,\"Z\"]");

  // "main": cols 1..3 (x right=290, left=80), row 1 (y 120..220)
  protocol::ListValue* area_main = grid2_area_names->getArray("main");
  EXPECT_TRUE(area_main);
  EXPECT_EQ(SerializeToJson(*area_main),
            "[\"M\",290,120,\"L\",80,120,\"L\",80,220,\"L\",290,220,\"Z\"]");
}

TEST_F(InspectorHighlightTest, GridLanesAreaNamesRTL) {
  ScopedCSSGridLanesLayoutForTest grid_lanes_feature(true);

  // Test column lanes (is_for_columns = true) in RTL:
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
    body {
      margin: 0;
      padding: 0;
    }
    #lanes {
      display: grid-lanes;
      direction: rtl;
      width: 400px;
      height: 300px;
      grid-template-columns: 100px 200px;
      grid-template-areas: "a b";
    }
    </style>
    <div id="lanes"></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  Node* lanes = GetDocument().getElementById(AtomicString("lanes"));
  EXPECT_TRUE(lanes);
  auto lanes_info =
      InspectorGridHighlight(lanes, InspectorHighlight::DefaultGridConfig());
  EXPECT_TRUE(lanes_info);
  protocol::DictionaryValue* lanes_area_names =
      lanes_info->getObject("areaNames");
  EXPECT_TRUE(lanes_area_names);
  EXPECT_EQ(lanes_area_names->size(), 2u);

  // In RTL, column 0 ("a") is on the right [300..400], column 1 ("b") is on the left [100..300].
  // Cross-axis is container height [0..300].
  protocol::ListValue* area_a = lanes_area_names->getArray("a");
  EXPECT_TRUE(area_a);
  EXPECT_EQ(SerializeToJson(*area_a),
            "[\"M\",400,0,\"L\",300,0,\"L\",300,300,\"L\",400,300,\"Z\"]");

  protocol::ListValue* area_b = lanes_area_names->getArray("b");
  EXPECT_TRUE(area_b);
  EXPECT_EQ(SerializeToJson(*area_b),
            "[\"M\",300,0,\"L\",100,0,\"L\",100,300,\"L\",300,300,\"Z\"]");

  // Test row lanes (is_for_columns = false):
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
    body {
      margin: 0;
      padding: 0;
    }
    #row_lanes {
      display: grid-lanes;
      direction: rtl;
      width: 400px;
      height: 300px;
      grid-template-rows: 100px 150px;
      grid-template-areas:
        "top"
        "bottom";
    }
    </style>
    <div id="row_lanes"></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  Node* row_lanes = GetDocument().getElementById(AtomicString("row_lanes"));
  EXPECT_TRUE(row_lanes);
  auto row_lanes_info =
      InspectorGridHighlight(row_lanes, InspectorHighlight::DefaultGridConfig());
  EXPECT_TRUE(row_lanes_info);
  protocol::DictionaryValue* row_lanes_area_names =
      row_lanes_info->getObject("areaNames");
  EXPECT_TRUE(row_lanes_area_names);
  EXPECT_EQ(row_lanes_area_names->size(), 2u);

  // Cross-axis is container width [0..400]. Rows are [0..100] and [100..250].
  protocol::ListValue* area_top = row_lanes_area_names->getArray("top");
  EXPECT_TRUE(area_top);
  EXPECT_EQ(SerializeToJson(*area_top),
            "[\"M\",0,0,\"L\",400,0,\"L\",400,100,\"L\",0,100,\"Z\"]");

  protocol::ListValue* area_bottom = row_lanes_area_names->getArray("bottom");
  EXPECT_TRUE(area_bottom);
  EXPECT_EQ(SerializeToJson(*area_bottom),
            "[\"M\",0,100,\"L\",400,100,\"L\",400,250,\"L\",0,250,\"Z\"]");
}

TEST_F(InspectorHighlightTest, FieldsetGrid) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
    #grid {
      display: grid;
      width: 200px;
      height: 200px;
      grid-template-columns: 1fr 1fr;
      grid-template-rows: 1fr 1fr;
    }
    </style>
    <fieldset id="grid">
      <legend>legend</legend>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
    </fieldset>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Node* grid = GetDocument().getElementById(AtomicString("grid"));
  EXPECT_TRUE(grid);
  auto info =
      InspectorGridHighlight(grid, InspectorHighlight::DefaultGridConfig());
  EXPECT_TRUE(info);
  EXPECT_TRUE(info->get("gridBorder"));
  EXPECT_EQ(2u, info->getArray("rowTrackSizes")->size());
  EXPECT_EQ(2u, info->getArray("columnTrackSizes")->size());
}

TEST_F(InspectorHighlightTest, GridTrackSizesIgnoreDeviceScaleFactor) {
  auto* chrome_client = MakeGarbageCollected<ScaledChromeClient>(2.f);
  auto page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600), chrome_client);
  Document& document = page_holder->GetDocument();

  document.body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
    #grid {
      display: grid;
      width: 200px;
      height: 200px;
      grid-template-columns: 100px 100px;
      grid-template-rows: 100px 100px;
    }
    </style>
    <div id="grid">
      <div></div>
      <div></div>
      <div></div>
      <div></div>
    </div>
  )HTML");
  document.View()->UpdateAllLifecyclePhasesForTest();
  Node* grid = document.getElementById(AtomicString("grid"));
  EXPECT_TRUE(grid);
  auto info =
      InspectorGridHighlight(grid, InspectorHighlight::DefaultGridConfig());
  EXPECT_TRUE(info);

  auto ExpectComputedSizes = [](protocol::ListValue* track_sizes,
                                double expected_size) {
    ASSERT_TRUE(track_sizes);
    for (wtf_size_t i = 0; i < track_sizes->size(); ++i) {
      protocol::DictionaryValue* track_size =
          static_cast<protocol::DictionaryValue*>(track_sizes->at(i));
      double computed_size = 0;
      EXPECT_TRUE(track_size->getDouble("computedSize", &computed_size));
      EXPECT_DOUBLE_EQ(expected_size, computed_size);
    }
  };

  ExpectComputedSizes(info->getArray("rowTrackSizes"), 100);
  ExpectComputedSizes(info->getArray("columnTrackSizes"), 100);
}

TEST_F(InspectorHighlightTest, FieldsetFlex) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
    #flex {
      display: flex;
      width: 200px;
      height: 200px;
    }
    </style>
    <fieldset id="flex">
      <legend>legend</legend>
      <div></div>
      <div></div>
    </fieldset>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* flex = GetDocument().getElementById(AtomicString("flex"));
  EXPECT_TRUE(flex);
  auto info = InspectorFlexContainerHighlight(
      flex, InspectorHighlight::DefaultFlexContainerConfig());
  EXPECT_TRUE(info);
  EXPECT_TRUE(info->get("containerBorder"));
  EXPECT_EQ(1u, info->getArray("lines")->size());
}

TEST_F(InspectorHighlightTest, OffsetPathHighlight) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      body {
        margin: 0;
        padding: 0;
      }
      div {
        width: 50px;
        height: 50px;
        offset-rotate: 0deg;
        offset-path: circle(100px at 150px 150px);
        offset-distance: 0;
        position: relative;
        left: 50px;
        box-sizing: border-box;
      }
    </style>
    <div>test</div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* element = GetDocument().QuerySelector(AtomicString("div"));
  EXPECT_TRUE(element);

  std::unique_ptr<protocol::DOM::BoxModel> model;
  EXPECT_TRUE(InspectorHighlight::GetBoxModel(element, &model, false));

  const auto& content_quad = *model->getContent();
  EXPECT_NEAR(225, content_quad[0], 0.1);
  EXPECT_NEAR(125, content_quad[1], 0.1);
  EXPECT_NEAR(275, content_quad[2], 0.1);
  EXPECT_NEAR(125, content_quad[3], 0.1);
  EXPECT_NEAR(275, content_quad[4], 0.1);
  EXPECT_NEAR(175, content_quad[5], 0.1);
  EXPECT_NEAR(225, content_quad[6], 0.1);
  EXPECT_NEAR(175, content_quad[7], 0.1);
}

TEST_F(InspectorHighlightTest, ShapeOutsidePathHighlightUsesPath) {
  ScopedCSSShapeOutsidePathAndShapeSupportForTest enable_path_and_shape(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      body {
        margin: 0;
      }
      #target {
        float: left;
        width: 100px;
        height: 100px;
        shape-outside: path("M 0 0 C 20 40 80 40 100 0 L 100 100 L 0 100 Z");
      }
    </style>
    <div id="target"></div>
    <p>text</p>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_TRUE(target);

  std::unique_ptr<protocol::DOM::BoxModel> model;
  ASSERT_TRUE(InspectorHighlight::GetBoxModel(target, &model, false));

  const std::string json = SerializeToJson(*model);
  EXPECT_THAT(json, testing::HasSubstr("\"shapeOutside\""));
  EXPECT_THAT(json, testing::HasSubstr("\"C\""));
  EXPECT_THAT(
      json, testing::HasSubstr("\"shape\":[\"M\",0,0,\"C\",20,40,80,40,100,0"));
}

TEST_F(InspectorHighlightTest, ShapeOutsidePathHighlightIsNotMirroredInRtl) {
  ScopedCSSShapeOutsidePathAndShapeSupportForTest enable_path_and_shape(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      body {
        margin: 0;
      }
      #container {
        direction: rtl;
        width: 300px;
      }
      #target {
        float: right;
        width: 100px;
        height: 100px;
        shape-outside: path("M 0 0 L 100 0 L 100 100 Z");
      }
    </style>
    <div id="container">
      <div id="target"></div>
      <p>text</p>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_TRUE(target);

  InspectorHighlightConfig config = InspectorHighlight::DefaultConfig();
  InspectorHighlightContrastInfo contrast_info;
  InspectorHighlight highlight(target, config, contrast_info,
                               /*append_element_info=*/false,
                               /*append_distance_info=*/false,
                               NodeContentVisibilityState::kNone);

  const std::string json = SerializeToJson(*highlight.AsProtocolValue());
  EXPECT_THAT(json, testing::HasSubstr(
                        "\"path\":[\"M\",200,0,\"L\",300,0,\"L\",300,100"));
}

TEST_F(InspectorHighlightTest, ShapeOutsideHighlightScalesAfterTranslation) {
  ScopedCSSShapeOutsidePathAndShapeSupportForTest enable_path_and_shape(true);

  auto* chrome_client = MakeGarbageCollected<ScaledChromeClient>(2.f);
  auto page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600), chrome_client);
  Document& document = page_holder->GetDocument();

  document.body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      body {
        margin: 10px;
      }
      #target {
        float: left;
        width: 100px;
        height: 100px;
        shape-outside: path("M 0 0 L 100 0 L 100 100 Z");
      }
    </style>
    <div id="target"></div>
    <p>text</p>
  )HTML");
  document.View()->UpdateAllLifecyclePhasesForTest();
  Element* target = document.getElementById(AtomicString("target"));
  EXPECT_TRUE(target);

  InspectorHighlightConfig config = InspectorHighlight::DefaultConfig();
  InspectorHighlightContrastInfo contrast_info;
  InspectorHighlight highlight(target, config, contrast_info,
                               /*append_element_info=*/false,
                               /*append_distance_info=*/false,
                               NodeContentVisibilityState::kNone);

  const std::string json = SerializeToJson(*highlight.AsProtocolValue());
  EXPECT_THAT(json, testing::HasSubstr("\"path\":[\"M\",5,5"));
}

TEST_F(InspectorHighlightTest, CanvasInlineChildHighlight) {
  ScopedCanvasDrawElementForTest forced_canvas_draw_element_feature(true);
  ScopedElementCanvasTransformForTest forced_canvas_transform_feature(true);

  PageTestBase::LoadAhem(*GetDocument().GetFrame());

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      body { margin: 0; }
      canvas {
        width: 200px;
        height: 200px;
      }
      #drawable-container {
        padding-left: 100px;
      }
      span {
        font: 25px/1 Ahem;
      }
    </style>
    <canvas id="canvas" layoutsubtree>
      <div drawable id="drawable-container">
        <span id="a">X</span><span id="drawable-span" drawable>X</span>
      </div>
    </canvas>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Set canvas transform on #drawable-span to (125, 0) which is its layout
  // position (100px padding + 25px for #a).
  Element* span = GetDocument().getElementById(AtomicString("drawable-span"));
  span->SetCanvasTransform(gfx::Transform::MakeTranslation(125, 0));
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  InspectorHighlightConfig config = InspectorHighlight::DefaultConfig();
  InspectorHighlightContrastInfo contrast_info;
  InspectorHighlight highlight(span, config, contrast_info,
                               /*append_element_info=*/false,
                               /*append_distance_info=*/false,
                               NodeContentVisibilityState::kNone);

  const std::string json = SerializeToJson(*highlight.AsProtocolValue());
  // The highlight path for #drawable-span should start at 125,0 and have lines
  // to 150,0, 150,25, 125,25, and finally back to 125,0 with Z.
  EXPECT_THAT(
      json,
      testing::HasSubstr(
          "\"path\":[\"M\",125,0,\"L\",150,0,\"L\",150,25,\"L\",125,25,\"Z\""));
}

}  // namespace blink
