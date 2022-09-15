// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_database_helper.h"

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace content {
namespace {
using PolicyCustomization =
    FirstPartySetsHandlerDatabaseHelper::PolicyCustomization;
using FlattenedSets = FirstPartySetsHandlerDatabaseHelper::FlattenedSets;

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

}  // namespace

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_SitesJoined) {
  FirstPartySetsHandlerDatabaseHelper::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 1)}};

  FirstPartySetsHandlerDatabaseHelper::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 1)},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member2.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 0)},
  };

  // "https://foo.test" and "https://member2.test" joined FPSs. We don't clear
  // site data upon joining, so the computed diff should be empty set.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_policy=*/{}, current_sets, /*current_policy=*/{}),
      IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_SitesLeft) {
  FirstPartySetsHandlerDatabaseHelper::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 1)},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member2.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 0)}};

  FirstPartySetsHandlerDatabaseHelper::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)}};

  // Expected diff: "https://foo.test", "https://member2.test" and
  // "https://member3.test" left FPSs.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_policy=*/{}, current_sets, /*current_policy=*/{}),
      UnorderedElementsAre(SerializesTo("https://foo.test"),
                           SerializesTo("https://member2.test"),
                           SerializesTo("https://member3.test")));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_OwnerChanged) {
  FirstPartySetsHandlerDatabaseHelper::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member2.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 0)},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 1)}};

  FirstPartySetsHandlerDatabaseHelper::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 1)},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member2.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 0)}};

  // Expected diff: "https://member3.test" changed owner.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_policy=*/{}, current_sets, /*current_policy=*/{}),
      UnorderedElementsAre(SerializesTo("https://member3.test")));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_OwnerLeft) {
  FirstPartySetsHandlerDatabaseHelper::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)},
      {net::SchemefulSite(GURL("https://bar.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 1)}};

  FirstPartySetsHandlerDatabaseHelper::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://foo.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://bar.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 0)}};

  // Expected diff: "https://example.test" left FPSs, "https://foo.test" and
  // "https://bar.test" changed owner.
  // It would be valid to only have example.test in the diff, but our logic
  // isn't sophisticated enough yet to know that foo.test and bar.test don't
  // need to be included in the result.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_policy=*/{}, current_sets, /*current_policy=*/{}),
      UnorderedElementsAre(SerializesTo("https://example.test"),
                           SerializesTo("https://foo.test"),
                           SerializesTo("https://bar.test")));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_OwnerMemberRotate) {
  FirstPartySetsHandlerDatabaseHelper::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)}};

  FirstPartySetsHandlerDatabaseHelper::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 0)},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kPrimary, absl::nullopt)}};

  // Expected diff: "https://example.test" and "https://foo.test" changed owner.
  // It would be valid to not include example.test and foo.test in the result,
  // but our logic isn't sophisticated enough yet to know that.ß
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_policy=*/{}, current_sets, /*current_policy=*/{}),
      UnorderedElementsAre(SerializesTo("https://example.test"),
                           SerializesTo("https://foo.test")));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_EmptyOldSets) {
  // Empty old_sets.
  FirstPartySetsHandlerDatabaseHelper::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)}};

  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  /*old_sets=*/{}, /*old_policy=*/{}, current_sets,
                  /*current_policy=*/{}),
              IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_EmptyCurrentSets) {
  // Empty current sets.
  FirstPartySetsHandlerDatabaseHelper::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)}};

  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  old_sets, /*old_policy=*/{}, /*current_sets=*/{},
                  /*current_policy=*/{}),
              IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_PolicySitesJoined) {
  FirstPartySetsHandlerDatabaseHelper::PolicyCustomization current_policy = {
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member2.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
  };

  // "https://example.test" and "https://member2.test" joined FPSs via
  // enterprise policy. We don't clear site data upon joining, so the computed
  // diff should be empty.
  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  /*old_sets=*/{}, /*old_policy=*/{}, /*current_sets=*/{},
                  current_policy),
              IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper,
     ComputeSetsDiff_PolicyRemovedSitesJoined) {
  FirstPartySetsHandlerDatabaseHelper::FlattenedSets sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)}};

  // "https://example.test" was removed from FPSs by policy modifications.
  FirstPartySetsHandlerDatabaseHelper::PolicyCustomization old_policy = {
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
      {net::SchemefulSite(GURL("https://example.test")), absl::nullopt},
  };

  // "https://example.test" added back to FPSs.
  FirstPartySetsHandlerDatabaseHelper::PolicyCustomization current_policy = {
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
      {net::SchemefulSite(GURL("https://example.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
  };

  // We don't clear site data upon joining, so the computed diff should be
  // empty.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          /*old_sets=*/sets, old_policy, /*current_sets=*/sets, current_policy),
      IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_PolicyMemberLeft) {
  FirstPartySetsHandlerDatabaseHelper::PolicyCustomization old_policy = {
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
      {net::SchemefulSite(GURL("https://member2.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
  };

  // "https://member2.test" left FPSs via enterprise policy.
  FirstPartySetsHandlerDatabaseHelper::PolicyCustomization current_policy = {
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
  };

  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          /*old_sets=*/{}, old_policy, /*current_sets=*/{}, current_policy),
      UnorderedElementsAre(SerializesTo("https://member2.test")));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_PolicyOwnerLeft) {
  FirstPartySetsHandlerDatabaseHelper::PolicyCustomization old_policy = {
      {net::SchemefulSite(GURL("https://example.test")),
       {net::FirstPartySetEntry(
           net::SchemefulSite(GURL("https://example.test")),
           net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(
           net::SchemefulSite(GURL("https://example.test")),
           net::SiteType::kAssociated, 0)}},
      {net::SchemefulSite(GURL("https://member2.test")),
       {net::FirstPartySetEntry(
           net::SchemefulSite(GURL("https://example.test")),
           net::SiteType::kAssociated, 0)}},
  };

  FirstPartySetsHandlerDatabaseHelper::PolicyCustomization current_policy = {
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(
           net::SchemefulSite(GURL("https://member1.test")),
           net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member2.test")),
       {net::FirstPartySetEntry(
           net::SchemefulSite(GURL("https://member1.test")),
           net::SiteType::kAssociated, 0)}},
  };

  // Expected diff: "https://example.test" left FPSs, "https://member1.test" and
  // "https://member2.test" changed owner.
  // It would be valid to only have example.test in the diff, but our logic
  // isn't sophisticated enough yet to know that member1.test and member2.test
  // don't need to be included in the result.
  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  /*old_sets=*/{}, /*old_policy=*/old_policy,
                  /*current_sets=*/{}, current_policy),
              UnorderedElementsAre(SerializesTo("https://example.test"),
                                   SerializesTo("https://member1.test"),
                                   SerializesTo("https://member2.test")));
}

TEST(FirstPartySetsHandlerDatabaseHelper,
     ComputeSetsDiff_PolicyMembersChangeSet) {
  FirstPartySetsHandlerDatabaseHelper::PolicyCustomization old_policy = {
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
      {net::SchemefulSite(GURL("https://bar.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://bar.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member2.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://bar.test")),
                                net::SiteType::kAssociated, 0)}},
  };

  FirstPartySetsHandlerDatabaseHelper::PolicyCustomization current_policy = {
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member2.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
      {net::SchemefulSite(GURL("https://bar.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://bar.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://bar.test")),
                                net::SiteType::kAssociated, 0)}},
  };

  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          /*old_sets=*/{}, old_policy, /*current_sets=*/{}, current_policy),
      UnorderedElementsAre(SerializesTo("https://member1.test"),
                           SerializesTo("https://member2.test")));
}

