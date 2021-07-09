// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets.h"

#include <initializer_list>

#include "base/json/json_reader.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/same_party_context.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;
using ::testing::Value;

// Some of these tests overlap with FirstPartySetParser unittests, but
// overlapping test coverage isn't the worst thing.

namespace network {

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

TEST(FirstPartySets, Sets_IsEmpty) {
  EXPECT_THAT(FirstPartySets().Sets(), IsEmpty());
}

TEST(FirstPartySets, ParsesJSON) {
  EXPECT_THAT(FirstPartySets().ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(FirstPartySets, AcceptsMinimal) {
  const std::string input =
      R"([{
        "owner": "https://example.test",
        "members": ["https://aaaa.test"]
        }])";
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySets().ParseAndSet(input),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://aaaa.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySets, AcceptsMultipleSets) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://member1.test"]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member2.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(
      FirstPartySets().ParseAndSet(input),
      Pointee(UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test")))));
}

TEST(FirstPartySets, ClearsPreloadedOnError) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://member1.test"]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member2.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));

  FirstPartySets sets;
  EXPECT_THAT(
      sets.ParseAndSet(input),
      Pointee(UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test")))));

  EXPECT_THAT(sets.ParseAndSet("{}"), Pointee(IsEmpty()));
}

TEST(FirstPartySets, OwnerIsOnlyMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://example.test"]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member2.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySets().ParseAndSet(input), Pointee(IsEmpty()));
}

TEST(FirstPartySets, OwnerIsMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://example.test", "https://member1.test"]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member2.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySets().ParseAndSet(input), Pointee(IsEmpty()));
}

TEST(FirstPartySets, RepeatedMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": [
        "https://member1.test",
        "https://member2.test",
        "https://member1.test"
        ]
    },
    {
      "owner": "https://foo.test",
      "members": ["https://member3.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_THAT(FirstPartySets().ParseAndSet(input), Pointee(IsEmpty()));
}

TEST(FirstPartySets, SetsManuallySpecified_Invalid_TooSmall) {
  FirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test");
  EXPECT_THAT(sets.ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(FirstPartySets, SetsManuallySpecified_Invalid_NotOrigins) {
  FirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test,member1");
  EXPECT_THAT(sets.ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(FirstPartySets, SetsManuallySpecified_Invalid_NotHTTPS) {
  FirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test,http://member1.test");
  EXPECT_THAT(sets.ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(FirstPartySets, SetsManuallySpecified_Invalid_RegisteredDomain_Owner) {
  FirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://www.example.test..,https://www.member.test");
  EXPECT_THAT(sets.ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(FirstPartySets, SetsManuallySpecified_Invalid_RegisteredDomain_Member) {
  FirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://www.example.test,https://www.member.test..");
  EXPECT_THAT(sets.ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(FirstPartySets, SetsManuallySpecified_Valid_SingleMember) {
  FirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test,https://member.test");
  EXPECT_THAT(sets.ParseAndSet("[]"),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySets,
     SetsManuallySpecified_Valid_SingleMember_RegisteredDomain) {
  FirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://www.example.test,https://www.member.test");
  EXPECT_THAT(sets.ParseAndSet("[]"),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySets, SetsManuallySpecified_Valid_MultipleMembers) {
  FirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  EXPECT_THAT(sets.ParseAndSet("[]"),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member2.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySets, SetsManuallySpecified_Valid_OwnerIsOnlyMember) {
  FirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test,https://example.test");
  EXPECT_THAT(sets.ParseAndSet("[]"), Pointee(IsEmpty()));
}

TEST(FirstPartySets, SetsManuallySpecified_Valid_OwnerIsMember) {
  FirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://example.test,https://example.test,https://member1.test");
  EXPECT_THAT(sets.ParseAndSet("[]"),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySets, SetsManuallySpecified_Valid_RepeatedMember) {
  FirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      R"(https://example.test,
       https://member1.test,
       https://member2.test,
       https://member1.test)");
  EXPECT_THAT(sets.ParseAndSet("[]"),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member2.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySets, SetsManuallySpecified_DeduplicatesOwnerOwner) {
  const std::string input = R"(
  [
    {
      "owner": "https://example.test",
      "members": ["https://member2.test", "https://member3.test"]
    },
    {
      "owner": "https://bar.test",
      "members": ["https://member4.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));

  FirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  EXPECT_THAT(
      sets.ParseAndSet(input),
      Pointee(UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://bar.test"),
                                        SerializesTo("https://bar.test")),
                                   Pair(SerializesTo("https://member4.test"),
                                        SerializesTo("https://bar.test")))));
}

TEST(FirstPartySets, SetsManuallySpecified_DeduplicatesOwnerMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://foo.test",
      "members": ["https://member1.test", "https://example.test"]
    },
    {
      "owner": "https://bar.test",
      "members": ["https://member2.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));

  FirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member3.test");
  EXPECT_THAT(sets.ParseAndSet(input),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://bar.test"),
                       SerializesTo("https://bar.test")),
                  Pair(SerializesTo("https://member2.test"),
                       SerializesTo("https://bar.test")),
                  Pair(SerializesTo("https://member3.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySets, SetsManuallySpecified_DeduplicatesMemberOwner) {
  const std::string input = R"(
  [
    {
      "owner": "https://foo.test",
      "members": ["https://member1.test", "https://member2.test"]
    },
    {
      "owner": "https://member3.test",
      "members": ["https://member4.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));

  FirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test,https://member3.test");
  EXPECT_THAT(sets.ParseAndSet(input),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://foo.test"),
                       SerializesTo("https://foo.test")),
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://foo.test")),
                  Pair(SerializesTo("https://member2.test"),
                       SerializesTo("https://foo.test")),
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member3.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySets, SetsManuallySpecified_DeduplicatesMemberMember) {
  const std::string input = R"(
  [
    {
      "owner": "https://foo.test",
      "members": ["https://member2.test", "https://member3.test"]
    },
    {
      "owner": "https://bar.test",
      "members": ["https://member4.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));

  FirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  EXPECT_THAT(
      sets.ParseAndSet(input),
      Pointee(UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member3.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://bar.test"),
                                        SerializesTo("https://bar.test")),
                                   Pair(SerializesTo("https://member4.test"),
                                        SerializesTo("https://bar.test")))));
}

TEST(FirstPartySets, SetsManuallySpecified_ClearsPreloadedOnError) {
  const std::string input = R"(
  [
    {
      "owner": "https://bar.test",
      "members": ["https://member3.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));

  FirstPartySets sets;
  sets.SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  EXPECT_THAT(
      sets.ParseAndSet(input),
      Pointee(UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://bar.test"),
                                        SerializesTo("https://bar.test")),
                                   Pair(SerializesTo("https://member3.test"),
                                        SerializesTo("https://bar.test")))));

  EXPECT_THAT(sets.ParseAndSet("{}"),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member2.test"),
                       SerializesTo("https://example.test")))));
}

TEST(FirstPartySets, SetsManuallySpecified_PrunesInducedSingletons) {
  const std::string input = R"(
  [
    {
      "owner": "https://foo.test",
      "members": ["https://member1.test"]
    }
  ]
  )";
  ASSERT_TRUE(base::JSONReader::Read(input));

  FirstPartySets sets;
  sets.SetManuallySpecifiedSet("https://example.test,https://member1.test");
  // If we just erased entries that overlapped with the manually-supplied set,
  // https://foo.test would be left as a singleton set. But since we disallow
  // singleton sets, we ensure that such cases are caught and removed.
  EXPECT_THAT(sets.ParseAndSet(input),
              Pointee(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       SerializesTo("https://example.test")),
                  Pair(SerializesTo("https://member1.test"),
                       SerializesTo("https://example.test")))));
}

class FirstPartySetsTest : public ::testing::Test {
 public:
  FirstPartySetsTest() {
    const std::string input = R"(
      [
        {
          "owner": "https://example.test",
          "members": ["https://member1.test", "https://member3.test"]
        },
        {
          "owner": "https://foo.test",
          "members": ["https://member2.test"]
        }
      ]
      )";
    CHECK(base::JSONReader::Read(input));

    CHECK(Value(
        sets().ParseAndSet(input),
        Pointee(UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                          SerializesTo("https://example.test")),
                                     Pair(SerializesTo("https://member1.test"),
                                          SerializesTo("https://example.test")),
                                     Pair(SerializesTo("https://member3.test"),
                                          SerializesTo("https://example.test")),
                                     Pair(SerializesTo("https://foo.test"),
                                          SerializesTo("https://foo.test")),
                                     Pair(SerializesTo("https://member2.test"),
                                          SerializesTo("https://foo.test"))))));
  }

  FirstPartySets& sets() { return sets_; }

 protected:
  FirstPartySets sets_;
};

TEST_F(FirstPartySetsTest, IsContextSamePartyWithSite_EmptyContext) {
  net::SchemefulSite example_site(GURL("https://example.test"));
  net::SchemefulSite nonmember(GURL("https://nonmember.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://nonmember.test")), top_frame, {},
        false /* infer_singleton_sets */));

    EXPECT_TRUE(sets().IsContextSamePartyWithSite(
        example_site, top_frame, {}, false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("http://example.test")), top_frame, {},
        false /* infer_singleton_sets */));
  }

  EXPECT_FALSE(sets().IsContextSamePartyWithSite(
      example_site, &nonmember, {}, false /* infer_singleton_sets */));
  EXPECT_FALSE(sets().IsContextSamePartyWithSite(
      nonmember, &example_site, {}, false /* infer_singleton_sets */));
}

TEST_F(FirstPartySetsTest, IsContextSamePartyWithSite_ContextIsNonmember) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://nonmember.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("http://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member1.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://foo.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member2.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://nonmember.test")), top_frame, context,
        false /* infer_singleton_sets */));
  }
}

TEST_F(FirstPartySetsTest, IsContextSamePartyWithSite_ContextIsOwner) {
  std::set<net::SchemefulSite> context(
      {net::SchemefulSite(GURL("https://example.test"))});

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_TRUE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("http://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_TRUE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member1.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://foo.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member2.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://nonmember.test")), top_frame, context,
        false /* infer_singleton_sets */));
  }
}

TEST_F(FirstPartySetsTest, IsContextSamePartyWithSite_ContextIsMember) {
  std::set<net::SchemefulSite> context(
      {net::SchemefulSite(GURL("https://member1.test"))});

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_TRUE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("http://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_TRUE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_TRUE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member1.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://foo.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member2.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://nonmember.test")), top_frame, context,
        false /* infer_singleton_sets */));
  }
}

TEST_F(FirstPartySetsTest, IsContextSamePartyWithSite_ContextIsOwnerAndMember) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_TRUE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("http://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_TRUE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member1.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_TRUE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member3.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://foo.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member2.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://nonmember.test")), top_frame, context,
        false /* infer_singleton_sets */));
  }
}

