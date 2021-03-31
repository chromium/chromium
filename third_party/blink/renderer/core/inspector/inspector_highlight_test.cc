// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_highlight.h"

#include "base/test/values_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
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
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(IntSize(800, 600));
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

}  // namespace blink
