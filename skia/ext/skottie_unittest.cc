// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/modules/skottie/include/Skottie.h"

TEST(Skottie, Basic) {
  // Just a solid green layer.
  static constexpr char anim_data[] =
      "{"
      "  \"v\" : \"4.12.0\","
      "  \"fr\": 30,"
      "  \"w\" : 400,"
      "  \"h\" : 200,"
      "  \"ip\": 0,"
      "  \"op\": 150,"
      "  \"assets\": [],"

      "  \"layers\": ["
      "    {"
      "      \"ty\": 1,"
      "      \"sw\": 400,"
      "      \"sh\": 200,"
      "      \"sc\": \"#00ff00\","
      "      \"ip\": 0,"
      "      \"op\": 150"
      "    }"
      "  ]"
      "}";

  SkMemoryStream stream(anim_data, strlen(anim_data));
  auto anim = skottie::Animation::Make(&stream);

  ASSERT_TRUE(anim);
  EXPECT_EQ(strcmp(anim->version().c_str(), "4.12.0"), 0);
  EXPECT_EQ(anim->size().width(), 400.0f);
  EXPECT_EQ(anim->size().height(), 200.0f);
  EXPECT_EQ(anim->duration(), 5.0f);

  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 200));
  anim->seek(0);
  anim->render(surface->getCanvas());

  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));

  for (int i = 0; i < pixmap.width(); ++i) {
    for (int j = 0; j < pixmap.height(); ++j) {
      EXPECT_EQ(pixmap.getColor(i, j), 0xff00ff00);
    }
  }
}
