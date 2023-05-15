// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_image_element.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

namespace {

class TestFrameClient : public EmptyLocalFrameClient {
 public:
  void OnMainFrameImageAdRectangleChanged(
      DOMNodeId element_id,
      const gfx::Rect& image_ad_rect) override {
    observed_image_ad_rects_.emplace_back(element_id, image_ad_rect);
  }

  const std::vector<std::pair<DOMNodeId, gfx::Rect>>& observed_image_ad_rects()
      const {
    return observed_image_ad_rects_;
  }

 private:
  std::vector<std::pair<DOMNodeId, gfx::Rect>> observed_image_ad_rects_;
};

}  // namespace

class HTMLImageElementTest : public PageTestBase {
 protected:
  static constexpr int kViewportWidth = 500;
  static constexpr int kViewportHeight = 600;

  void SetUp() override {
    test_frame_client_ = MakeGarbageCollected<TestFrameClient>();

    PageTestBase::SetupPageWithClients(
        nullptr, test_frame_client_.Get(), nullptr,
        gfx::Size(kViewportWidth, kViewportHeight));
  }

  Persistent<TestFrameClient> test_frame_client_;
};

// Instantiate class constants. Not needed after C++17.
constexpr int HTMLImageElementTest::kViewportWidth;
constexpr int HTMLImageElementTest::kViewportHeight;

TEST_F(HTMLImageElementTest, width) {
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image->setAttribute(html_names::kWidthAttr, "400");
  // TODO(yoav): `width` does not impact resourceWidth until we resolve
  // https://github.com/ResponsiveImagesCG/picture-element/issues/268
  EXPECT_EQ(absl::nullopt, image->GetResourceWidth());
  image->setAttribute(html_names::kSizesAttr, "100vw");
  EXPECT_EQ(500, image->GetResourceWidth());
}

TEST_F(HTMLImageElementTest, sourceSize) {
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image->setAttribute(html_names::kWidthAttr, "400");
  EXPECT_EQ(kViewportWidth, image->SourceSize(*image));
  image->setAttribute(html_names::kSizesAttr, "50vw");
  EXPECT_EQ(250, image->SourceSize(*image));
}

TEST_F(HTMLImageElementTest, ImageAdRectangleUpdate) {
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML(R"HTML(
    <img id="target"
         style="position:absolute;top:5px;left:5px;width:10px;height:10px;">
    </img>

    <p style="position:absolute;top:10000px;">abc</p>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("target"));
  image->SetIsAdRelated();

  EXPECT_TRUE(test_frame_client_->observed_image_ad_rects().empty());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(test_frame_client_->observed_image_ad_rects().size(), 1u);
  DOMNodeId id = test_frame_client_->observed_image_ad_rects()[0].first;
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[0].second,
            gfx::Rect(5, 5, 10, 10));

  // Scrolling won't trigger another notification, as the rectangle hasn't
  // changed relative to the page.
  {
    auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
    script->setTextContent(R"JS(
      window.scroll(0, 100);
    )JS");
    GetDocument().body()->appendChild(script);
    UpdateAllLifecyclePhasesForTest();
  }

  EXPECT_EQ(test_frame_client_->observed_image_ad_rects().size(), 1u);

  // Update the size to 1x1. A new notification is expected to signal the
  // removal of the element.
  {
    auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
    script->setTextContent(R"JS(
      var image = document.getElementById('target');
      image.style.width = '1px';
      image.style.height = '1px';
    )JS");
    GetDocument().body()->appendChild(script);
    UpdateAllLifecyclePhasesForTest();
  }

  EXPECT_EQ(test_frame_client_->observed_image_ad_rects().size(), 2u);
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[1].first, id);
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[1].second,
            gfx::Rect());

  // Update the size to 30x30. A new notification is expected to signal the new
  // rectangle.
  {
    auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
    script->setTextContent(R"JS(
      var image = document.getElementById('target');
      image.style.width = '30px';
      image.style.height = '30px';
    )JS");
    GetDocument().body()->appendChild(script);
    UpdateAllLifecyclePhasesForTest();
  }

  EXPECT_EQ(test_frame_client_->observed_image_ad_rects().size(), 3u);
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[2].first, id);
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[2].second,
            gfx::Rect(5, 5, 30, 30));

  // Remove the element. A new notification is expected to signal the removal of
  // the element.
  {
    auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
    script->setTextContent(R"JS(
      var image = document.getElementById('target');
      image.remove()
    )JS");
    GetDocument().body()->appendChild(script);
    UpdateAllLifecyclePhasesForTest();
  }

  EXPECT_EQ(test_frame_client_->observed_image_ad_rects().size(), 4u);
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[3].first, id);
  EXPECT_EQ(test_frame_client_->observed_image_ad_rects()[3].second,
            gfx::Rect());
}

}  // namespace blink