TEST_F(FirstPartySetsTest, IsContextSamePartyWithSite_ContextMixesParties) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
      net::SchemefulSite(GURL("https://foo.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("http://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member1.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://foo.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member2.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://nonmember.test")), top_frame, context,
        false /* infer_singleton_sets */));
  }
}

TEST_F(FirstPartySetsTest,
       IsContextSamePartyWithSite_ContextMixesMembersAndNonmembers) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
      net::SchemefulSite(GURL("http://nonmember.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("http://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member1.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://foo.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member2.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://nonmember.test")), top_frame, context,
        false /* infer_singleton_sets */));
  }
}

TEST_F(FirstPartySetsTest, IsContextSamePartyWithSite_ContextMixesSchemes) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
      net::SchemefulSite(GURL("http://example.test")),
  });

  net::SchemefulSite example_site(GURL("https://example.test"));

  for (const net::SchemefulSite* top_frame :
       std::initializer_list<net::SchemefulSite*>{&example_site, nullptr}) {
    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("http://example.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member1.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://foo.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://member2.test")), top_frame, context,
        false /* infer_singleton_sets */));

    EXPECT_FALSE(sets().IsContextSamePartyWithSite(
        net::SchemefulSite(GURL("https://nonmember.test")), top_frame, context,
        false /* infer_singleton_sets */));
  }
}

