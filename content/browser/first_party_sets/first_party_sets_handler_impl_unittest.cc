// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/first_party_set_entry.h"
#include "services/network/public/mojom/first_party_sets.mojom.h"
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

// Some of these tests overlap with FirstPartySetParser unittests, but
// overlapping test coverage isn't the worst thing.
namespace content {
namespace {
using PolicyCustomization = FirstPartySetsHandlerImpl::PolicyCustomization;
using FlattenedSets = FirstPartySetsHandlerImpl::FlattenedSets;
using SingleSet = FirstPartySetParser::SingleSet;

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

MATCHER_P(PublicSetsAre, sets_matcher, "") {
  const network::mojom::PublicFirstPartySetsPtr& public_sets = arg;
  const base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>& sets =
      public_sets->sets;
  return testing::ExplainMatchResult(sets_matcher, sets, result_listener);
}

FirstPartySetsHandlerImpl::FlattenedSets MakeFlattenedSetsFromMap(
    const base::flat_map<std::string, std::vector<std::string>>&
        owners_to_members) {
  FirstPartySetsHandlerImpl::FlattenedSets result;
  for (const auto& [owner, members] : owners_to_members) {
    net::SchemefulSite owner_site((GURL(owner)));
    result.insert(std::make_pair(
        owner_site, net::FirstPartySetEntry(owner_site, net::SiteType::kPrimary,
                                            absl::nullopt)));
    uint32_t index = 0;
    for (const std::string& member : members) {
      net::SchemefulSite member_site((GURL(member)));
      result.insert(std::make_pair(
          member_site, net::FirstPartySetEntry(
                           owner_site, net::SiteType::kAssociated, index)));
      ++index;
    }
  }
  return result;
}

// Parses `input` as a collection of primaries and their associated sites, and
// appends the results to `output`. Does not preserve indices (so it is only
// suitable for creating enterprise policy sets).
void ParseAndAppend(
    const base::flat_map<std::string, std::vector<std::string>>& input,
    std::vector<SingleSet>& output) {
  for (auto& [owner, members] : input) {
    std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>> sites;
    net::SchemefulSite owner_site((GURL(owner)));
    sites.emplace_back(
        owner_site, net::FirstPartySetEntry(owner_site, net::SiteType::kPrimary,
                                            absl::nullopt));
    for (const std::string& member : members) {
      sites.emplace_back(
          GURL(member),
          net::FirstPartySetEntry(owner_site, net::SiteType::kAssociated,
                                  absl::nullopt));
    }
    output.emplace_back(sites);
  }
}

// Creates a ParsedPolicySetLists with the replacements and additions fields
// constructed from `replacements` and `additions`.
FirstPartySetParser::ParsedPolicySetLists MakeParsedPolicyFromMap(
    const base::flat_map<std::string, std::vector<std::string>>& replacements,
    const base::flat_map<std::string, std::vector<std::string>>& additions) {
  FirstPartySetParser::ParsedPolicySetLists result;
  ParseAndAppend(replacements, result.replacements);
  ParseAndAppend(additions, result.additions);
  return result;
}

network::mojom::PublicFirstPartySetsPtr GetSetsAndWait() {
  base::test::TestFuture<network::mojom::PublicFirstPartySetsPtr> future;
  absl::optional<network::mojom::PublicFirstPartySetsPtr> result =
      FirstPartySetsHandlerImpl::GetInstance()->GetSets(future.GetCallback());
  return result.has_value() ? std::move(result).value() : future.Take();
}

network::mojom::PublicFirstPartySetsPtr MakePublicFirstPartySets(
    FlattenedSets sets) {
  network::mojom::PublicFirstPartySetsPtr public_sets =
      network::mojom::PublicFirstPartySets::New();
  public_sets->sets = std::move(sets);
  return public_sets;
}
}  // namespace

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_SitesJoined) {
  FirstPartySetsHandlerImpl::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 1)}};

  FirstPartySetsHandlerImpl::FlattenedSets current_sets = {
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
      FirstPartySetsHandlerImpl::ComputeSetsDiff(
          old_sets, /*old_policy=*/{}, current_sets, /*current_policy=*/{}),
      IsEmpty());
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_SitesLeft) {
  FirstPartySetsHandlerImpl::FlattenedSets old_sets = {
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

  FirstPartySetsHandlerImpl::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)}};

  // Expected diff: "https://foo.test", "https://member2.test" and
  // "https://member3.test" left FPSs.
  EXPECT_THAT(
      FirstPartySetsHandlerImpl::ComputeSetsDiff(
          old_sets, /*old_policy=*/{}, current_sets, /*current_policy=*/{}),
      UnorderedElementsAre(SerializesTo("https://foo.test"),
                           SerializesTo("https://member2.test"),
                           SerializesTo("https://member3.test")));
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_OwnerChanged) {
  FirstPartySetsHandlerImpl::FlattenedSets old_sets = {
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

  FirstPartySetsHandlerImpl::FlattenedSets current_sets = {
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
      FirstPartySetsHandlerImpl::ComputeSetsDiff(
          old_sets, /*old_policy=*/{}, current_sets, /*current_policy=*/{}),
      UnorderedElementsAre(SerializesTo("https://member3.test")));
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_OwnerLeft) {
  FirstPartySetsHandlerImpl::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)},
      {net::SchemefulSite(GURL("https://bar.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 1)}};

  FirstPartySetsHandlerImpl::FlattenedSets current_sets = {
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
      FirstPartySetsHandlerImpl::ComputeSetsDiff(
          old_sets, /*old_policy=*/{}, current_sets, /*current_policy=*/{}),
      UnorderedElementsAre(SerializesTo("https://example.test"),
                           SerializesTo("https://foo.test"),
                           SerializesTo("https://bar.test")));
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_OwnerMemberRotate) {
  FirstPartySetsHandlerImpl::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)}};

  FirstPartySetsHandlerImpl::FlattenedSets current_sets = {
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
      FirstPartySetsHandlerImpl::ComputeSetsDiff(
          old_sets, /*old_policy=*/{}, current_sets, /*current_policy=*/{}),
      UnorderedElementsAre(SerializesTo("https://example.test"),
                           SerializesTo("https://foo.test")));
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_EmptyOldSets) {
  // Empty old_sets.
  FirstPartySetsHandlerImpl::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)}};

  EXPECT_THAT(FirstPartySetsHandlerImpl::ComputeSetsDiff(
                  /*old_sets=*/{}, /*old_policy=*/{}, current_sets,
                  /*current_policy=*/{}),
              IsEmpty());
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_EmptyCurrentSets) {
  // Empty current sets.
  FirstPartySetsHandlerImpl::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)}};

  EXPECT_THAT(FirstPartySetsHandlerImpl::ComputeSetsDiff(
                  old_sets, /*old_policy=*/{}, /*current_sets=*/{},
                  /*current_policy=*/{}),
              IsEmpty());
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_PolicySitesJoined) {
  FirstPartySetsHandlerImpl::PolicyCustomization current_policy = {
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
  EXPECT_THAT(FirstPartySetsHandlerImpl::ComputeSetsDiff(
                  /*old_sets=*/{}, /*old_policy=*/{}, /*current_sets=*/{},
                  current_policy),
              IsEmpty());
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_PolicyRemovedSitesJoined) {
  FirstPartySetsHandlerImpl::FlattenedSets sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::FirstPartySetEntry(net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)}};

  // "https://example.test" was removed from FPSs by policy modifications.
  FirstPartySetsHandlerImpl::PolicyCustomization old_policy = {
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
      {net::SchemefulSite(GURL("https://example.test")), absl::nullopt},
  };

  // "https://example.test" added back to FPSs.
  FirstPartySetsHandlerImpl::PolicyCustomization current_policy = {
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
      FirstPartySetsHandlerImpl::ComputeSetsDiff(
          /*old_sets=*/sets, old_policy, /*current_sets=*/sets, current_policy),
      IsEmpty());
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_PolicyMemberLeft) {
  FirstPartySetsHandlerImpl::PolicyCustomization old_policy = {
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
  FirstPartySetsHandlerImpl::PolicyCustomization current_policy = {
      {net::SchemefulSite(GURL("https://foo.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kPrimary, absl::nullopt)}},
      {net::SchemefulSite(GURL("https://member1.test")),
       {net::FirstPartySetEntry(net::SchemefulSite(GURL("https://foo.test")),
                                net::SiteType::kAssociated, 0)}},
  };

  EXPECT_THAT(
      FirstPartySetsHandlerImpl::ComputeSetsDiff(
          /*old_sets=*/{}, old_policy, /*current_sets=*/{}, current_policy),
      UnorderedElementsAre(SerializesTo("https://member2.test")));
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_PolicyOwnerLeft) {
  FirstPartySetsHandlerImpl::PolicyCustomization old_policy = {
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

  FirstPartySetsHandlerImpl::PolicyCustomization current_policy = {
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
  EXPECT_THAT(FirstPartySetsHandlerImpl::ComputeSetsDiff(
                  /*old_sets=*/{}, /*old_policy=*/old_policy,
                  /*current_sets=*/{}, current_policy),
              UnorderedElementsAre(SerializesTo("https://example.test"),
                                   SerializesTo("https://member1.test"),
                                   SerializesTo("https://member2.test")));
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_PolicyMembersChangeSet) {
  FirstPartySetsHandlerImpl::PolicyCustomization old_policy = {
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

  FirstPartySetsHandlerImpl::PolicyCustomization current_policy = {
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
      FirstPartySetsHandlerImpl::ComputeSetsDiff(
          /*old_sets=*/{}, old_policy, /*current_sets=*/{}, current_policy),
      UnorderedElementsAre(SerializesTo("https://member1.test"),
                           SerializesTo("https://member2.test")));
}

TEST(FirstPartySetsHandlerImpl, ValidateEnterprisePolicy_ValidPolicy) {
  base::Value input = base::JSONReader::Read(R"(
             {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  }
                ],
                "additions": [
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://member2.test"]
                  }
                ]
              }
            )")
                          .value();
  EXPECT_EQ(FirstPartySetsHandler::ValidateEnterprisePolicy(input.GetDict()),
            absl::nullopt);
}

TEST(FirstPartySetsHandlerImpl, ValidateEnterprisePolicy_InvalidPolicy) {
  // Some input that matches our policies schema but breaks FPS invariants.
  // For more test coverage, see the ParseSetsFromEnterprisePolicy unit tests.
  base::Value input = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  }
                ],
                "additions": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member2.test"]
                  }
                ]
              }
            )")
                          .value();
  FirstPartySetsHandler::PolicyParsingError expected_error{
      FirstPartySetsHandler::ParseError::kNonDisjointSets,
      FirstPartySetsHandler::PolicySetType::kAddition, 0};
  EXPECT_EQ(FirstPartySetsHandler::ValidateEnterprisePolicy(input.GetDict()),
            expected_error);
}

