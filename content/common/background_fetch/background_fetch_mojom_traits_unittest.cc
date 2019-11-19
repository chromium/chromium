// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/geometry/size.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace content {

namespace {

// Creates a new ImageResource object for the given arguments.
blink::Manifest::ImageResource CreateIcon(const std::string& src,
                                          std::vector<gfx::Size> sizes,
                                          const std::string& type) {
  blink::Manifest::ImageResource icon;
  icon.src = GURL(src);
  icon.sizes = std::move(sizes);
  icon.type = base::ASCIIToUTF16(type);

  return icon;
}

}  // namespace

TEST(BackgroundFetchStructTraitsTest, ImageResourceRoundtrip) {
  blink::Manifest::ImageResource icon =
      CreateIcon("my_icon.png", {{256, 256}}, "image/png");

  blink::Manifest::ImageResource roundtrip_icon;
  ASSERT_TRUE(blink::mojom::ManifestImageResource::Deserialize(
      blink::mojom::ManifestImageResource::Serialize(&icon), &roundtrip_icon));

  EXPECT_EQ(icon, roundtrip_icon);
}

}  // namespace content
