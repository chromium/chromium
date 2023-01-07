// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/hash_util.h"

#include "mojo/public/interfaces/bindings/tests/test_structs.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

using HashTest = testing::Test;

TEST_F(HashTest, NestedStruct) {
  // Just check that this template instantiation compiles.
  ASSERT_EQ(
      ::mojo::internal::Hash(::mojo::internal::kHashSeed,
                             SimpleNestedStruct::New(ContainsOther::New(1))),
      ::mojo::internal::Hash(::mojo::internal::kHashSeed,
                             SimpleNestedStruct::New(ContainsOther::New(1))));
}

}  // namespace
}  // namespace test
}  // namespace mojo
