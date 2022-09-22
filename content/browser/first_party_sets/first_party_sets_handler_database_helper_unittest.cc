// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_database_helper.h"

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/public_sets.h"
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

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

}  // namespace

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_SitesJoined) {
  net::PublicSets old_sets(
      /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://member1.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)},
                   {net::SchemefulSite(GURL("https://member3.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 1)}},
      /*aliases=*/{});

  net::PublicSets current_sets(
      /*entries=*/
      {
          {net::SchemefulSite(GURL("https://example.test")),
           net::FirstPartySetEntry(
               net::SchemefulSite(GURL("https://example.test")),
               net::SiteType::kPrimary, absl::nullopt)},
          {net::SchemefulSite(GURL("https://member1.test")),
           net::FirstPartySetEntry(
               net::SchemefulSite(GURL("https://example.test")),
               net::SiteType::kAssociated, 0)},
          {net::SchemefulSite(GURL("https://member3.test")),
           net::FirstPartySetEntry(
               net::SchemefulSite(GURL("https://example.test")),
               net::SiteType::kAssociated, 1)},
          {net::SchemefulSite(GURL("https://foo.test")),
           net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                   net::SiteType::kPrimary, absl::nullopt)},
          {net::SchemefulSite(GURL("https://member2.test")),
           net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                   net::SiteType::kAssociated, 0)},
      },
      /*aliases=*/{});

  // "https://foo.test" and "https://member2.test" joined FPSs. We don't clear
  // site data upon joining, so the computed diff should be empty set.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_config=*/net::FirstPartySetsContextConfig(),
          current_sets, /*current_config=*/net::FirstPartySetsContextConfig()),
      IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_SitesLeft) {
  net::PublicSets old_sets(
      /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://member1.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)},
                   {net::SchemefulSite(GURL("https://member3.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 1)},
                   {net::SchemefulSite(GURL("https://foo.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://member2.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  net::PublicSets current_sets(
      /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://member1.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  // Expected diff: "https://foo.test", "https://member2.test" and
  // "https://member3.test" left FPSs.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_config=*/net::FirstPartySetsContextConfig(),
          current_sets, /*current_config=*/net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(SerializesTo("https://foo.test"),
                           SerializesTo("https://member2.test"),
                           SerializesTo("https://member3.test")));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_OwnerChanged) {
  net::PublicSets old_sets(
      /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://member1.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)},
                   {net::SchemefulSite(GURL("https://foo.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://member2.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kAssociated, 0)},
                   {net::SchemefulSite(GURL("https://member3.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kAssociated, 1)}},
      /*aliases=*/{});

  net::PublicSets current_sets(
      /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://member1.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)},
                   {net::SchemefulSite(GURL("https://member3.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 1)},
                   {net::SchemefulSite(GURL("https://foo.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://member2.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  // Expected diff: "https://member3.test" changed owner.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_config=*/net::FirstPartySetsContextConfig(),
          current_sets, /*current_config=*/net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(SerializesTo("https://member3.test")));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_OwnerLeft) {
  net::PublicSets old_sets(
      /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://foo.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)},
                   {net::SchemefulSite(GURL("https://bar.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 1)}},
      /*aliases=*/{});

  net::PublicSets current_sets(
      /*entries=*/{{net::SchemefulSite(GURL("https://foo.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://bar.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  // Expected diff: "https://example.test" left FPSs, "https://foo.test" and
  // "https://bar.test" changed owner.
  // It would be valid to only have example.test in the diff, but our logic
  // isn't sophisticated enough yet to know that foo.test and bar.test don't
  // need to be included in the result.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_config=*/net::FirstPartySetsContextConfig(),
          current_sets, /*current_config=*/net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(SerializesTo("https://example.test"),
                           SerializesTo("https://foo.test"),
                           SerializesTo("https://bar.test")));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_OwnerMemberRotate) {
  net::PublicSets old_sets(
      /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://foo.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  net::PublicSets current_sets(
      /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kAssociated, 0)},
                   {net::SchemefulSite(GURL("https://foo.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://foo.test")),
                        net::SiteType::kPrimary, absl::nullopt)}},
      /*aliases=*/{});

  // Expected diff: "https://example.test" and "https://foo.test" changed owner.
  // It would be valid to not include example.test and foo.test in the result,
  // but our logic isn't sophisticated enough yet to know that.ß
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_config=*/net::FirstPartySetsContextConfig(),
          current_sets, /*current_config=*/net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(SerializesTo("https://example.test"),
                           SerializesTo("https://foo.test")));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_EmptyOldSets) {
  // Empty old_sets.
  net::PublicSets current_sets(
      /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://member1.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          /*old_sets=*/net::PublicSets(),
          /*old_config=*/net::FirstPartySetsContextConfig(), current_sets,
          /*current_config=*/net::FirstPartySetsContextConfig()),
      IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_EmptyCurrentSets) {
  // Empty current sets.
  net::PublicSets old_sets(
      /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://member1.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  old_sets, /*old_config=*/net::FirstPartySetsContextConfig(),
                  /*current_sets=*/net::PublicSets(),
                  /*current_config=*/net::FirstPartySetsContextConfig()),
              IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_PolicySitesJoined) {
  net::FirstPartySetsContextConfig current_config({
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member2.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
  });

  // "https://example.test" and "https://member2.test" joined FPSs via
  // enterprise policy. We don't clear site data upon joining, so the computed
  // diff should be empty.
  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  /*old_sets=*/net::PublicSets(), /*old_config=*/current_config,
                  /*current_sets=*/net::PublicSets(), current_config),
              IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper,
     ComputeSetsDiff_PolicyRemovedSitesJoined) {
  net::PublicSets sets(
      /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://member1.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  // "https://example.test" was removed from FPSs by policy modifications.
  net::FirstPartySetsContextConfig old_config({
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
      {net::SchemefulSite(GURL("https://example.test")), absl::nullopt},
  });

  // "https://example.test" added back to FPSs.
  net::FirstPartySetsContextConfig current_config({
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
      {net::SchemefulSite(GURL("https://example.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
  });

  // We don't clear site data upon joining, so the computed diff should be
  // empty.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          /*old_sets=*/sets, old_config, /*current_sets=*/sets, current_config),
      IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_PolicyMemberLeft) {
  net::FirstPartySetsContextConfig old_config({
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
      {net::SchemefulSite(GURL("https://member2.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
  });

  // "https://member2.test" left FPSs via enterprise policy.
  net::FirstPartySetsContextConfig current_config({
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
  });

  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  /*old_sets=*/net::PublicSets(), old_config,
                  /*current_sets=*/net::PublicSets(), current_config),
              UnorderedElementsAre(SerializesTo("https://member2.test")));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_PolicyOwnerLeft) {
  net::FirstPartySetsContextConfig old_config({
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
  });

  net::FirstPartySetsContextConfig current_config({
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(
           net::SchemefulSite(GURL("https://member1.test")),
           net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member2.test")),
       {net::FirstPartySetEntry(
           net::SchemefulSite(GURL("https://member1.test")),
           net::SiteType::kAssociated, 0)}},
  });

  // Expected diff: "https://example.test" left FPSs, "https://member1.test" and
  // "https://member2.test" changed owner.
  // It would be valid to only have example.test in the diff, but our logic
  // isn't sophisticated enough yet to know that member1.test and member2.test
  // don't need to be included in the result.
  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  /*old_sets=*/net::PublicSets(), /*old_config=*/old_config,
                  /*current_sets=*/net::PublicSets(), current_config),
              UnorderedElementsAre(SerializesTo("https://example.test"),
                                   SerializesTo("https://member1.test"),
                                   SerializesTo("https://member2.test")));
}

TEST(FirstPartySetsHandlerDatabaseHelper,
     ComputeSetsDiff_PolicyMembersChangeSet) {
  net::FirstPartySetsContextConfig old_config({
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
  });

  net::FirstPartySetsContextConfig current_config({
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
  });

  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  /*old_sets=*/net::PublicSets(), old_config,
                  /*current_sets=*/net::PublicSets(), current_config),
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

  db_helper_->PersistPublicSets(
      browser_context_id, base::Version("0.0.1"),
      net::PublicSets(
          /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                        net::FirstPartySetEntry(
                            net::SchemefulSite(GURL("https://example.test")),
                            net::SiteType::kPrimary, absl::nullopt)},
                       {net::SchemefulSite(GURL("https://member1.test")),
                        net::FirstPartySetEntry(
                            net::SchemefulSite(GURL("https://example.test")),
                            net::SiteType::kAssociated, 0)},
                       {net::SchemefulSite(GURL("https://member3.test")),
                        net::FirstPartySetEntry(
                            net::SchemefulSite(GURL("https://example.test")),
                            net::SiteType::kAssociated, 1)},
                       {net::SchemefulSite(GURL("https://foo.test")),
                        net::FirstPartySetEntry(
                            net::SchemefulSite(GURL("https://foo.test")),
                            net::SiteType::kPrimary, absl::nullopt)},
                       {net::SchemefulSite(GURL("https://member2.test")),
                        net::FirstPartySetEntry(
                            net::SchemefulSite(GURL("https://foo.test")),
                            net::SiteType::kAssociated, 0)}},
          /*aliases=*/{}));

  net::PublicSets current_sets = net::PublicSets(
      /*entries=*/{{net::SchemefulSite(GURL("https://example.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kPrimary, absl::nullopt)},
                   {net::SchemefulSite(GURL("https://member1.test")),
                    net::FirstPartySetEntry(
                        net::SchemefulSite(GURL("https://example.test")),
                        net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  std::vector<net::SchemefulSite> res =
      db_helper_->UpdateAndGetSitesToClearForContext(
          browser_context_id, current_sets,
          /*current_config=*/net::FirstPartySetsContextConfig());

  // Expected diff: "https://foo.test", "https://member2.test" and
  // "https://member3.test" left FPSs.
  EXPECT_THAT(res, UnorderedElementsAre(SerializesTo("https://foo.test"),
                                        SerializesTo("https://member2.test"),
                                        SerializesTo("https://member3.test")));
}

}  // namespace content
