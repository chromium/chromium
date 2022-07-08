// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_loader.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

namespace content {

using SingleSet = FirstPartySetsLoader::SingleSet;

namespace {

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

void SetComponentSets(FirstPartySetsLoader& loader, base::StringPiece content) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("sets_file.json"));
  CHECK(base::WriteFile(path, content));

  loader.SetComponentSets(
      base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ));
}

enum class FirstPartySetsSource { kPublicSets, kCommandLineSet };

FirstPartySetsLoader::FlattenedSets MakeFlattenedSetsFromMap(
    const base::flat_map<std::string, std::vector<std::string>>&
        owners_to_members) {
  FirstPartySetsLoader::FlattenedSets result;
  for (const auto& [owner, members] : owners_to_members) {
    net::SchemefulSite owner_site = net::SchemefulSite(GURL(owner));
    result.emplace(owner_site, owner_site);
    for (const std::string& member : members) {
      net::SchemefulSite member_site = net::SchemefulSite(GURL(member));
      result.emplace(member_site, owner_site);
    }
  }
  return result;
}

}  // namespace

class FirstPartySetsLoaderTest : public ::testing::Test {
 public:
  FirstPartySetsLoaderTest() : loader_(future_.GetCallback()) {}

  FirstPartySetsLoader& loader() { return loader_; }

  base::flat_map<net::SchemefulSite, net::SchemefulSite> WaitAndGetResult() {
    return future_.Get();
  }

 private:
  base::test::TaskEnvironment env_;
  base::test::TestFuture<base::flat_map<net::SchemefulSite, net::SchemefulSite>>
      future_;
  FirstPartySetsLoader loader_;
};

TEST_F(FirstPartySetsLoaderTest, IgnoresInvalidFile) {
  loader().SetManuallySpecifiedSet("");
  const std::string input = "certainly not valid JSON";
  SetComponentSets(loader(), input);
  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, ParsesComponent) {
  SetComponentSets(loader(), "");
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");
  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, AcceptsMinimal) {
  const std::string input =
      "{\"owner\": \"https://example.test\",\"members\": "
      "[\"https://aaaa.test\",],}";
  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://aaaa.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTest, AcceptsMultipleSets) {
  const std::string input =
      "{\"owner\": \"https://example.test\",\"members\": "
      "[\"https://member1.test\"]}\n"
      "{\"owner\": \"https://foo.test\",\"members\": "
      "[\"https://member2.test\"]}";

  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test"))));
}

TEST_F(FirstPartySetsLoaderTest, SetComponentSets_Idempotent) {
  std::string input =
      R"({"owner": "https://example.test", "members": ["https://member1.test"]}
{"owner": "https://foo.test", "members": ["https://member2.test"]})";

  std::string input2 = R"({ "owner": "https://example2.test", "members":)"
                       R"( ["https://member1.test"]}
{"owner": "https://foo2.test", "members": ["https://member2.test"]})";

  SetComponentSets(loader(), input);
  SetComponentSets(loader(), input2);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(),
              // The second call to SetComponentSets should have had no effect.
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test"))));
}