class FirstPartySetsHandlerImplTest : public ::testing::Test {
 public:
  explicit FirstPartySetsHandlerImplTest(bool enabled) {
    FirstPartySetsHandlerImpl::GetInstance()->SetEnabledForTesting(enabled);

    CHECK(scoped_dir_.CreateUniqueTempDir());
    CHECK(PathExists(scoped_dir_.GetPath()));

    persisted_sets_path_ = scoped_dir_.GetPath().Append(
        FILE_PATH_LITERAL("persisted_first_party_sets.json"));
  }

  base::File WritePublicSetsFile(base::StringPiece content) {
    base::FilePath path =
        scoped_dir_.GetPath().Append(FILE_PATH_LITERAL("sets_file.json"));
    CHECK(base::WriteFile(path, content));

    return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  }

  void TearDown() override {
    FirstPartySetsHandlerImpl::GetInstance()->ResetForTesting();
  }

  base::test::TaskEnvironment& env() { return env_; }

 protected:
  base::ScopedTempDir scoped_dir_;
  base::FilePath persisted_sets_path_;
  base::test::TaskEnvironment env_;
};

class FirstPartySetsHandlerImplDisabledTest
    : public FirstPartySetsHandlerImplTest {
 public:
  FirstPartySetsHandlerImplDisabledTest()
      : FirstPartySetsHandlerImplTest(false) {}
};

