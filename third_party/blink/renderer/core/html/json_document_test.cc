// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/json_document.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {
class JSONDocumentTest : public SimTest {
 public:
  void SetUp() override { SimTest::SetUp(); }

  void LoadResource(const String& json) {
    SimRequest request("https://foobar.com", "application/json");
    LoadURL("https://foobar.com");
    request.Complete(json);
    Compositor().BeginFrame();
  }
  void ClickPrettyPrintCheckbox() {
    GetDocument()
        .documentElement()
        ->QuerySelector(html_names::kDivTag.LocalName())
        ->GetShadowRoot()
        ->QuerySelector(html_names::kInputTag.LocalName())
        ->DispatchSimulatedClick(MouseEvent::Create());
  }
};

TEST_F(JSONDocumentTest, JSONDoc) {
  LoadResource(
      "{\"menu\":{\"id\":\"file\",\"value\":\"File\",\"popup\":{\"menuitem\":[{"
      "\"value\":\"New\",\"click\":\"CreateNewDoc\"}]},\"itemCount\":3,"
      "\"isShown\":true}}");
  EXPECT_EQ(
      GetDocument()
          .documentElement()
          ->QuerySelector(html_names::kPreTag.LocalName())
          ->textContent(),
      "{\"menu\":{\"id\":\"file\",\"value\":\"File\",\"popup\":{\"menuitem\":[{"
      "\"value\":\"New\",\"click\":\"CreateNewDoc\"}]},\"itemCount\":3,"
      "\"isShown\":true}}");
  ClickPrettyPrintCheckbox();

  EXPECT_EQ(
      GetDocument()
          .documentElement()
          ->QuerySelector(html_names::kPreTag.LocalName())
          ->textContent(),
      "{\n  \"menu\": {\n    \"id\": \"file\",\n    \"value\": \"File\",\n    "
      "\"popup\": {\n      \"menuitem\": [\n        {\n          \"value\": "
      "\"New\",\n          \"click\": \"CreateNewDoc\"\n        }\n      ]\n   "
      " },\n    \"itemCount\": 3,\n    \"isShown\": true\n  }\n}\n");
}

TEST_F(JSONDocumentTest, InvalidJSON) {
  LoadResource(
      "{\"menu:{\"id\":\"file\",\"value\":\"File\",\"popup\":{\"menuitem\":[{"
      "\"value\":\"New\",\"click\":\"CreateNewDoc\"}]},\"itemCount\":3,"
      "\"isShown\":true}}");
  EXPECT_EQ(
      GetDocument()
          .documentElement()
          ->QuerySelector(html_names::kPreTag.LocalName())
          ->textContent(),
      "{\"menu:{\"id\":\"file\",\"value\":\"File\",\"popup\":{\"menuitem\":[{"
      "\"value\":\"New\",\"click\":\"CreateNewDoc\"}]},\"itemCount\":3,"
      "\"isShown\":true}}");
  ClickPrettyPrintCheckbox();
  EXPECT_EQ(GetDocument()
                .documentElement()
                ->QuerySelector(html_names::kPreTag.LocalName())
                ->textContent(),
            "{\"menu:{\"id\":\"file\",\"value\":\"File\",\"popup\":{"
            "\"menuitem\":[{\"value\":\"New\",\"click\":\"CreateNewDoc\"}]},"
            "\"itemCount\":3,\"isShown\":true}}");
}
}  // namespace blink
