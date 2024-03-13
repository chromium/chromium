// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_database_helper.h"

#include <optional>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace content {
namespace {
const base::Version kVersion("1.2.3");
}  // namespace

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_SitesJoined) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));
  net::SchemefulSite member3(GURL("https://member3.test"));

  net::GlobalFirstPartySets old_sets(
      kVersion,
      /*entries=*/
      {{example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                         std::nullopt)},
       {member1,
        net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
       {member3,
        net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)}},
      /*aliases=*/{});

  net::GlobalFirstPartySets current_sets(
      kVersion,
      /*entries=*/
      {
          {example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                            std::nullopt)},
          {member1,
           net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
          {member3,
           net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
          {foo,
           net::FirstPartySetEntry(foo, net::SiteType::kPrimary, std::nullopt)},
          {member2,
           net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0)},
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
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));
  net::SchemefulSite member3(GURL("https://member3.test"));

  net::GlobalFirstPartySets old_sets(
      kVersion,
      /*entries=*/
      {{example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                         std::nullopt)},
       {member1,
        net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
       {member3,
        net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
       {foo,
        net::FirstPartySetEntry(foo, net::SiteType::kPrimary, std::nullopt)},
       {member2, net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  net::GlobalFirstPartySets current_sets(
      kVersion,
      /*entries=*/
      {{example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                         std::nullopt)},
       {member1,
        net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  // Expected diff: "https://foo.test", "https://member2.test" and
  // "https://member3.test" left FPSs.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_config=*/net::FirstPartySetsContextConfig(),
          current_sets, /*current_config=*/net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(foo, member2, member3));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_PrimaryChanged) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));
  net::SchemefulSite member3(GURL("https://member3.test"));

  net::GlobalFirstPartySets old_sets(
      kVersion,
      /*entries=*/
      {{example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                         std::nullopt)},
       {member1,
        net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
       {foo,
        net::FirstPartySetEntry(foo, net::SiteType::kPrimary, std::nullopt)},
       {member2, net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0)},
       {member3, net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 1)}},
      /*aliases=*/{});

  net::GlobalFirstPartySets current_sets(
      kVersion,
      /*entries=*/
      {{example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                         std::nullopt)},
       {member1,
        net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
       {member3,
        net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
       {foo,
        net::FirstPartySetEntry(foo, net::SiteType::kPrimary, std::nullopt)},
       {member2, net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  // Expected diff: "https://member3.test" changed primary.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_config=*/net::FirstPartySetsContextConfig(),
          current_sets, /*current_config=*/net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(member3));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_PrimaryLeft) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite bar(GURL("https://bar.test"));

  net::GlobalFirstPartySets old_sets(
      kVersion,
      /*entries=*/
      {{example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                         std::nullopt)},
       {foo, net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
       {bar, net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)}},
      /*aliases=*/{});

  net::GlobalFirstPartySets current_sets(
      kVersion,
      /*entries=*/
      {{foo,
        net::FirstPartySetEntry(foo, net::SiteType::kPrimary, std::nullopt)},
       {bar, net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  // Expected diff: "https://example.test" left FPSs, "https://foo.test" and
  // "https://bar.test" changed primary.
  // It would be valid to only have example.test in the diff, but our logic
  // isn't sophisticated enough yet to know that foo.test and bar.test don't
  // need to be included in the result.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_config=*/net::FirstPartySetsContextConfig(),
          current_sets, /*current_config=*/net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(example, foo, bar));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_PrimaryMemberRotate) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));

  net::GlobalFirstPartySets old_sets(
      kVersion,
      /*entries=*/
      {{example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                         std::nullopt)},
       {foo, net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  net::GlobalFirstPartySets current_sets(
      kVersion,
      /*entries=*/
      {{example, net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0)},
       {foo,
        net::FirstPartySetEntry(foo, net::SiteType::kPrimary, std::nullopt)}},
      /*aliases=*/{});

  // Expected diff: "https://example.test" and "https://foo.test" changed
  // primary. It would be valid to not include example.test and foo.test in the
  // result, but our logic isn't sophisticated enough yet to know that.ß
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          old_sets, /*old_config=*/net::FirstPartySetsContextConfig(),
          current_sets, /*current_config=*/net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(example, foo));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_EmptyOldSets) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));

  // Empty old_sets.
  net::GlobalFirstPartySets current_sets(
      kVersion,
      /*entries=*/
      {{example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                         std::nullopt)},
       {member1,
        net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          /*old_sets=*/net::GlobalFirstPartySets(),
          /*old_config=*/net::FirstPartySetsContextConfig(), current_sets,
          /*current_config=*/net::FirstPartySetsContextConfig()),
      IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_EmptyCurrentSets) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));

  // Empty current sets.
  net::GlobalFirstPartySets old_sets(
      kVersion,
      /*entries=*/
      {{example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                         std::nullopt)},
       {member1,
        net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  old_sets, /*old_config=*/net::FirstPartySetsContextConfig(),
                  /*current_sets=*/net::GlobalFirstPartySets(),
                  /*current_config=*/net::FirstPartySetsContextConfig()),
              IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_PolicySitesJoined) {
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));

  net::FirstPartySetsContextConfig current_config({
      {foo, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                foo, net::SiteType::kPrimary, std::nullopt))},
      {member2, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    foo, net::SiteType::kAssociated, 0))},
  });

  // "https://example.test" and "https://member2.test" joined FPSs via
  // enterprise policy. We don't clear site data upon joining, so the computed
  // diff should be empty.
  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  /*old_sets=*/net::GlobalFirstPartySets(),
                  /*old_config=*/current_config,
                  /*current_sets=*/net::GlobalFirstPartySets(), current_config),
              IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper,
     ComputeSetsDiff_PolicyRemovedSitesJoined) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));

  net::GlobalFirstPartySets sets(
      kVersion,
      /*entries=*/
      {{example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                         std::nullopt)},
       {member1,
        net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  // "https://example.test" was removed from FPSs by policy modifications.
  net::FirstPartySetsContextConfig old_config({
      {foo, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                foo, net::SiteType::kPrimary, std::nullopt))},
      {member1, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    foo, net::SiteType::kAssociated, 0))},
      {example, net::FirstPartySetEntryOverride()},
  });

  // "https://example.test" added back to FPSs.
  net::FirstPartySetsContextConfig current_config({
      {foo, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                foo, net::SiteType::kPrimary, std::nullopt))},
      {member1, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    foo, net::SiteType::kAssociated, 0))},
      {example, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    foo, net::SiteType::kAssociated, 0))},
  });

  // We don't clear site data upon joining, so the computed diff should be
  // empty.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          /*old_sets=*/sets, old_config, /*current_sets=*/sets, current_config),
      IsEmpty());
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_PolicyMemberLeft) {
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));

  net::FirstPartySetsContextConfig old_config({
      {foo, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                foo, net::SiteType::kPrimary, std::nullopt))},
      {member1, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    foo, net::SiteType::kAssociated, 0))},
      {member2, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    foo, net::SiteType::kAssociated, 0))},
  });

  // "https://member2.test" left FPSs via enterprise policy.
  net::FirstPartySetsContextConfig current_config({
      {foo, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                foo, net::SiteType::kPrimary, std::nullopt))},
      {member1, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    foo, net::SiteType::kAssociated, 0))},
  });

  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  /*old_sets=*/net::GlobalFirstPartySets(), old_config,
                  /*current_sets=*/net::GlobalFirstPartySets(), current_config),
              UnorderedElementsAre(member2));
}