TEST_F(FirstPartySetsHandlerImplDisabledTest, IgnoresValid) {
  // Persisted sets are expected to be loaded with the provided path.
  FirstPartySetsHandlerImpl::GetInstance()->Init(scoped_dir_.GetPath(),
                                                 /*flag_value=*/"");

  env().RunUntilIdle();

  // TODO(shuuran@chromium.org): test site state is cleared.

  // First-Party Sets is disabled, write an empty persisted sets to disk.
  std::string got;
  ASSERT_TRUE(base::ReadFileToString(persisted_sets_path_, &got));
  EXPECT_EQ(got, "{}");
}

class FirstPartySetsHandlerImplEnabledTest
    : public FirstPartySetsHandlerImplTest {
 public:
  FirstPartySetsHandlerImplEnabledTest()
      : FirstPartySetsHandlerImplTest(true) {}
};

TEST_F(FirstPartySetsHandlerImplEnabledTest, EmptyPersistedSetsDir) {
  // Empty `user_data_dir` will fail to load persisted sets, but that will not
  // prevent `on_sets_ready` from being invoked.
  FirstPartySetsHandlerImpl::GetInstance()->Init(
      /*user_data_dir=*/{},
      /*flag_value=*/"https://example.test,https://member1.test");

  EXPECT_THAT(GetSetsAndWait(),
              PublicSetsAre(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated, 0)))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       Successful_PersistedSetsFileNotExist) {
  FirstPartySetsHandlerImpl::GetInstance()
      ->SetEmbedderWillProvidePublicSetsForTesting(true);
  const std::string input = R"({"owner": "https://foo.test", )"
                            R"("members": ["https://member2.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  FirstPartySetsHandlerImpl::GetInstance()->SetPublicFirstPartySets(
      WritePublicSetsFile(input));

  // Persisted sets are expected to be loaded with the provided path.
  FirstPartySetsHandlerImpl::GetInstance()->Init(
      scoped_dir_.GetPath(),
      /*flag_value=*/"https://example.test,https://member1.test");
  EXPECT_THAT(GetSetsAndWait(),
              PublicSetsAre(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated, 0)),
                  Pair(SerializesTo("https://foo.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member2.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kAssociated, 0)))));

  env().RunUntilIdle();

  std::string got;
  ASSERT_TRUE(base::ReadFileToString(persisted_sets_path_, &got));
  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(got),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated, absl::nullopt)),
                  Pair(SerializesTo("https://foo.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member2.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kAssociated, absl::nullopt))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest, Successful_PersistedSetsEmpty) {
  FirstPartySetsHandlerImpl::GetInstance()
      ->SetEmbedderWillProvidePublicSetsForTesting(true);
  ASSERT_TRUE(base::WriteFile(persisted_sets_path_, "{}"));

  const std::string input = R"({"owner": "https://foo.test", )"
                            R"("members": ["https://member2.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  FirstPartySetsHandlerImpl::GetInstance()->SetPublicFirstPartySets(
      WritePublicSetsFile(input));

  // Persisted sets are expected to be loaded with the provided path.
  FirstPartySetsHandlerImpl::GetInstance()->Init(
      scoped_dir_.GetPath(),
      /*flag_value=*/"https://example.test,https://member1.test");
  EXPECT_THAT(GetSetsAndWait(),
              PublicSetsAre(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated, 0)),
                  Pair(SerializesTo("https://foo.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member2.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kAssociated, 0)))));

  env().RunUntilIdle();

  std::string got;
  ASSERT_TRUE(base::ReadFileToString(persisted_sets_path_, &got));
  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(got),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member1.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated, absl::nullopt)),
                  Pair(SerializesTo("https://foo.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member2.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://foo.test")),
                           net::SiteType::kAssociated, absl::nullopt))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       GetSetsIfEnabledAndReady_AfterSetsReady) {
  FirstPartySetsHandlerImpl::GetInstance()
      ->SetEmbedderWillProvidePublicSetsForTesting(true);
  ASSERT_TRUE(base::WriteFile(persisted_sets_path_, "{}"));

  const std::string input = R"({"owner": "https://example.test", )"
                            R"("members": ["https://member.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  FirstPartySetsHandlerImpl::GetInstance()->SetPublicFirstPartySets(
      WritePublicSetsFile(input));

  // Persisted sets are expected to be loaded with the provided path.
  FirstPartySetsHandlerImpl::GetInstance()->Init(scoped_dir_.GetPath(),
                                                 /*flag_value=*/"");
  EXPECT_THAT(GetSetsAndWait(),
              PublicSetsAre(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated, 0)))));

  env().RunUntilIdle();

  std::string got;
  ASSERT_TRUE(base::ReadFileToString(persisted_sets_path_, &got));
  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(got),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated, absl::nullopt))));

  EXPECT_THAT(
      FirstPartySetsHandlerImpl::GetInstance()->GetSets(base::NullCallback()),
      testing::Optional(PublicSetsAre(UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://example.test")),
                   net::SiteType::kPrimary, absl::nullopt)),
          Pair(SerializesTo("https://member.test"),
               net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://example.test")),
                   net::SiteType::kAssociated, 0))))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       GetSetsIfEnabledAndReady_BeforeSetsReady) {
  FirstPartySetsHandlerImpl::GetInstance()
      ->SetEmbedderWillProvidePublicSetsForTesting(true);
  ASSERT_TRUE(base::WriteFile(persisted_sets_path_, "{}"));

  // Call GetSets before the sets are ready, and before Init has been called.
  base::test::TestFuture<network::mojom::PublicFirstPartySetsPtr> future;
  EXPECT_EQ(
      FirstPartySetsHandlerImpl::GetInstance()->GetSets(future.GetCallback()),
      absl::nullopt);

  // Persisted sets are expected to be loaded with the provided path.
  FirstPartySetsHandlerImpl::GetInstance()->Init(scoped_dir_.GetPath(),
                                                 /*flag_value=*/"");

  const std::string input = R"({"owner": "https://example.test", )"
                            R"("members": ["https://member.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  FirstPartySetsHandlerImpl::GetInstance()->SetPublicFirstPartySets(
      WritePublicSetsFile(input));

  EXPECT_THAT(future.Get(),
              PublicSetsAre(UnorderedElementsAre(
                  Pair(SerializesTo("https://example.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kPrimary, absl::nullopt)),
                  Pair(SerializesTo("https://member.test"),
                       net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://example.test")),
                           net::SiteType::kAssociated, 0)))));

  EXPECT_THAT(
      FirstPartySetsHandlerImpl::GetInstance()->GetSets(base::NullCallback()),
      testing::Optional(PublicSetsAre(UnorderedElementsAre(
          Pair(SerializesTo("https://example.test"),
               net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://example.test")),
                   net::SiteType::kPrimary, absl::nullopt)),
          Pair(SerializesTo("https://member.test"),
               net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://example.test")),
                   net::SiteType::kAssociated, 0))))));
}

