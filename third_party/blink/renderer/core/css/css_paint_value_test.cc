// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_paint_value.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class CSSPaintValueTest : public RenderingTest {
 public:
  void LoadTestData(const std::string& file_name) {
    String testing_path = test::BlinkRootDir();
    testing_path.append("/renderer/core/css/test_data/");
    WebURL url = url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), testing_path,
        WebString::FromUTF8(file_name));
    frame_test_helpers::LoadFrame(helper_.GetWebView()->MainFrameImpl(),
                                  base_url_ + file_name);
    ForceFullCompositingUpdate();
    url_test_helpers::RegisterMockedURLUnregister(url);
  }

  void ForceFullCompositingUpdate() {
    helper_.GetWebView()->UpdateAllLifecyclePhases();
  }

  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
    helper_.Initialize(nullptr, nullptr, nullptr);
    base_url_ = "https://www.test.com/";
  }
  frame_test_helpers::WebViewHelper helper_;
  std::string base_url_;
};

void CheckTargetObject(Document* document) {
  LayoutObject* target_layout_object =
      document->getElementById("target")->GetLayoutObject();
  EXPECT_NE(target_layout_object, nullptr);
  EXPECT_NE(target_layout_object->Style()->InsideLink(),
            EInsideLink::kNotInsideLink);

  CSSPaintValue* css_paint_value =
      CSSPaintValue::Create(CSSCustomIdentValue::Create("linkpainter"));
  EXPECT_EQ(css_paint_value->GetImage(*target_layout_object, *document,
                                      target_layout_object->StyleRef(),
                                      FloatSize(100.0f, 100.0f)),
            nullptr);
}

// Regression test for https://crbug.com/835589.
TEST_F(CSSPaintValueTest, CSSPaintDoNotPaintForLink) {
  LoadTestData("csspaint-do-not-paint-for-link.html");
  Document* document = GetFrame()->GetDocument();
  CheckTargetObject(document);
}

// Regression test for https://crbug.com/835589.
TEST_F(CSSPaintValueTest, CSSPaintDoNotPaintWhenParentHasLink) {
  LoadTestData("csspaint-do-not-paint-for-link-descendant.html");
  Document* document = GetFrame()->GetDocument();
  CheckTargetObject(document);
}

}  // namespace blink
