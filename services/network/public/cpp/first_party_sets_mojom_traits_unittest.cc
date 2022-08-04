// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/first_party_sets_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/first_party_set_entry.h"
#include "services/network/public/mojom/first_party_sets.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

TEST(FirstPartySetsTraitsTest, Roundtrips_FirstPartySetEntry) {
  net::SchemefulSite primary(GURL("https://primary.test"));

  net::FirstPartySetEntry original(primary);
  net::FirstPartySetEntry round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::FirstPartySetEntry>(
      original, round_tripped));

  EXPECT_EQ(original, round_tripped);
  EXPECT_EQ(round_tripped.primary(), primary);
}

}  // namespace
}  // namespace network
