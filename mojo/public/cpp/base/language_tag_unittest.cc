// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/i18n/language_tag.h"

#include "base/i18n/tags.h"
#include "mojo/public/cpp/base/language_tag_mojom_traits.h"
#include "mojo/public/cpp/bindings/lib/default_construct_tag_internal.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/language_tag.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace mojo_base {

namespace {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::LanguageTag;

TEST(LanguageTagTest, Empty) {
  mojom::LanguageTagPtr in = mojom::LanguageTag::New("");
  LanguageTag out = GetKnownLanguageTag("und");
  ASSERT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::LanguageTag>(in, out));
}

TEST(LanguageTagTest, Standard) {
  LanguageTag in = GetKnownLanguageTag("en");
  LanguageTag out = GetKnownLanguageTag("und");
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::LanguageTag>(in, out));
  EXPECT_EQ(in, out);
}

TEST(LanguageTagTest, Complex) {
  LanguageTag in = GetKnownLanguageTag("zh-TW");
  LanguageTag out = GetKnownLanguageTag("und");
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::LanguageTag>(in, out));
  EXPECT_EQ(in, out);
}

}  // namespace
}  // namespace mojo_base
