// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"

#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/timing/media_record_id.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

namespace internal {
extern bool IsExplicitlyRegisteredForTiming(const LayoutObject& layout_object);
}

class ImageElementTimingTest : public testing::Test,
                               public PaintTestConfigurations {
 protected:
  void SetUp() override {
    web_view_helper_.Initialize();
    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(), "about:blank");
    base_url_ = url_test_helpers::ToKURL("http://www.test.com/");
    // Enable compositing on the page.
    web_view_helper_.GetWebView()
        ->GetPage()
        ->GetSettings()
        .SetAcceleratedCompositingEnabled(true);
    GetDoc()->View()->SetParentVisible(true);
    GetDoc()->View()->SetSelfVisible(true);
  }

  // Sets an image resource for the LayoutImage with the given |id| and return
  // the LayoutImage.
  LayoutImage* SetImageResource(const char* id, int width, int height) {
    ImageResourceContent* content = CreateImageForTest(width, height);
    if (auto* layout_image = DynamicTo<LayoutImage>(GetLayoutObjectById(id))) {
      layout_image->ImageResource()->SetImageResource(content);
      return layout_image;
    }
    return nullptr;
  }

  // Similar to above but for a LayoutSVGImage.
  LayoutSVGImage* SetSVGImageResource(const char* id, int width, int height) {
    ImageResourceContent* content = CreateImageForTest(width, height);
    if (auto* layout_image =
            DynamicTo<LayoutSVGImage>(GetLayoutObjectById(id))) {
      layout_image->ImageResource()->SetImageResource(content);
      return layout_image;
    }
    return nullptr;
  }

  bool ImagesNotifiedContains(MediaRecordIdHash record_id_hash) {
    return ImageElementTiming::From(*GetDoc()->domWindow())
        .images_notified_.Contains(record_id_hash);
  }

  unsigned ImagesNotifiedSize() {
    return ImageElementTiming::From(*GetDoc()->domWindow())
        .images_notified_.size();
  }

  Document* GetDoc() {
    return web_view_helper_.GetWebView()
        ->MainFrameImpl()
        ->GetFrame()
        ->GetDocument();
  }

  LayoutObject* GetLayoutObjectById(const char* id) {
    return GetDoc()->getElementById(AtomicString(id))->GetLayoutObject();
  }

  void UpdateAllLifecyclePhases() {
    web_view_helper_.GetWebView()
        ->MainFrameImpl()
        ->GetFrame()
        ->View()
        ->UpdateAllLifecyclePhasesForTest();
  }

  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  WebURL base_url_;

 private:
  ImageResourceContent* CreateImageForTest(int width, int height) {
    sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
    SkImageInfo raster_image_info =
        SkImageInfo::MakeN32Premul(width, height, src_rgb_color_space);
    sk_sp<SkSurface> surface(SkSurfaces::Raster(raster_image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    ImageResourceContent* original_image_content =
        ImageResourceContent::CreateLoaded(
            UnacceleratedStaticBitmapImage::Create(image).get());
    return original_image_content;
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(ImageElementTimingTest);

TEST_P(ImageElementTimingTest, TestIsExplicitlyRegisteredForTiming) {
  frame_test_helpers::LoadHTMLString(
      web_view_helper_.GetWebView()->MainFrameImpl(), R"HTML(
    <img id="missing-attribute" style='width: 100px; height: 100px;'/>
    <img id="unset-attribute" elementtiming
         style='width: 100px; height: 100px;'/>
    <img id="empty-attribute" elementtiming=""
         style='width: 100px; height: 100px;'/>
    <img id="valid-attribute" elementtiming="valid-id"
         style='width: 100px; height: 100px;'/>
  )HTML",
      base_url_);

  LayoutObject* without_attribute = GetLayoutObjectById("missing-attribute");
  bool actual = internal::IsExplicitlyRegisteredForTiming(*without_attribute);
  EXPECT_FALSE(actual) << "Nodes without an 'elementtiming' attribute should "
                          "not be explicitly registered.";

  LayoutObject* with_undefined_attribute =
      GetLayoutObjectById("unset-attribute");
  actual = internal::IsExplicitlyRegisteredForTiming(*with_undefined_attribute);
  EXPECT_TRUE(actual) << "Nodes with undefined 'elementtiming' attribute "
                         "should be explicitly registered.";

  LayoutObject* with_empty_attribute = GetLayoutObjectById("empty-attribute");
  actual = internal::IsExplicitlyRegisteredForTiming(*with_empty_attribute);
  EXPECT_TRUE(actual) << "Nodes with an empty 'elementtiming' attribute "
                         "should be explicitly registered.";

  LayoutObject* with_explicit_element_timing =
      GetLayoutObjectById("valid-attribute");
  actual =
      internal::IsExplicitlyRegisteredForTiming(*with_explicit_element_timing);
  EXPECT_TRUE(actual) << "Nodes with a non-empty 'elementtiming' attribute "
                         "should be explicitly registered.";
}

TEST_P(ImageElementTimingTest, IgnoresUnmarkedElement) {
  // Tests that, if the 'elementtiming' attribute is missing, the element isn't
  // considered by ImageElementTiming.
  frame_test_helpers::LoadHTMLString(
      web_view_helper_.GetWebView()->MainFrameImpl(), R"HTML(
    <img id="target" style='width: 100px; height: 100px;'/>
  )HTML",
      base_url_);
  LayoutImage* layout_image = SetImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(ImagesNotifiedContains(
      MediaRecordId::GenerateHash(layout_image, layout_image->CachedImage())));
}

TEST_P(ImageElementTimingTest, ImageInsideSVG) {
  frame_test_helpers::LoadHTMLString(
      web_view_helper_.GetWebView()->MainFrameImpl(), R"HTML(
    <svg>
      <foreignObject width="100" height="100">
        <img elementtiming="image-inside-svg" id="target"
             style='width: 100px; height: 100px;'/>
      </foreignObject>
    </svg>
  )HTML",
      base_url_);
  LayoutImage* layout_image = SetImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  UpdateAllLifecyclePhases();

  // |layout_image| should have had its paint notified to ImageElementTiming.
  EXPECT_TRUE(ImagesNotifiedContains(
      MediaRecordId::GenerateHash(layout_image, layout_image->CachedImage())));
}