TEST_F(FirstPartySetsLoaderTest, OwnerIsOnlyMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://example.test"]}
{"owner": "https://foo.test", "members": ["https://member2.test"]})";

  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, OwnerIsMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members":)"
      R"( ["https://example.test", "https://member1.test"]}
{"owner": "https://foo.test", "members": ["https://member2.test"]})";
  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, RepeatedMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members":)"
      R"( ["https://member1.test", "https://member2.test",)"
      R"( "https://member1.test"]}
{"owner": "https://foo.test", "members": ["https://member3.test"]})";

  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Invalid_TooSmall) {
  loader().SetManuallySpecifiedSet("https://example.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Invalid_NotOrigins) {
  loader().SetManuallySpecifiedSet("https://example.test,member1");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Invalid_NotHTTPS) {
  loader().SetManuallySpecifiedSet("https://example.test,http://member1.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_Invalid_RegisteredDomain_Owner) {
  loader().SetManuallySpecifiedSet(
      "https://www.example.test..,https://www.member.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_Invalid_RegisteredDomain_Member) {
  loader().SetManuallySpecifiedSet(
      "https://www.example.test,https://www.member.test..");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Valid_SingleMember) {
  loader().SetManuallySpecifiedSet("https://example.test,https://member.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_Valid_SingleMember_RegisteredDomain) {
  loader().SetManuallySpecifiedSet(
      "https://www.example.test,https://www.member.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Valid_MultipleMembers) {
  loader().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_Valid_OwnerIsOnlyMember) {
  loader().SetManuallySpecifiedSet("https://example.test,https://example.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Valid_OwnerIsMember) {
  loader().SetManuallySpecifiedSet(
      "https://example.test,https://example.test,https://member1.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Valid_RepeatedMember) {
  loader().SetManuallySpecifiedSet(R"(https://example.test,
https://member1.test,
https://member2.test,
https://member1.test)");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_DeduplicatesOwnerOwner) {
  const std::string input = R"({"owner": "https://example.test", "members": )"
                            R"(["https://member2.test", "https://member3.test"]}
{"owner": "https://bar.test", "members": ["https://member4.test"]})";
  SetComponentSets(loader(), input);
  loader().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://bar.test"),
                                        SerializesTo("https://bar.test")),
                                   Pair(SerializesTo("https://member4.test"),
                                        SerializesTo("https://bar.test"))));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_DeduplicatesOwnerMember) {
  const std::string input = R"({"owner": "https://foo.test", "members": )"
                            R"(["https://member1.test", "https://example.test"]}
{"owner": "https://bar.test", "members": ["https://member2.test"]})";
  SetComponentSets(loader(), input);
  loader().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member3.test");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member3.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://bar.test"),
                                        SerializesTo("https://bar.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://bar.test"))));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_DeduplicatesMemberOwner) {
  const std::string input = R"({"owner": "https://foo.test", "members": )"
                            R"(["https://member1.test", "https://member2.test"]}
{"owner": "https://member3.test", "members": ["https://member4.test"]})";
  SetComponentSets(loader(), input);
  loader().SetManuallySpecifiedSet("https://example.test,https://member3.test");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member3.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test"))));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_DeduplicatesMemberMember) {
  const std::string input = R"({"owner": "https://foo.test", "members": )"
                            R"(["https://member2.test", "https://member3.test"]}
{"owner": "https://bar.test", "members": ["https://member4.test"]})";
  SetComponentSets(loader(), input);
  loader().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
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
                                        SerializesTo("https://bar.test"))));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_PrunesInducedSingletons) {
  const std::string input =
      R"({"owner": "https://foo.test", "members": ["https://member1.test"]})";
  SetComponentSets(loader(), input);
  loader().SetManuallySpecifiedSet("https://example.test,https://member1.test");

  // If we just erased entries that overlapped with the manually-supplied
  // set, https://foo.test would be left as a singleton set. But since we
  // disallow singleton sets, we ensure that such cases are caught and
  // removed.
  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test"))));
}