TEST_F(FirstPartySetsTest, IsContextSamePartyWithSite_InfersSingletonSets) {
  std::set<net::SchemefulSite> context({
      net::SchemefulSite(GURL("https://nonmember.test")),
  });
  net::SchemefulSite nonmember(GURL("https://nonmember.test"));
  net::SchemefulSite nonmember2(GURL("https://nonmember2.test"));
  net::SchemefulSite member(GURL("https://member1.test"));

  EXPECT_TRUE(sets().IsContextSamePartyWithSite(nonmember, nullptr, {}, true));

  EXPECT_TRUE(
      sets().IsContextSamePartyWithSite(nonmember, &nonmember, {}, true));

  EXPECT_TRUE(
      sets().IsContextSamePartyWithSite(nonmember, nullptr, context, true));

  EXPECT_TRUE(
      sets().IsContextSamePartyWithSite(nonmember, &nonmember, context, true));

  // Context mismatches.
  EXPECT_FALSE(sets().IsContextSamePartyWithSite(nonmember, &nonmember,
                                                 {nonmember2}, true));

  // Context mismatches (but is a member of some set).
  EXPECT_FALSE(
      sets().IsContextSamePartyWithSite(nonmember, &nonmember, {member}, true));

  // Top frame mismatches.
  EXPECT_FALSE(
      sets().IsContextSamePartyWithSite(nonmember, &member, {nonmember}, true));

  // Request URL mismatches.
  EXPECT_FALSE(sets().IsContextSamePartyWithSite(
      net::SchemefulSite(GURL("https://nonmember1.test")), &nonmember2,
      {nonmember2}, true));

  // Request URL mismatches (but is a member of some set).
  EXPECT_FALSE(
      sets().IsContextSamePartyWithSite(member, &nonmember, {nonmember}, true));
}

