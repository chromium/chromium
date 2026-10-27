// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_object_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class HTMLObjectElementTest : public testing::Test {
 protected:
  void SetUp() final {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(HTMLObjectElementTest, FallbackRecalcForReattach) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <object id='obj' data='dummy'></object>
  )HTML");

  auto* object =
      To<HTMLObjectElement>(GetDocument().getElementById(AtomicString("obj")));
  ASSERT_TRUE(object);

  Element* slot = object->GetShadowRoot()->firstElementChild();
  ASSERT_TRUE(slot);

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  object->RenderFallbackContent(HTMLObjectElement::ErrorEventPolicy::kDispatch);
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetDocument().GetStyleEngine().RecalcStyle();
  EXPECT_TRUE(IsA<HTMLSlotElement>(slot));
  EXPECT_TRUE(object->UseFallbackContent());
  EXPECT_TRUE(object->GetComputedStyle());
  EXPECT_TRUE(slot->GetComputedStyle());
}

class HTMLObjectElementSimTest : public SimTest {};

TEST_F(HTMLObjectElementSimTest, NoLoadOnEnsureComputedStyle) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
     <!doctype html>
     <div style='display: none'>
       <object id='obj' data='file.png' type='image/png'></object>
     </div>
  )HTML");

  Element* object = GetElementById("obj");
  ASSERT_TRUE(object);
  EXPECT_FALSE(object->GetComputedStyle());
  object->EnsureComputedStyle();
  // Pass if no crash (URLLoaderMock DCHECK failure) because of a request being
  // performed for 'file.png'.
}

TEST_F(HTMLObjectElementSimTest, NoSyncLoadInCustomStyleForLayoutObject) {
  SimRequest request("https://example.com/test.html", "text/html");
  SimSubresourceRequest image_request("https://example.com/file.png",
                                      "image/png");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
     <!doctype html>
     <div id='container' style='display: none'>
       <object id='obj' data='file.png' type='image/png'></object>
     </div>
  )HTML");

  Element* container = GetElementById("container");
  ASSERT_TRUE(container);
  container->removeAttribute(html_names::kStyleAttr);
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_EQ(0, GetDocument().Fetcher()->BlockingRequestCount());

  test::RunPendingTasks();
  EXPECT_EQ(1, GetDocument().Fetcher()->BlockingRequestCount());
  image_request.Complete("");
}

}  // namespace blink