TEST_P(ImageElementTimingTest, ImageInsideNonRenderedSVG) {
  frame_test_helpers::LoadHTMLString(
      web_view_helper_.GetWebView()->MainFrameImpl(), R"HTML(
    <svg mask="url(#mask)">
      <mask id="mask">
        <foreignObject width="100" height="100">
          <img elementtiming="image-inside-svg" id="target"
               style='width: 100px; height: 100px;'/>
        </foreignObject>
      </mask>
      <rect width="100" height="100" fill="green"/>
    </svg>
  )HTML",
      base_url_);

  // HTML inside foreignObject in a non-rendered SVG subtree should not generate
  // layout objects. Generating layout objects for caused crashes
  // (crbug.com/905850) as well as correctness issues.
  EXPECT_FALSE(GetLayoutObjectById("target"));
}

TEST_P(ImageElementTimingTest, ImageRemoved) {
  frame_test_helpers::LoadHTMLString(
      web_view_helper_.GetWebView()->MainFrameImpl(), R"HTML(
    <img elementtiming="will-be-removed" id="target"
         style='width: 100px; height: 100px;'/>
  )HTML",
      base_url_);
  LayoutImage* layout_image = SetImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(ImagesNotifiedContains(
      MediaRecordId::GenerateHash(layout_image, layout_image->CachedImage())));

  GetDoc()->getElementById(AtomicString("target"))->remove();
  // |layout_image| should no longer be part of |images_notified| since it will
  // be destroyed.
  EXPECT_EQ(ImagesNotifiedSize(), 0u);
}

TEST_P(ImageElementTimingTest, SVGImageRemoved) {
  frame_test_helpers::LoadHTMLString(
      web_view_helper_.GetWebView()->MainFrameImpl(), R"HTML(
    <svg>
      <image elementtiming="svg-will-be-removed" id="target"
             style='width: 100px; height: 100px;'/>
    </svg>
  )HTML",
      base_url_);
  LayoutSVGImage* layout_image = SetSVGImageResource("target", 5, 5);
  ASSERT_TRUE(layout_image);
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(ImagesNotifiedContains(MediaRecordId::GenerateHash(
      layout_image, layout_image->ImageResource()->CachedImage())));

  GetDoc()->getElementById(AtomicString("target"))->remove();
  // |layout_image| should no longer be part of |images_notified| since it will
  // be destroyed.
  EXPECT_EQ(ImagesNotifiedSize(), 0u);
}

TEST_P(ImageElementTimingTest, BackgroundImageRemoved) {
  frame_test_helpers::LoadHTMLString(
      web_view_helper_.GetWebView()->MainFrameImpl(), R"HTML(
    <style>
      #target {
        width: 100px;
        height: 100px;
        background: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
    </style>
    <div elementtiming="time-my-background-image" id="target"></div>
  )HTML",
      base_url_);
  LayoutObject* object = GetLayoutObjectById("target");
  ImageResourceContent* content =
      object->Style()->BackgroundLayers().GetImage()->CachedImage();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(ImagesNotifiedSize(), 1u);
  EXPECT_TRUE(
      ImagesNotifiedContains(MediaRecordId::GenerateHash(object, content)));

  GetDoc()->getElementById(AtomicString("target"))->remove();
  EXPECT_EQ(ImagesNotifiedSize(), 0u);
}

}  // namespace blink