TEST_F(FirstPartySetsTest, ComputeContext) {
  using SamePartyContextType = net::SamePartyContext::Type;

  net::SchemefulSite nonmember(GURL("https://nonmember.test"));
  net::SchemefulSite nonmember1(GURL("https://nonmember1.test"));
  net::SchemefulSite member(GURL("https://member1.test"));
  net::SchemefulSite owner(GURL("https://example.test"));

  // Works as usual for sites that are in First-Party sets.
  EXPECT_THAT(sets().ComputeContext(member, &member, {member}),
              net::SamePartyContext(SamePartyContextType::kSameParty));
  EXPECT_THAT(sets().ComputeContext(owner, &member, {member}),
              net::SamePartyContext(SamePartyContextType::kSameParty));
  EXPECT_THAT(sets().ComputeContext(member, &owner, {member}),
              net::SamePartyContext(SamePartyContextType::kSameParty));
  EXPECT_THAT(sets().ComputeContext(member, &member, {owner}),
              net::SamePartyContext(SamePartyContextType::kSameParty));
  EXPECT_THAT(sets().ComputeContext(member, &member, {member, owner}),
              net::SamePartyContext(SamePartyContextType::kSameParty));

  EXPECT_THAT(sets().ComputeContext(nonmember, &member, {member}),
              net::SamePartyContext(SamePartyContextType::kCrossParty));
  EXPECT_THAT(sets().ComputeContext(member, &nonmember, {member}),
              net::SamePartyContext(SamePartyContextType::kCrossParty));

  // Top&resource differs from Ancestors.
  EXPECT_THAT(sets().ComputeContext(member, &member, {nonmember}),
              net::SamePartyContext(SamePartyContextType::kCrossParty,
                                    SamePartyContextType::kCrossParty,
                                    SamePartyContextType::kSameParty));

  // Metrics values infer singleton sets when appropriate.
  EXPECT_THAT(sets().ComputeContext(nonmember, &nonmember, {nonmember}),
              net::SamePartyContext(SamePartyContextType::kCrossParty,
                                    SamePartyContextType::kSameParty,
                                    SamePartyContextType::kSameParty));
  EXPECT_THAT(sets().ComputeContext(nonmember, &nonmember1, {nonmember}),
              net::SamePartyContext(SamePartyContextType::kCrossParty));
  EXPECT_THAT(sets().ComputeContext(nonmember1, &nonmember, {nonmember}),
              net::SamePartyContext(SamePartyContextType::kCrossParty));
  EXPECT_THAT(sets().ComputeContext(nonmember, &nonmember, {nonmember1}),
              net::SamePartyContext(SamePartyContextType::kCrossParty,
                                    SamePartyContextType::kCrossParty,
                                    SamePartyContextType::kSameParty));

  EXPECT_THAT(sets().ComputeContext(member, &member, {member, nonmember}),
              net::SamePartyContext(SamePartyContextType::kCrossParty,
                                    SamePartyContextType::kCrossParty,
                                    SamePartyContextType::kSameParty));
  EXPECT_THAT(sets().ComputeContext(nonmember, &nonmember, {member, nonmember}),
              net::SamePartyContext(SamePartyContextType::kCrossParty,
                                    SamePartyContextType::kCrossParty,
                                    SamePartyContextType::kSameParty));
}

