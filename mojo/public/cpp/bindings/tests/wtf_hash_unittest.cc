// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/wtf_hash_util.h"

#include "mojo/public/interfaces/bindings/tests/test_structs.mojom-blink.h"
#include "mojo/public/interfaces/bindings/tests/test_wtf_types.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"

namespace mojo {
namespace test {
namespace {

using WTFHashTest = testing::Test;

TEST_F(WTFHashTest, NestedStruct) {
  // Just check that this template instantiation compiles.
  ASSERT_EQ(::mojo::internal::Hash(
                ::mojo::internal::kHashSeed,
                blink::SimpleNestedStruct::New(blink::ContainsOther::New(1))),
            ::mojo::internal::Hash(
                ::mojo::internal::kHashSeed,
                blink::SimpleNestedStruct::New(blink::ContainsOther::New(1))));
}

TEST_F(WTFHashTest, Enum) {
  // Just check that this template instantiation compiles.

  // Top-level.
  ASSERT_EQ(
      WTF::HashTraits<blink::TopLevelEnum>::GetHash(blink::TopLevelEnum::E0),
      WTF::HashTraits<blink::TopLevelEnum>::GetHash(blink::TopLevelEnum::E0));

  // Nested in struct.
  ASSERT_EQ(WTF::HashTraits<blink::TestWTFStruct::NestedEnum>::GetHash(
                blink::TestWTFStruct::NestedEnum::E0),
            WTF::HashTraits<blink::TestWTFStruct::NestedEnum>::GetHash(
                blink::TestWTFStruct::NestedEnum::E0));

  // Nested in interface.
  ASSERT_EQ(WTF::HashTraits<blink::TestWTF::NestedEnum>::GetHash(
                blink::TestWTF::NestedEnum::E0),
            WTF::HashTraits<blink::TestWTF::NestedEnum>::GetHash(
                blink::TestWTF::NestedEnum::E0));
}

}  // namespace
}  // namespace test
}  // namespace mojo
