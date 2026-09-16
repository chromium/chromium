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
      Item(Item::string, "a"),
      Item(Item::token, "b"),
      Item(Item::byte_sequence, "c"),
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
  const Parameter kExpected("d", Item(Item::string, "abc"));

  Parameter actual;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::StructuredHeadersParameter>(
          kExpected, actual));
  EXPECT_EQ(kExpected, actual);
}

TEST(StructuredHeadersMojomTraitsTest,
     ParameterizedItem_SerializeAndDeserialize) {
  const ParameterizedItem kExpected(Item(Item::string, "def"),
                                    net::structured_headers::Parameters({
                                        Parameter("y", Item(Item::string, "s")),
                                        Parameter("x", Item(Item::string, "q")),
                                        Parameter("z", Item(Item::string, "r")),
                                    }));

  ParameterizedItem actual;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::StructuredHeadersParameterizedItem>(kExpected, actual));
  EXPECT_EQ(kExpected, actual);
}

TEST(StructuredHeadersMojomTraitsTest, Dictionary_SerializeAndDeserialize) {
  const Dictionary kExpected({
      {"empty", ParameterizedMember()},
      {"item", ParameterizedMember(Item(Item::string, "def"),
                                   {
                                       Parameter("y", Item(Item::string, "s")),
                                       Parameter("x", Item(Item::string, "q")),
                                       Parameter("z", Item(Item::string, "r")),
                                   })},
      {"inner-list",
       ParameterizedMember(/*items=*/
                           {ParameterizedItem(Item(Item::string, "abc"), {}),
                            ParameterizedItem(
                                Item(Item::string, "xyz"),
                                {Parameter("y", Item(Item::string, "q"))})},
                           /*params=*/{Parameter("z",
                                                 Item(Item::string, "r"))})},
  });

  Dictionary actual;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::StructuredHeadersDictionary>(
          kExpected, actual));
  EXPECT_EQ(kExpected, actual);
}

}  // namespace
}  // namespace network