TEST(FirstPartySetsHandlerDatabaseHelper, ComputeSetsDiff_PolicyPrimaryLeft) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));

  net::FirstPartySetsContextConfig old_config({
      {example, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    example, net::SiteType::kPrimary, std::nullopt))},
      {member1, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    example, net::SiteType::kAssociated, 0))},
      {member2, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    example, net::SiteType::kAssociated, 0))},
  });

  net::FirstPartySetsContextConfig current_config({
      {member1, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    member1, net::SiteType::kPrimary, std::nullopt))},
      {member2, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    member1, net::SiteType::kAssociated, 0))},
  });

  // Expected diff: "https://example.test" left FPSs, "https://member1.test" and
  // "https://member2.test" changed primary.
  // It would be valid to only have example.test in the diff, but our logic
  // isn't sophisticated enough yet to know that member1.test and member2.test
  // don't need to be included in the result.
  EXPECT_THAT(
      FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
          /*old_sets=*/net::GlobalFirstPartySets(), /*old_config=*/old_config,
          /*current_sets=*/net::GlobalFirstPartySets(), current_config),
      UnorderedElementsAre(example, member1, member2));
}

TEST(FirstPartySetsHandlerDatabaseHelper,
     ComputeSetsDiff_PolicyMembersChangeSet) {
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite bar(GURL("https://bar.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));

  net::FirstPartySetsContextConfig old_config({
      {foo, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                foo, net::SiteType::kPrimary, std::nullopt))},
      {member1, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    foo, net::SiteType::kAssociated, 0))},
      {bar, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                bar, net::SiteType::kPrimary, std::nullopt))},
      {member2, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    bar, net::SiteType::kAssociated, 0))},
  });

  net::FirstPartySetsContextConfig current_config({
      {foo, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                foo, net::SiteType::kPrimary, std::nullopt))},
      {member2, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    foo, net::SiteType::kAssociated, 0))},
      {bar, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                bar, net::SiteType::kPrimary, std::nullopt))},
      {member1, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                    bar, net::SiteType::kAssociated, 0))},
  });

  EXPECT_THAT(FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
                  /*old_sets=*/net::GlobalFirstPartySets(), old_config,
                  /*current_sets=*/net::GlobalFirstPartySets(), current_config),
              UnorderedElementsAre(member1, member2));
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
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));
  net::SchemefulSite member3(GURL("https://member3.test"));
  const std::string browser_context_id("b");

  db_helper_->PersistSets(
      browser_context_id,
      net::GlobalFirstPartySets(
          base::Version("0.0.1"),
          /*entries=*/
          {{example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                             std::nullopt)},
           {member1,
            net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)},
           {member3,
            net::FirstPartySetEntry(example, net::SiteType::kAssociated, 1)},
           {foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                         std::nullopt)},
           {member2,
            net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0)}},
          /*aliases=*/{}),
      /*config=*/net::FirstPartySetsContextConfig());

  net::GlobalFirstPartySets current_sets(
      kVersion,
      /*entries=*/
      {{example, net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                         std::nullopt)},
       {member1,
        net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)}},
      /*aliases=*/{});

  std::optional<std::pair<std::vector<net::SchemefulSite>,
                          net::FirstPartySetsCacheFilter>>
      res = db_helper_->UpdateAndGetSitesToClearForContext(
          browser_context_id, current_sets,
          /*current_config=*/net::FirstPartySetsContextConfig());

  // Expected diff: "https://foo.test", "https://member2.test" and
  // "https://member3.test" left FPSs.
  EXPECT_TRUE(res.has_value());
  EXPECT_THAT(res->first, UnorderedElementsAre(foo, member2, member3));
  EXPECT_EQ(res->second, net::FirstPartySetsCacheFilter(
                             /*filter=*/{{foo, 1}, {member2, 1}, {member3, 1}},
                             /*browser_run_id=*/1));
}

}  // namespace content