class FirstPartySetsHandlerGetCustomizationForPolicyTest
    : public FirstPartySetsHandlerImplEnabledTest {
 public:
  FirstPartySetsHandlerGetCustomizationForPolicyTest() {
    FirstPartySetsHandlerImpl::GetInstance()
        ->SetEmbedderWillProvidePublicSetsForTesting(true);
    FirstPartySetsHandlerImpl::GetInstance()->Init(scoped_dir_.GetPath(),
                                                   /*flag_value=*/"");
  }

  // Writes the public list of First-Party Sets which GetCustomizationForPolicy
  // awaits.
  //
  // Initializes the First-Party Sets with the following relationship:
  //
  // [
  //   {
  //     "owner": "https://owner1.test",
  //     "members": ["https://member1.test", "https://member2.test"]
  //   }
  // ]
  void InitPublicFirstPartySets() {
    const std::string input =
        R"({"owner": "https://owner1.test", )"
        R"("members": ["https://member1.test", "https://member2.test"]})";
    ASSERT_TRUE(base::JSONReader::Read(input));
    FirstPartySetsHandlerImpl::GetInstance()->SetPublicFirstPartySets(
        WritePublicSetsFile(input));

    FirstPartySetsHandlerImpl::FlattenedSets public_sets =
        MakeFlattenedSetsFromMap(
            {{"https://owner1.test",
              {"https://member1.test", "https://member2.test"}}});

    ASSERT_THAT(GetSetsAndWait(), PublicSetsAre(public_sets));
  }

 protected:
  base::OnceCallback<void(PolicyCustomization)> GetCustomizationCallback() {
    return future_.GetCallback();
  }

  PolicyCustomization GetCustomization() { return future_.Take(); }

 private:
  base::test::TestFuture<FirstPartySetsHandler::PolicyCustomization> future_;
};

