// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_image_element.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class HTMLImageElementTest : public PageTestBase {
 protected:
  static constexpr int kViewportWidth = 500;
  static constexpr int kViewportHeight = 600;

  void SetUp() override {
    PageTestBase::SetUp(gfx::Size(kViewportWidth, kViewportHeight));
  }
};

// Instantiate class constants. Not needed after C++17.
constexpr int HTMLImageElementTest::kViewportWidth;
constexpr int HTMLImageElementTest::kViewportHeight;

TEST_F(HTMLImageElementTest, width) {
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image->setAttribute(html_names::kWidthAttr, AtomicString("400"));
  // TODO(yoav): `width` does not impact resourceWidth until we resolve
  // https://github.com/ResponsiveImagesCG/picture-element/issues/268
  EXPECT_EQ(std::nullopt, image->GetResourceWidth());
  image->setAttribute(html_names::kSizesAttr, AtomicString("100vw"));
  EXPECT_EQ(500, image->GetResourceWidth());
}

TEST_F(HTMLImageElementTest, sourceSize) {
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image->setAttribute(html_names::kWidthAttr, AtomicString("400"));
  EXPECT_EQ(kViewportWidth, image->SourceSize(*image));
  image->setAttribute(html_names::kSizesAttr, AtomicString("50vw"));
  EXPECT_EQ(250, image->SourceSize(*image));
}

TEST_F(HTMLImageElementTest, ResourceWidthWithPicture) {
  SetBodyInnerHTML(R"HTML(
    <picture>
      <source srcset="a.png" sizes="auto"/>
      <img id="i" width="5" height="5" src="b.png" loading="lazy" sizes="auto"/>
    </picture>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("i"));
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(*image->GetResourceWidth(), 5);
}

TEST_F(HTMLImageElementTest, ResourceWidthWithPictureContainingScripts) {
  SetBodyInnerHTML(R"HTML(
    <picture>
      <source srcset="a.png" sizes="auto"/>
      <script></script>
      <img id="i" width="5" height="5" src="b.png" loading="lazy" sizes="auto"/>
      <script></script>
    </picture>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("i"));
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(*image->GetResourceWidth(), 5);
}

using HTMLImageElementSimTest = SimTest;

TEST_F(HTMLImageElementSimTest, Sharedstoragewritable_SecureContext_Allowed) {
  WebRuntimeFeaturesBase::EnableSharedStorageAPI(true);
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource("https://example.com/foo.png",
                                       "image/png");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"(
    <img src="foo.png" id="target"
      allow="shared-storage"
      sharedstoragewritable></img>
  )");

  image_resource.Complete("image data");
  EXPECT_TRUE(ConsoleMessages().empty());
}

TEST_F(HTMLImageElementSimTest,
       Sharedstoragewritable_InsecureContext_NotAllowed) {
  WebRuntimeFeaturesBase::EnableSharedStorageAPI(true);
  SimRequest main_resource("http://example.com/index.html", "text/html");
  SimSubresourceRequest image_resource("http://example.com/foo.png",
                                       "image/png");
  LoadURL("http://example.com/index.html");
  main_resource.Complete(R"(
    <img src="foo.png" id="target"
      allow="shared-storage"
      sharedstoragewritable></img>
  )");

  image_resource.Complete("image data");
  EXPECT_EQ(ConsoleMessages().size(), 1u);
  EXPECT_TRUE(ConsoleMessages().front().StartsWith(
      "sharedStorageWritable: sharedStorage operations are only available in "
      "secure contexts."))
      << "Expect error that Shared Storage operations are not allowed in "
         "insecure contexts but got: "
      << ConsoleMessages().front();
}

TEST_F(HTMLImageElementSimTest, OnloadTransparentPlaceholderImage) {
  SimRequest main_resource("http://example.com/index.html", "text/html");
  LoadURL("http://example.com/index.html");
  main_resource.Complete(R"(
    <body onload='console.log("main body onload");'>
      <img src="data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="
           onload='console.log("image element onload");'>
    </body>)");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Ensure that both body and image are successfully loaded.
  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image element onload"));
}

TEST_F(HTMLImageElementSimTest, CurrentSrcForTransparentPlaceholderImage) {
  const String image_source =
      "data:image/gif;base64,R0lGODlhAQABAIAAAP///////"
      "yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==";

  SimRequest main_resource("http://example.com/index.html", "text/html");
  LoadURL("http://example.com/index.html");
  main_resource.Complete(R"(
    <img id="myimg" src=)" +
                         image_source + R"(>
    <script>
      console.log(myimg.currentSrc);
    </script>)");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Ensure that currentSrc is correctly set as the image source.
  EXPECT_TRUE(ConsoleMessages().Contains(image_source));
}

class HTMLImageElementUseCounterTest : public HTMLImageElementTest {
 protected:
  bool IsCounted(WebFeature feature) {
    return GetDocument().IsUseCounted(feature);
  }
};

TEST_F(HTMLImageElementUseCounterTest, AutoSizesUseCountersNoSizes) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"
         loading="lazy">
    </img>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("target"));
  ASSERT_NE(image, nullptr);

  EXPECT_FALSE(IsCounted(WebFeature::kAutoSizesLazy));
  EXPECT_FALSE(IsCounted(WebFeature::kAutoSizesNonLazy));
}

TEST_F(HTMLImageElementUseCounterTest, AutoSizesUseCountersNonAutoSizes) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"
         sizes = "33px"
         loading="lazy">
    </img>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("target"));
  ASSERT_NE(image, nullptr);

  EXPECT_FALSE(IsCounted(WebFeature::kAutoSizesLazy));
  EXPECT_FALSE(IsCounted(WebFeature::kAutoSizesNonLazy));
}

TEST_F(HTMLImageElementUseCounterTest, AutoSizesNonLazyUseCounter) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"
         sizes="auto">
    </img>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("target"));
  ASSERT_NE(image, nullptr);

  EXPECT_FALSE(IsCounted(WebFeature::kAutoSizesLazy));
  EXPECT_TRUE(IsCounted(WebFeature::kAutoSizesNonLazy));
}

TEST_F(HTMLImageElementUseCounterTest, AutoSizesLazyUseCounter) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"
         sizes="auto"
         loading="lazy">
    </img>
  )HTML");

  HTMLImageElement* image = To<HTMLImageElement>(GetElementById("target"));
  ASSERT_NE(image, nullptr);

  EXPECT_TRUE(IsCounted(WebFeature::kAutoSizesLazy));
  EXPECT_FALSE(IsCounted(WebFeature::kAutoSizesNonLazy));
}

}  // namespace blink
