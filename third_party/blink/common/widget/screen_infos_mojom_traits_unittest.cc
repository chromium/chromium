// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/widget/screen_infos_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/widget/screen_infos.h"
#include "third_party/blink/public/mojom/widget/screen_infos.mojom.h"

namespace {

using mojo::test::SerializeAndDeserialize;

TEST(StructTraitsTest, ScreenInfosRoundtripValid) {
  blink::ScreenInfos i, o;
  i.screen_infos = {blink::ScreenInfo()};
  EXPECT_EQ(i.current_display_id, i.screen_infos[0].display_id);
  ASSERT_TRUE(SerializeAndDeserialize<blink::mojom::ScreenInfos>(i, o));
  EXPECT_EQ(i, o);
}

TEST(StructTraitsTest, ScreenInfosMustBeNonEmpty) {
  // These fail; screen_infos must contain at least one element.
  blink::mojom::ScreenInfosPtr i1 = blink::mojom::ScreenInfos::New();
  blink::ScreenInfos i2, o;
  ASSERT_FALSE(SerializeAndDeserialize<blink::mojom::ScreenInfos>(i1, o));
  EXPECT_DEATH(blink::mojom::ScreenInfos::SerializeAsMessage(&i2), "");
}

TEST(StructTraitsTest, ScreenInfosSelectedDisplayIdMustBePresent) {
  // These fail; current_display_id must match a screen_infos element.
  blink::mojom::ScreenInfosPtr i1 = blink::mojom::ScreenInfos::New();
  blink::ScreenInfos i2, o;
  i1->screen_infos = {blink::ScreenInfo()};
  i1->current_display_id = i1->screen_infos[0].display_id + 123;
  i2.screen_infos = {blink::ScreenInfo()};
  i2.current_display_id = i2.screen_infos[0].display_id + 123;
  EXPECT_NE(i1->current_display_id, i1->screen_infos[0].display_id);
  EXPECT_NE(i2.current_display_id, i2.screen_infos[0].display_id);
  ASSERT_FALSE(SerializeAndDeserialize<blink::mojom::ScreenInfos>(i1, o));
  EXPECT_DEATH(blink::mojom::ScreenInfos::SerializeAsMessage(&i2), "");
}

TEST(StructTraitsTest, ScreenInfosDisplayIdsMustBeUnique) {
  // These fail; screen_infos must use unique display_id values.
  blink::mojom::ScreenInfosPtr i1 = blink::mojom::ScreenInfos::New();
  blink::ScreenInfos i2, o;
  i1->screen_infos = {blink::ScreenInfo(), blink::ScreenInfo()};
  i2.screen_infos = {blink::ScreenInfo(), blink::ScreenInfo()};
  EXPECT_EQ(i1->screen_infos[0].display_id, i1->screen_infos[1].display_id);
  EXPECT_EQ(i2.screen_infos[0].display_id, i2.screen_infos[1].display_id);
  ASSERT_FALSE(SerializeAndDeserialize<blink::mojom::ScreenInfos>(i1, o));
  EXPECT_DEATH(blink::mojom::ScreenInfos::SerializeAsMessage(&i2), "");
}

}  // namespace