TEST_F(FirstPartySetsHandlerGetCustomizationForPolicyTest,
       DefaultOverridesPolicy_DefaultCustomizations) {
  base::Value policy = base::JSONReader::Read(R"({})").value();
  FirstPartySetsHandlerImpl::GetInstance()->GetCustomizationForPolicy(
      policy.GetDict(), GetCustomizationCallback());

  InitPublicFirstPartySets();
  EXPECT_THAT(GetCustomization(), PolicyCustomization());
}

TEST_F(FirstPartySetsHandlerGetCustomizationForPolicyTest,
       MalformedOverridesPolicy_DefaultCustomizations) {
  base::Value policy = base::JSONReader::Read(R"({
    "replacements": 123,
    "additions": true
  })")
                           .value();
  FirstPartySetsHandlerImpl::GetInstance()->GetCustomizationForPolicy(
      policy.GetDict(), GetCustomizationCallback());

  InitPublicFirstPartySets();
  EXPECT_THAT(GetCustomization(), PolicyCustomization());
}

TEST_F(FirstPartySetsHandlerGetCustomizationForPolicyTest,
       NonDefaultOverridesPolicy_NonDefaultCustomizations) {
  base::Value policy = base::JSONReader::Read(R"(
                {
                "replacements": [
                  {
                    "owner": "https://member1.test",
                    "members": ["https://owner3.test"]
                  }
                ],
                "additions": [
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://member2.test"]
                  }
                ]
              }
            )")
                           .value();
  FirstPartySetsHandlerImpl::GetInstance()->GetCustomizationForPolicy(
      policy.GetDict(), GetCustomizationCallback());

  InitPublicFirstPartySets();
  EXPECT_THAT(GetCustomization(),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://owner1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner2.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://member1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://member1.test")),
                           net::SiteType::kPrimary, absl::nullopt))),
                  Pair(SerializesTo("https://owner3.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://member1.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://member2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner2.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://owner2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner2.test")),
                           net::SiteType::kPrimary, absl::nullopt)))));
}

