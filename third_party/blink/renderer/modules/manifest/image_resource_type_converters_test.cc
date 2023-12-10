// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/image_resource_type_converters.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_resource.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

namespace {

using Purpose = blink::mojom::blink::ManifestImageResource::Purpose;
using blink::mojom::blink::ManifestImageResource;
using blink::mojom::blink::ManifestImageResourcePtr;

TEST(ImageResourceConverter, EmptySizesTest) {
  blink::test::TaskEnvironment task_environment;
  blink::ManifestImageResource* resource =
      blink::ManifestImageResource::Create();

  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->sizes.empty());

  // Explicitly set to empty.
  resource->setSizes("");
  converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->sizes.empty());
}

TEST(ImageResourceConverter, ValidSizesTest) {
  blink::test::TaskEnvironment task_environment;
  blink::ManifestImageResource* resource =
      blink::ManifestImageResource::Create();

  resource->setSizes("2x3");
  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_EQ(converted->sizes.size(), 1u);
  EXPECT_EQ(converted->sizes.front(), gfx::Size(2, 3));

  resource->setSizes("42X24");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(converted->sizes.size(), 1u);
  EXPECT_EQ(converted->sizes.front(), gfx::Size(42, 24));

  resource->setSizes("any");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(converted->sizes.size(), 1u);
  EXPECT_EQ(converted->sizes.front(), gfx::Size(0, 0));

  resource->setSizes("ANY");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(converted->sizes.size(), 1u);
  EXPECT_EQ(converted->sizes.front(), gfx::Size(0, 0));

  resource->setSizes("2x2 4x4");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(converted->sizes.size(), 2u);
  EXPECT_EQ(converted->sizes.front(), gfx::Size(2, 2));
  EXPECT_EQ(converted->sizes.back(), gfx::Size(4, 4));

  resource->setSizes("2x2 4x4 2x2");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(2u, converted->sizes.size());
  EXPECT_EQ(gfx::Size(2, 2), converted->sizes.front());
  EXPECT_EQ(gfx::Size(4, 4), converted->sizes.back());

  resource->setSizes(" 2x2 any");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(2u, converted->sizes.size());
  EXPECT_EQ(gfx::Size(2, 2), converted->sizes.front());
  EXPECT_EQ(gfx::Size(0, 0), converted->sizes.back());
}

TEST(ImageResourceConverter, InvalidSizesTest) {
  blink::test::TaskEnvironment task_environment;
  blink::ManifestImageResource* resource =
      blink::ManifestImageResource::Create();

  resource->setSizes("02x3");
  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->sizes.empty());

  resource->setSizes("42X024");
  converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->sizes.empty());

  resource->setSizes("42x");
  converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->sizes.empty());

  resource->setSizes("foo");
  converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->sizes.empty());
}

TEST(ImageResourceConverter, EmptyPurposeTest) {
  blink::test::TaskEnvironment task_environment;
  blink::ManifestImageResource* resource =
      blink::ManifestImageResource::Create();

  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->purpose.empty());

  // Explicitly set to empty.
  resource->setPurpose("");
  converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->purpose.empty());
}

TEST(ImageResourceConverter, ValidPurposeTest) {
  blink::test::TaskEnvironment task_environment;
  blink::ManifestImageResource* resource =
      blink::ManifestImageResource::Create();

  resource->setPurpose("any");
  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_EQ(1u, converted->purpose.size());
  ASSERT_EQ(Purpose::ANY, converted->purpose.front());

  resource->setPurpose(" Monochrome");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(1u, converted->purpose.size());
  ASSERT_EQ(Purpose::MONOCHROME, converted->purpose.front());

  resource->setPurpose(" Monochrome  AnY");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(2u, converted->purpose.size());
  ASSERT_EQ(Purpose::MONOCHROME, converted->purpose.front());
  ASSERT_EQ(Purpose::ANY, converted->purpose.back());

  resource->setPurpose("any monochrome  AnY");
  converted = ManifestImageResource::From(resource);
  ASSERT_EQ(2u, converted->purpose.size());
  ASSERT_EQ(Purpose::ANY, converted->purpose.front());
  ASSERT_EQ(Purpose::MONOCHROME, converted->purpose.back());
}

TEST(ImageResourceConverter, InvalidPurposeTest) {
  blink::test::TaskEnvironment task_environment;
  blink::ManifestImageResource* resource =
      blink::ManifestImageResource::Create();

  resource->setPurpose("any?");
  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->purpose.empty());
}

TEST(ImageResourceConverter, EmptyTypeTest) {
  blink::test::TaskEnvironment task_environment;
  blink::ManifestImageResource* resource =
      blink::ManifestImageResource::Create();

  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->type.empty());

  // Explicitly set to empty.
  resource->setType("");
  converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->type.empty());
}

TEST(ImageResourceConverter, InvalidTypeTest) {
  blink::test::TaskEnvironment task_environment;
  blink::ManifestImageResource* resource =
      blink::ManifestImageResource::Create();

  resource->setType("image/NOTVALID!");
  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  ASSERT_TRUE(converted->type.empty());
}

TEST(ImageResourceConverter, ValidTypeTest) {
  blink::test::TaskEnvironment task_environment;
  blink::ManifestImageResource* resource =
      blink::ManifestImageResource::Create();

  resource->setType("image/jpeg");
  ManifestImageResourcePtr converted = ManifestImageResource::From(resource);
  EXPECT_EQ("image/jpeg", converted->type);
}

TEST(ImageResourceConverter, ExampleValueTest) {
  blink::test::TaskEnvironment task_environment;
  blink::ManifestImageResource* resource =
      blink::ManifestImageResource::Create();
  resource->setSrc("http://example.com/lolcat.jpg");
  resource->setPurpose("MONOCHROME");
  resource->setSizes("32x32 64x64 128x128");
  resource->setType("image/jpeg");

  auto expected_resource = ManifestImageResource::New();
  expected_resource->src = blink::KURL("http://example.com/lolcat.jpg");
  expected_resource->purpose = {Purpose::MONOCHROME};
  expected_resource->sizes = {{32, 32}, {64, 64}, {128, 128}};
  expected_resource->type = "image/jpeg";

  EXPECT_EQ(expected_resource, ManifestImageResource::From(resource));
}

TEST(ImageResourceConverter, BlinkToMojoTypeTest) {
  blink::test::TaskEnvironment task_environment;
  blink::ManifestImageResource* icon = blink::ManifestImageResource::Create();
  icon->setSrc("http://example.com/lolcat.jpg");
  icon->setPurpose("MONOCHROME");
  icon->setSizes("32x32 64x64 128x128");
  icon->setType("image/jpeg");

  blink::Manifest::ImageResource mojo_icon =
      blink::ConvertManifestImageResource(icon);
  EXPECT_EQ(mojo_icon.src.spec(), "http://example.com/lolcat.jpg");
  EXPECT_EQ(mojo_icon.type, blink::WebString("image/jpeg").Utf16());
  EXPECT_EQ(mojo_icon.sizes[1], gfx::Size(64, 64));
  EXPECT_EQ(mojo_icon.purpose[0],
            blink::mojom::ManifestImageResource_Purpose::MONOCHROME);
}

}  // namespace

}  // namespace mojo