class FirstPartySetsHandlerDatabaseHelperTest : public testing::Test {
 public:
  FirstPartySetsHandlerDatabaseHelperTest() {
    EXPECT_TRUE(dir_.CreateUniqueTempDir());
    db_helper_ = std::make_unique<FirstPartySetsHandlerDatabaseHelper>(
        dir_.GetPath().AppendASCII("TestFirstPartySets.db"));
  }

 protected:
  base::ScopedTempDir dir_;
  base::test::TaskEnvironment env_;

  std::unique_ptr<FirstPartySetsHandlerDatabaseHelper> db_helper_;
};

TEST_F(FirstPartySetsHandlerDatabaseHelperTest,
       UpdateAndGetSitesToClearForContext) {
  const std::string& browser_context_id = "b";

  db_helper_->PersistPublicSets(/*old_sets=*/{
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 1)},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member2.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 0)}});

  FirstPartySetsHandlerDatabaseHelper::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)}};

  std::vector<net::SchemefulSite> res =
      db_helper_->UpdateAndGetSitesToClearForContext(browser_context_id,
                                                     current_sets,
                                                     /*current_policy=*/{});

  // Expected diff: "https://foo.test", "https://member2.test" and
  // "https://member3.test" left FPSs.
  EXPECT_THAT(res, UnorderedElementsAre(SerializesTo("https://foo.test"),
                                        SerializesTo("https://member2.test"),
                                        SerializesTo("https://member3.test")));
}

}  // namespace content