TEST(FirstPartySetsProfilePolicyCustomizations, EmptyPolicySetLists) {
  EXPECT_THAT(FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
                  MakePublicFirstPartySets(MakeFlattenedSetsFromMap(
                      {{"https://owner1.test", {"https://member1.test"}}})),
                  MakeParsedPolicyFromMap({}, {})),
              FirstPartySetsHandlerImpl::PolicyCustomization());
}

TEST(FirstPartySetsProfilePolicyCustomizations,
     Replacements_NoIntersection_NoRemoval) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakePublicFirstPartySets(MakeFlattenedSetsFromMap(
              {{"https://owner1.test", {"https://member1.test"}}})),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://owner2.test",
                                 {"https://member2.test"}}},
              /*additions=*/{}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner2.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://owner2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner2.test")),
                           net::SiteType::kPrimary, absl::nullopt)))));
}

// The common member between the policy and existing set is removed from its
// previous set.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Replacements_ReplacesExistingMember_RemovedFromFormerSet) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakePublicFirstPartySets(MakeFlattenedSetsFromMap(
              {{"https://owner1.test",
                {"https://member1a.test", "https://member1b.test"}}})),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://owner2.test",
                                 {"https://member1b.test"}}},
              /*additions=*/{}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1b.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner2.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://owner2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner2.test")),
                           net::SiteType::kPrimary, absl::nullopt)))));
}

// The common owner between the policy and existing set is removed and its
// former members are removed since they are now unowned.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Replacements_ReplacesExistingOwner_RemovesFormerMembers) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakePublicFirstPartySets(MakeFlattenedSetsFromMap(
              {{"https://owner1.test",
                {"https://member1a.test", "https://member1b.test"}}})),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://owner1.test",
                                 {"https://member2.test"}}},
              /*additions=*/{}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner1.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://owner1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner1.test")),
                           net::SiteType::kPrimary, absl::nullopt))),
                  Pair(SerializesTo("https://member1a.test"), absl::nullopt),
                  Pair(SerializesTo("https://member1b.test"), absl::nullopt)));
}

// The common member between the policy and existing set is removed and any
// leftover singletons are deleted.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Replacements_ReplacesExistingMember_RemovesSingletons) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakePublicFirstPartySets(MakeFlattenedSetsFromMap(
              {{"https://owner1.test", {"https://member1.test"}}})),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://owner3.test",
                                 {"https://member1.test"}}},
              /*additions=*/{}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner3.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://owner3.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner3.test")),
                           net::SiteType::kPrimary, absl::nullopt))),
                  Pair(SerializesTo("https://owner1.test"), absl::nullopt)));
}

// The policy set and the existing set have nothing in common so the policy set
// gets added in without updating the existing set.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Additions_NoIntersection_AddsWithoutUpdating) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakePublicFirstPartySets(MakeFlattenedSetsFromMap(
              {{"https://owner1.test", {"https://member1.test"}}})),
          MakeParsedPolicyFromMap(
              /*replacements=*/{},
              /*additions=*/{
                  {"https://owner2.test", {"https://member2.test"}}}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner2.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://owner2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner2.test")),
                           net::SiteType::kPrimary, absl::nullopt)))));
}

