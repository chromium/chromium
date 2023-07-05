// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_video.h"

#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class LayoutVideoTest : public RenderingTest {
 public:
  void CreateAndSetImage(const char* id, int width, int height) {
    // Create one image with size(width, height)
    sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
    SkImageInfo raster_image_info =
        SkImageInfo::MakeN32Premul(width, height, src_rgb_color_space);
    sk_sp<SkSurface> surface(SkSurfaces::Raster(raster_image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    ImageResourceContent* image_content = ImageResourceContent::CreateLoaded(
        UnacceleratedStaticBitmapImage::Create(image).get());

    // Set image to video
    auto* video = To<HTMLVideoElement>(GetElementById(id));
    auto* layout_image = To<LayoutImage>(video->GetLayoutObject());
    video->setAttribute(html_names::kPosterAttr,
                        AtomicString("http://example.com/foo.jpg"));
    layout_image->ImageResource()->SetImageResource(image_content);
  }
};

TEST_F(LayoutVideoTest, PosterSizeWithNormal) {
  SetBodyInnerHTML(R"HTML(
    <style>
      video {zoom:1}
    </style>
    <video id='video' />
  )HTML");

  CreateAndSetImage("video", 10, 10);
  UpdateAllLifecyclePhasesForTest();

  int width = To<LayoutBox>(GetLayoutObjectByElementId("video"))
                  ->AbsoluteBoundingBoxRect()
                  .width();
  EXPECT_EQ(width, 10);
}

TEST_F(LayoutVideoTest, PosterSizeWithZoom) {
  SetBodyInnerHTML(R"HTML(
    <style>
      video {zoom:1.5}
    </style>
    <video id='video' />
  )HTML");

  CreateAndSetImage("video", 10, 10);
  UpdateAllLifecyclePhasesForTest();

  int width = To<LayoutBox>(GetLayoutObjectByElementId("video"))
                  ->AbsoluteBoundingBoxRect()
                  .width();
  EXPECT_EQ(width, 15);
}

TEST_F(LayoutVideoTest, PosterSizeAfterPlay) {
  SetBodyInnerHTML(R"HTML(
    <video id='video' src='http://example.com/foo.mp4' />
  )HTML");

  CreateAndSetImage("video", 10, 10);
  UpdateAllLifecyclePhasesForTest();
  auto* video = To<HTMLVideoElement>(GetElementById("video"));

  // Try playing the video (should stall without a real source)
  video->Play();
  EXPECT_FALSE(video->IsShowPosterFlagSet());
  EXPECT_FALSE(video->HasAvailableVideoFrame());

  // Width should still be that of the poster image, NOT the default video
  // element width
  int width = To<LayoutBox>(GetLayoutObjectByElementId("video"))
                  ->AbsoluteBoundingBoxRect()
                  .width();
  EXPECT_EQ(width, 10);
}

// TODO(1190335): Remove this once "default poster image" is not longer
// supported. Blink embedders (such as Webview) can set the default poster image
// for a video using `blink::Settings`. The default poster image should not be
// used to affect the layout of a video, even when a normal poster image would.
TEST_F(LayoutVideoTest, DefaultPosterImageSize) {
  // Override the default poster image
  GetDocument().GetSettings()->SetDefaultVideoPosterURL(
      "https://www.example.com/foo.jpg");

  SetBodyInnerHTML(R"HTML(
    <video id='video' src='http://example.com/foo.mp4' />
  )HTML");

  // Pretend we loaded the poster
  CreateAndSetImage("video", 10, 10);

  // Width should be the default video width, NOT poster image width
  int width = To<LayoutBox>(GetLayoutObjectByElementId("video"))
                  ->AbsoluteBoundingBoxRect()
                  .width();
  EXPECT_NE(width, 10);
  EXPECT_EQ(width, LayoutVideo::kDefaultWidth);
}

}  // namespace blink
