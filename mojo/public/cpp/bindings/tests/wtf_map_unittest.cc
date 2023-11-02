// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/rect_blink.h"
#include "mojo/public/interfaces/bindings/tests/rect.mojom-blink.h"
#include "mojo/public/interfaces/bindings/tests/test_structs.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

TEST(WTFMapTest, StructKey) {
  WTF::HashMap<blink::RectPtr, int32_t> map;
  map.insert(blink::Rect::New(1, 2, 3, 4), 123);

  blink::RectPtr key = blink::Rect::New(1, 2, 3, 4);
  ASSERT_NE(map.end(), map.find(key));
  ASSERT_EQ(123, map.find(key)->value);

  map.erase(key);
  ASSERT_EQ(0u, map.size());
}

TEST(WTFMapTest, TypemappedStructKey) {
  WTF::HashMap<blink::ContainsHashablePtr, int32_t> map;
  map.insert(blink::ContainsHashable::New(RectBlink(1, 2, 3, 4)), 123);

  blink::ContainsHashablePtr key =
      blink::ContainsHashable::New(RectBlink(1, 2, 3, 4));
  ASSERT_NE(map.end(), map.find(key));
  ASSERT_EQ(123, map.find(key)->value);

  map.erase(key);
  ASSERT_EQ(0u, map.size());
}

}  // namespace
}  // namespace test
}  // namespace mojo
