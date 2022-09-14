// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/tests/rect_chromium.h"
#include "mojo/public/interfaces/bindings/tests/rect.mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_structs.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

TEST(MapTest, StructKey) {
  base::flat_map<RectPtr, int32_t> map;
  map.insert(std::make_pair(Rect::New(1, 2, 3, 4), 123));

  RectPtr key = Rect::New(1, 2, 3, 4);
  ASSERT_NE(map.end(), map.find(key));
  ASSERT_EQ(123, map.find(key)->second);

  map.erase(key);
  ASSERT_EQ(0u, map.size());
}

TEST(MapTest, TypemappedStructKey) {
  base::flat_map<ContainsHashablePtr, int32_t> map;
  map.insert(
      std::make_pair(ContainsHashable::New(RectChromium(1, 2, 3, 4)), 123));

  ContainsHashablePtr key = ContainsHashable::New(RectChromium(1, 2, 3, 4));
  ASSERT_NE(map.end(), map.find(key));
  ASSERT_EQ(123, map.find(key)->second);

  map.erase(key);
  ASSERT_EQ(0u, map.size());
}

}  // namespace
}  // namespace test
}  // namespace mojo