// The owner of a policy set is also a member in an existing set.
// The policy set absorbs all sites in the existing set into its members.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Additions_PolicyOwnerIsExistingMember_PolicySetAbsorbsExistingSet) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakePublicFirstPartySets(MakeFlattenedSetsFromMap(
              {{"https://owner1.test", {"https://member2.test"}}})),
          MakeParsedPolicyFromMap(
              /*replacements=*/{},
              /*additions=*/{
                  {"https://member2.test",
                   {"https://member2a.test", "https://member2b.test"}}}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://owner1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://member2.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://member2a.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://member2.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://member2b.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://member2.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://member2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://member2.test")),
                           net::SiteType::kPrimary, absl::nullopt)))));
}

// The owner of a policy set is also an owner of an existing set.
// The policy set absorbs all of its owner's existing members into its members.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Additions_PolicyOwnerIsExistingOwner_PolicySetAbsorbsExistingMembers) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakePublicFirstPartySets(MakeFlattenedSetsFromMap(
              {{"https://owner1.test",
                {"https://member1.test", "https://member3.test"}}})),
          MakeParsedPolicyFromMap(
              /*replacements=*/{},
              /*additions=*/{
                  {"https://owner1.test", {"https://member2.test"}}}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner1.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://member1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner1.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://member3.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner1.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://owner1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner1.test")),
                           net::SiteType::kPrimary, absl::nullopt)))));
}