TEST_F(FirstPartySetsTest, IsInNontrivialFirstPartySet) {
  EXPECT_TRUE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("https://example.test"))));

  EXPECT_FALSE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("http://example.test"))));

  EXPECT_TRUE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("https://member1.test"))));

  EXPECT_FALSE(sets().IsInNontrivialFirstPartySet(
      net::SchemefulSite(GURL("https://nonmember.test"))));
}

TEST_F(FirstPartySetsTest, Sets_NonEmpty) {
  EXPECT_THAT(
      sets().Sets(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               UnorderedElementsAre(SerializesTo("https://example.test"),
                                    SerializesTo("https://member1.test"),
                                    SerializesTo("https://member3.test"))),
          Pair(SerializesTo("https://foo.test"),
               UnorderedElementsAre(SerializesTo("https://foo.test"),
                                    SerializesTo("https://member2.test")))));
}

TEST_F(FirstPartySetsTest, ComputeContextType) {
  std::set<net::SchemefulSite> homogeneous_context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://member1.test")),
  });
  std::set<net::SchemefulSite> mixed_context({
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://nonmember.test")),
  });
  net::SchemefulSite singleton(GURL("https://implicit-singleton.test"));

  EXPECT_EQ(
      net::FirstPartySetsContextType::kTopFrameIgnoredHomogeneous,
      sets().ComputeContextType(
          net::SchemefulSite(GURL("https://example.test")), absl::nullopt, {}));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopFrameIgnoredHomogeneous,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")), absl::nullopt,
                homogeneous_context));

  EXPECT_EQ(net::FirstPartySetsContextType::kTopFrameIgnoredMixed,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")), absl::nullopt,
                mixed_context));

  EXPECT_EQ(net::FirstPartySetsContextType::kHomogeneous,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")),
                net::SchemefulSite(GURL("https://member1.test")), {}));
  EXPECT_EQ(net::FirstPartySetsContextType::kHomogeneous,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")),
                net::SchemefulSite(GURL("https://member1.test")),
                homogeneous_context));
  EXPECT_EQ(net::FirstPartySetsContextType::kHomogeneous,
            sets().ComputeContextType(singleton, singleton, {singleton}));

  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMatchMixed,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")),
                net::SchemefulSite(GURL("https://member1.test")),
                {net::SchemefulSite(GURL("https://foo.test"))}));
  EXPECT_EQ(
      net::FirstPartySetsContextType::kTopResourceMatchMixed,
      sets().ComputeContextType(
          net::SchemefulSite(GURL("https://example.test")),
          net::SchemefulSite(GURL("https://member1.test")), mixed_context));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMatchMixed,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")),
                net::SchemefulSite(GURL("https://member1.test")), {singleton}));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMatchMixed,
            sets().ComputeContextType(singleton, singleton, mixed_context));

  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMismatch,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")),
                net::SchemefulSite(GURL("https://foo.test")), {}));
  EXPECT_EQ(
      net::FirstPartySetsContextType::kTopResourceMismatch,
      sets().ComputeContextType(
          net::SchemefulSite(GURL("https://example.test")),
          net::SchemefulSite(GURL("https://foo.test")), homogeneous_context));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMismatch,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")),
                net::SchemefulSite(GURL("https://foo.test")), mixed_context));
  EXPECT_EQ(net::FirstPartySetsContextType::kTopResourceMismatch,
            sets().ComputeContextType(
                net::SchemefulSite(GURL("https://example.test")), singleton,
                mixed_context));
}

}  // namespace network