// There is no overlap between the existing sets and the addition sets, so
// normalization should be a noop.
TEST(FirstPartySetsLoaderTestNormalizeAdditionSets,
     NoOverlap_AdditionSetsAreUnchanged) {
  const FirstPartySetsLoader::FlattenedSets existing_sets(
      MakeFlattenedSetsFromMap(
          {{"https://owner42.test", {"https://member42.test"}}}));
  const std::vector<FirstPartySetsLoader::SingleSet> additions{
      SingleSet(net::SchemefulSite(GURL("https://owner1.test")),
                {net::SchemefulSite(GURL("https://member1.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner2.test")),
                {net::SchemefulSite(GURL("https://member2.test"))})};

  EXPECT_THAT(
      FirstPartySetsLoader::NormalizeAdditionSets(existing_sets, additions),
      UnorderedElementsAreArray(additions));
}

// There is no transitive overlap since only all the overlaps are from the same
// addition set, so normalization should be a noop.
TEST(FirstPartySetsLoaderTestNormalizeAdditionSets,
     NoTransitiveOverlap_SingleSetMultipleOverlaps_AdditionSetsAreUnchanged) {
  const FirstPartySetsLoader::FlattenedSets existing_sets(
      MakeFlattenedSetsFromMap(
          {{"https://owner42.test",
            {"https://member1a.test", "https://member1b.test"}}}));
  const std::vector<FirstPartySetsLoader::SingleSet> additions{
      SingleSet(net::SchemefulSite(GURL("https://owner1.test")),
                {net::SchemefulSite(GURL("https://member1a.test")),
                 net::SchemefulSite(GURL("https://member1b.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner2.test")),
                {net::SchemefulSite(GURL("https://member2.test"))})};

  EXPECT_THAT(
      FirstPartySetsLoader::NormalizeAdditionSets(existing_sets, additions),
      UnorderedElementsAreArray(additions));
}

// There is no transitive overlap since the addition sets intersect with
// different existing sets, so normalization should be a noop.
TEST(FirstPartySetsLoaderTestNormalizeAdditionSets,
     NoTransitiveOverlap_SeparateOverlaps_AdditionSetsAreUnchanged) {
  const FirstPartySetsLoader::FlattenedSets existing_sets(
      MakeFlattenedSetsFromMap(
          {{"https://ownerA.test", {"https://member1.test"}},
           {"https://ownerB.test", {"https://member2.test"}}}));
  const std::vector<FirstPartySetsLoader::SingleSet> additions{
      SingleSet(net::SchemefulSite(GURL("https://owner1.test")),
                {net::SchemefulSite(GURL("https://member1.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner2.test")),
                {net::SchemefulSite(GURL("https://member2.test"))})};

  EXPECT_THAT(
      FirstPartySetsLoader::NormalizeAdditionSets(existing_sets, additions),
      UnorderedElementsAreArray(additions));
}

TEST(FirstPartySetsLoaderTestNormalizeAdditionSets,
     TransitiveOverlap_TwoCommonOwners) {
  const FirstPartySetsLoader::FlattenedSets existing_sets(
      MakeFlattenedSetsFromMap(
          {{"https://owner1.test", {"https://owner2.test"}}}));
  const std::vector<FirstPartySetsLoader::SingleSet> additions{
      SingleSet(net::SchemefulSite(GURL("https://owner0.test")),
                {net::SchemefulSite(GURL("https://member0.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner1.test")),
                {net::SchemefulSite(GURL("https://member1.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner2.test")),
                {net::SchemefulSite(GURL("https://member2.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner42.test")),
                {net::SchemefulSite(GURL("https://member42.test"))})};

  // {owner1, {member1}} and {owner2, {member2}} transitively overlap with the
  // existing set.
  // owner1 takes ownership of the normalized addition set since it was
  // provided first.
  // The other addition sets are unaffected.
  EXPECT_THAT(
      FirstPartySetsLoader::NormalizeAdditionSets(existing_sets, additions),
      UnorderedElementsAre(
          SingleSet(net::SchemefulSite(GURL("https://owner0.test")),
                    {net::SchemefulSite(GURL("https://member0.test"))}),
          SingleSet(net::SchemefulSite(GURL("https://owner1.test")),
                    {net::SchemefulSite(GURL("https://member1.test")),
                     net::SchemefulSite(GURL("https://owner2.test")),
                     net::SchemefulSite(GURL("https://member2.test"))}),
          SingleSet(net::SchemefulSite(GURL("https://owner42.test")),
                    {net::SchemefulSite(GURL("https://member42.test"))})));
}

TEST(FirstPartySetsLoaderTestNormalizeAdditionSets,
     TransitiveOverlap_TwoCommonMembers) {
  const FirstPartySetsLoader::FlattenedSets existing_sets(
      MakeFlattenedSetsFromMap(
          {{"https://owner2.test", {"https://owner1.test"}}}));
  const std::vector<FirstPartySetsLoader::SingleSet> additions{
      SingleSet(net::SchemefulSite(GURL("https://owner0.test")),
                {net::SchemefulSite(GURL("https://member0.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner2.test")),
                {net::SchemefulSite(GURL("https://member2.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner1.test")),
                {net::SchemefulSite(GURL("https://member1.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner42.test")),
                {net::SchemefulSite(GURL("https://member42.test"))})};

  // {owner1, {member1}} and {owner2, {member2}} transitively overlap with the
  // existing set.
  // owner2 takes ownership of the normalized addition set since it was
  // provided first.
  // The other addition sets are unaffected.
  EXPECT_THAT(
      FirstPartySetsLoader::NormalizeAdditionSets(existing_sets, additions),
      UnorderedElementsAre(
          SingleSet(net::SchemefulSite(GURL("https://owner0.test")),
                    {net::SchemefulSite(GURL("https://member0.test"))}),
          SingleSet(net::SchemefulSite(GURL("https://owner2.test")),
                    {net::SchemefulSite(GURL("https://member2.test")),
                     net::SchemefulSite(GURL("https://owner1.test")),
                     net::SchemefulSite(GURL("https://member1.test"))}),
          SingleSet(net::SchemefulSite(GURL("https://owner42.test")),
                    {net::SchemefulSite(GURL("https://member42.test"))})));
}

TEST(FirstPartySetsLoaderTestNormalizeAdditionSets,
     TransitiveOverlap_ThreeCommonOwners) {
  const FirstPartySetsLoader::FlattenedSets existing_sets(
      MakeFlattenedSetsFromMap({{"https://owner.test",
                                 {"https://owner1.test", "https://owner42.test",
                                  "https://owner2.test"}}}));
  const std::vector<FirstPartySetsLoader::SingleSet> additions{
      SingleSet(net::SchemefulSite(GURL("https://owner42.test")),
                {net::SchemefulSite(GURL("https://member42.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner0.test")),
                {net::SchemefulSite(GURL("https://member0.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner2.test")),
                {net::SchemefulSite(GURL("https://member2.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner1.test")),
                {net::SchemefulSite(GURL("https://member1.test"))})};

  // {owner1, {member1}}, {owner2, {member2}}, and {owner42, {member42}}
  // transitively overlap with the existing set.
  // owner42 takes ownership of the normalized addition set since it was
  // provided first.
  // The other addition sets are unaffected.
  EXPECT_THAT(
      FirstPartySetsLoader::NormalizeAdditionSets(existing_sets, additions),
      UnorderedElementsAre(
          SingleSet(net::SchemefulSite(GURL("https://owner42.test")),
                    {net::SchemefulSite(GURL("https://member42.test")),
                     net::SchemefulSite(GURL("https://owner1.test")),
                     net::SchemefulSite(GURL("https://member1.test")),
                     net::SchemefulSite(GURL("https://owner2.test")),
                     net::SchemefulSite(GURL("https://member2.test"))}),
          SingleSet(net::SchemefulSite(GURL("https://owner0.test")),
                    {net::SchemefulSite(GURL("https://member0.test"))})));
}

TEST(FirstPartySetsLoaderTestNormalizeAdditionSets,
     TransitiveOverlap_ThreeCommonMembers) {
  const FirstPartySetsLoader::FlattenedSets existing_sets(
      MakeFlattenedSetsFromMap(
          {{"https://owner.test",
            {"https://member1.test", "https://member42.test",
             "https://member2.test"}}}));
  const std::vector<FirstPartySetsLoader::SingleSet> additions{
      SingleSet(net::SchemefulSite(GURL("https://owner42.test")),
                {net::SchemefulSite(GURL("https://member42.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner0.test")),
                {net::SchemefulSite(GURL("https://member0.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner2.test")),
                {net::SchemefulSite(GURL("https://member2.test"))}),
      SingleSet(net::SchemefulSite(GURL("https://owner1.test")),
                {net::SchemefulSite(GURL("https://member1.test"))})};

  // {owner1, {member1}}, {owner2, {member2}}, and {owner42, {member42}}
  // transitively overlap with the existing set.
  // owner42 takes ownership of the normalized addition set since it was
  // provided first.
  // The other addition sets are unaffected.
  EXPECT_THAT(
      FirstPartySetsLoader::NormalizeAdditionSets(existing_sets, additions),
      UnorderedElementsAre(
          SingleSet(net::SchemefulSite(GURL("https://owner42.test")),
                    {net::SchemefulSite(GURL("https://member42.test")),
                     net::SchemefulSite(GURL("https://owner1.test")),
                     net::SchemefulSite(GURL("https://member1.test")),
                     net::SchemefulSite(GURL("https://owner2.test")),
                     net::SchemefulSite(GURL("https://member2.test"))}),
          SingleSet(net::SchemefulSite(GURL("https://owner0.test")),
                    {net::SchemefulSite(GURL("https://member0.test"))})));
}

}  // namespace content