TEST(FirstPartySetsProfilePolicyCustomizations,
     TransitiveOverlap_TwoCommonOwners) {
  net::SchemefulSite owner0(GURL("https://owner0.test"));
  net::SchemefulSite member0(GURL("https://member0.test"));
  net::SchemefulSite owner1(GURL("https://owner1.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite owner2(GURL("https://owner2.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));
  net::SchemefulSite owner42(GURL("https://owner42.test"));
  net::SchemefulSite member42(GURL("https://member42.test"));
  // {owner1, {member1}} and {owner2, {member2}} transitively overlap with the
  // existing set.
  // owner1 takes ownership of the normalized addition set since it was
  // provided first.
  // The other addition sets are unaffected.
  EXPECT_THAT(
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakePublicFirstPartySets(MakeFlattenedSetsFromMap(
              {{"https://owner1.test", {"https://owner2.test"}}})),
          FirstPartySetParser::ParsedPolicySetLists(
              /*replacement_list=*/{},
              {
                  SingleSet({{owner0, net::FirstPartySetEntry(
                                          owner0, net::SiteType::kPrimary,
                                          absl::nullopt)},
                             {member0, net::FirstPartySetEntry(
                                           owner0, net::SiteType::kAssociated,
                                           absl::nullopt)}}),
                  SingleSet({{owner1, net::FirstPartySetEntry(
                                          owner1, net::SiteType::kPrimary,
                                          absl::nullopt)},
                             {member1, net::FirstPartySetEntry(
                                           owner1, net::SiteType::kAssociated,
                                           absl::nullopt)}}),
                  SingleSet({{owner2, net::FirstPartySetEntry(
                                          owner2, net::SiteType::kPrimary,
                                          absl::nullopt)},
                             {member2, net::FirstPartySetEntry(
                                           owner2, net::SiteType::kAssociated,
                                           absl::nullopt)}}),
                  SingleSet({{owner42, net::FirstPartySetEntry(
                                           owner42, net::SiteType::kPrimary,
                                           absl::nullopt)},
                             {member42, net::FirstPartySetEntry(
                                            owner42, net::SiteType::kAssociated,
                                            absl::nullopt)}}),
              })),
      UnorderedElementsAre(
          Pair(member0,
               absl::make_optional(net::FirstPartySetEntry(
                   owner0, net::SiteType::kAssociated, absl::nullopt))),
          Pair(member1,
               absl::make_optional(net::FirstPartySetEntry(
                   owner1, net::SiteType::kAssociated, absl::nullopt))),
          Pair(member2,
               absl::make_optional(net::FirstPartySetEntry(
                   owner1, net::SiteType::kAssociated, absl::nullopt))),
          Pair(member42,
               absl::make_optional(net::FirstPartySetEntry(
                   owner42, net::SiteType::kAssociated, absl::nullopt))),
          Pair(owner0, absl::make_optional(net::FirstPartySetEntry(
                           owner0, net::SiteType::kPrimary, absl::nullopt))),
          Pair(owner1, absl::make_optional(net::FirstPartySetEntry(
                           owner1, net::SiteType::kPrimary, absl::nullopt))),
          Pair(owner2, absl::make_optional(net::FirstPartySetEntry(
                           owner1, net::SiteType::kAssociated, absl::nullopt))),
          Pair(owner42,
               absl::make_optional(net::FirstPartySetEntry(
                   owner42, net::SiteType::kPrimary, absl::nullopt)))));
}

TEST(FirstPartySetsProfilePolicyCustomizations,
     TransitiveOverlap_TwoCommonMembers) {
  net::SchemefulSite owner0(GURL("https://owner0.test"));
  net::SchemefulSite member0(GURL("https://member0.test"));
  net::SchemefulSite owner1(GURL("https://owner1.test"));
  net::SchemefulSite member1(GURL("https://member1.test"));
  net::SchemefulSite owner2(GURL("https://owner2.test"));
  net::SchemefulSite member2(GURL("https://member2.test"));
  net::SchemefulSite owner42(GURL("https://owner42.test"));
  net::SchemefulSite member42(GURL("https://member42.test"));
  // {owner1, {member1}} and {owner2, {member2}} transitively overlap with the
  // existing set.
  // owner2 takes ownership of the normalized addition set since it was
  // provided first.
  // The other addition sets are unaffected.
  EXPECT_THAT(
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakePublicFirstPartySets(MakeFlattenedSetsFromMap(
              {{"https://owner2.test", {"https://owner1.test"}}})),
          FirstPartySetParser::ParsedPolicySetLists(
              /*replacement_list=*/{},
              {
                  SingleSet({{owner0, net::FirstPartySetEntry(
                                          owner0, net::SiteType::kPrimary,
                                          absl::nullopt)},
                             {member0, net::FirstPartySetEntry(
                                           owner0, net::SiteType::kAssociated,
                                           absl::nullopt)}}),
                  SingleSet({{owner2, net::FirstPartySetEntry(
                                          owner2, net::SiteType::kPrimary,
                                          absl::nullopt)},
                             {member2, net::FirstPartySetEntry(
                                           owner2, net::SiteType::kAssociated,
                                           absl::nullopt)}}),
                  SingleSet({{owner1, net::FirstPartySetEntry(
                                          owner1, net::SiteType::kPrimary,
                                          absl::nullopt)},
                             {member1, net::FirstPartySetEntry(
                                           owner1, net::SiteType::kAssociated,
                                           absl::nullopt)}}),
                  SingleSet({{owner42, net::FirstPartySetEntry(
                                           owner42, net::SiteType::kPrimary,
                                           absl::nullopt)},
                             {member42, net::FirstPartySetEntry(
                                            owner42, net::SiteType::kAssociated,
                                            absl::nullopt)}}),
              })),
      UnorderedElementsAre(
          Pair(member0,
               absl::make_optional(net::FirstPartySetEntry(
                   owner0, net::SiteType::kAssociated, absl::nullopt))),
          Pair(member1,
               absl::make_optional(net::FirstPartySetEntry(
                   owner2, net::SiteType::kAssociated, absl::nullopt))),
          Pair(member2,
               absl::make_optional(net::FirstPartySetEntry(
                   owner2, net::SiteType::kAssociated, absl::nullopt))),
          Pair(member42,
               absl::make_optional(net::FirstPartySetEntry(
                   owner42, net::SiteType::kAssociated, absl::nullopt))),
          Pair(owner0, absl::make_optional(net::FirstPartySetEntry(
                           owner0, net::SiteType::kPrimary, absl::nullopt))),
          Pair(owner1, absl::make_optional(net::FirstPartySetEntry(
                           owner2, net::SiteType::kAssociated, absl::nullopt))),
          Pair(owner2, absl::make_optional(net::FirstPartySetEntry(
                           owner2, net::SiteType::kPrimary, absl::nullopt))),
          Pair(owner42,
               absl::make_optional(net::FirstPartySetEntry(
                   owner42, net::SiteType::kPrimary, absl::nullopt)))));
}

// Existing set overlaps with both replacement and addition set.
TEST(FirstPartySetsProfilePolicyCustomizations,
     ReplacementsAndAdditions_SetListsOverlapWithSameExistingSet) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakePublicFirstPartySets(MakeFlattenedSetsFromMap(
              {{"https://owner1.test",
                {"https://member1.test", "https://member2.test"}}})),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://owner0.test",
                                 {"https://member1.test"}}},
              /*additions=*/{
                  {"https://owner1.test", {"https://new-member1.test"}}}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner0.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://owner0.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner0.test")),
                           net::SiteType::kPrimary, absl::nullopt))),
                  Pair(SerializesTo("https://new-member1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner1.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://member2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner1.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://owner1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://owner1.test")),
                           net::SiteType::kPrimary, absl::nullopt)))));
}
}  // namespace content
