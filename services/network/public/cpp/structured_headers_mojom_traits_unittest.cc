// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/structured_headers_mojom_traits.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/structured_headers.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using ::net::structured_headers::Dictionary;
using ::net::structured_headers::Item;
using ::net::structured_headers::ParameterizedItem;
using ::net::structured_headers::ParameterizedMember;

using Parameter = std::pair<std::string, Item>;

TEST(StructuredHeadersMojomTraitsTest, Item_SerializeAndDeserialize) {
  const Item kTestCases[] = {
      Item(),
      Item(static_cast<int64_t>(7)),
      Item(2.8),
      Item("a", Item::kStringType),
      Item("b", Item::kTokenType),
      Item("c", Item::kByteSequenceType),
      Item(true),
  };

  for (const auto& expected : kTestCases) {
    Item actual;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::StructuredHeadersItem>(
            expected, actual));
    EXPECT_EQ(expected, actual);
  }
}

TEST(StructuredHeadersMojomTraitsTest, Parameter_SerializeAndDeserialize) {
  const Parameter kExpected("d", Item("abc"));

  Parameter actual;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::StructuredHeadersParameter>(
          kExpected, actual));
  EXPECT_EQ(kExpected, actual);
}

TEST(StructuredHeadersMojomTraitsTest,
     ParameterizedItem_SerializeAndDeserialize) {
  const ParameterizedItem kExpected(Item("def"),
                                    net::structured_headers::Parameters({
                                        Parameter("y", Item("s")),
                                        Parameter("x", Item("q")),
                                        Parameter("z", Item("r")),
                                    }));

  ParameterizedItem actual;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::StructuredHeadersParameterizedItem>(kExpected, actual));
  EXPECT_EQ(kExpected, actual);
}

TEST(StructuredHeadersMojomTraitsTest, Dictionary_SerializeAndDeserialize) {
  const Dictionary kExpected({
      {"abc",
       ParameterizedMember({ParameterizedItem(Item("def"),
                                              {
                                                  Parameter("y", Item("s")),
                                                  Parameter("x", Item("q")),
                                                  Parameter("z", Item("r")),
                                              })},
                           /*member_is_inner_list=*/false, {})},
      {"def", ParameterizedMember(
                  {ParameterizedItem(Item("abc"), {}),
                   ParameterizedItem(Item("xyz"), {Parameter("y", Item("q"))})},
                  /*member_is_inner_list=*/true, {Parameter("z", Item("r"))})},
  });

  Dictionary actual;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::StructuredHeadersDictionary>(
          kExpected, actual));
  EXPECT_EQ(kExpected, actual);
}

}  // namespace
}  // namespace network
