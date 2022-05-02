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

// Create a base::Value::Dict representation of a First-Party Set that has
// an owner field equal to |owner| and members field equal to |members|.
base::Value::Dict MakeFirstPartySetDict(
    const std::string& owner,
    const base::flat_set<std::string>& members) {
  base::Value::Dict dict;
  base::Value::List member_list;

  dict.Set("owner", owner);
  for (const std::string& member : members) {
    member_list.Append(member);
  }
  dict.Set("members", std::move(member_list));
  return dict;
}

// Converts a map of (owner->members) into a base::Value::List of First-Party
// Sets, each represented as a base::Value::Dict for ease of testing.
base::Value::List MakeFirstPartySetsList(
    const base::flat_map<std::string, std::vector<std::string>>&
        owners_to_members) {
  base::Value::List set_list;
  for (auto& [owner, members] : owners_to_members) {
    set_list.Append(MakeFirstPartySetDict(owner, members));
  }
  return set_list;
}

// Creates a base::Value::Dict representing a policy input JSON with a
// 'replacements' field equal to |replacements| and an 'additions' field equal
// to |additions|.
base::Value::Dict MakePolicySetInputFromMap(
    const base::flat_map<std::string, std::vector<std::string>>& replacements,
    const base::flat_map<std::string, std::vector<std::string>>& additions) {
  base::Value::Dict result;
  result.Set("replacements", base::Value(MakeFirstPartySetsList(replacements)));
  result.Set("additions", base::Value(MakeFirstPartySetsList(additions)));
  return result;
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
  FirstPartySetsLoaderTest() = default;

  base::flat_map<net::SchemefulSite, net::SchemefulSite> WaitAndGetResult() {
    return future_.Get();
  }

 protected:
  base::test::TaskEnvironment env_;
  base::test::TestFuture<base::flat_map<net::SchemefulSite, net::SchemefulSite>>
      future_;
};

class FirstPartySetsLoaderTestWithoutPolicySets
    : public FirstPartySetsLoaderTest {
 public:
  FirstPartySetsLoaderTestWithoutPolicySets()
      : loader_(future_.GetCallback(), base::Value::Dict()) {}

  FirstPartySetsLoader& loader() { return loader_; }

 private:
  FirstPartySetsLoader loader_;
};

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets, IgnoresInvalidFile) {
  loader().SetManuallySpecifiedSet("");
  const std::string input = "certainly not valid JSON";
  SetComponentSets(loader(), input);
  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets, ParsesComponent) {
  SetComponentSets(loader(), "");
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");
  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets, AcceptsMinimal) {
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

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets, AcceptsMultipleSets) {
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

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets, SetComponentSets_Idempotent) {
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

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets, OwnerIsOnlyMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://example.test"]}
{"owner": "https://foo.test", "members": ["https://member2.test"]})";

  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets, OwnerIsMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members":)"
      R"( ["https://example.test", "https://member1.test"]}
{"owner": "https://foo.test", "members": ["https://member2.test"]})";
  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets, RepeatedMember) {
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

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
       SetsManuallySpecified_Invalid_TooSmall) {
  loader().SetManuallySpecifiedSet("https://example.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
       SetsManuallySpecified_Invalid_NotOrigins) {
  loader().SetManuallySpecifiedSet("https://example.test,member1");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
       SetsManuallySpecified_Invalid_NotHTTPS) {
  loader().SetManuallySpecifiedSet("https://example.test,http://member1.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
       SetsManuallySpecified_Invalid_RegisteredDomain_Owner) {
  loader().SetManuallySpecifiedSet(
      "https://www.example.test..,https://www.member.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
       SetsManuallySpecified_Invalid_RegisteredDomain_Member) {
  loader().SetManuallySpecifiedSet(
      "https://www.example.test,https://www.member.test..");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
       SetsManuallySpecified_Valid_SingleMember) {
  loader().SetManuallySpecifiedSet("https://example.test,https://member.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
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

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
       SetsManuallySpecified_Valid_MultipleMembers) {
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

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
       SetsManuallySpecified_Valid_OwnerIsOnlyMember) {
  loader().SetManuallySpecifiedSet("https://example.test,https://example.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
       SetsManuallySpecified_Valid_OwnerIsMember) {
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

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
       SetsManuallySpecified_Valid_RepeatedMember) {
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

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
       SetsManuallySpecified_DeduplicatesOwnerOwner) {
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

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
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

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
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

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
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

TEST_F(FirstPartySetsLoaderTestWithoutPolicySets,
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

// These tests verify that the policy sets override the public sets and that the
// policy sets override the manually-specified set.
class FirstPartySetsLoaderTestWithPolicySets
    : public FirstPartySetsLoaderTest,
      public testing::WithParamInterface<FirstPartySetsSource> {
 public:
  FirstPartySetsLoaderTestWithPolicySets() = default;

 protected:
  // This method sets either the public sets or the manually specified
  // set with a {owner: `owner`, members: `members`} First-Party Set.
  //
  // This is used to test how the policy sets override either the public or
  // manually specified set.
  void SetEitherPublicOrManuallySpecifiedSet(
      FirstPartySetsLoader& loader,
      const std::string& owner,
      const std::vector<std::string>& members) {
    switch (GetParam()) {
      case FirstPartySetsSource::kPublicSets: {
        // Create the JSON representation.
        base::Value::Dict public_set;
        public_set.Set("owner", base::Value(owner));
        base::Value::List member_list;
        for (const std::string& member : members) {
          member_list.Append(member);
        }
        public_set.Set("members", base::Value(std::move(member_list)));
        std::string component_input;
        JSONStringValueSerializer serializer(&component_input);
        serializer.Serialize(public_set);
        SetComponentSets(loader, component_input);
        loader.SetManuallySpecifiedSet("");
        return;
      }
      case FirstPartySetsSource::kCommandLineSet:
        SetComponentSets(loader, "");
        loader.SetManuallySpecifiedSet(base::StringPrintf(
            "%s,%s", owner.c_str(), base::JoinString(members, ",").c_str()));
        return;
    }
  }

  base::OnceCallback<
      void(base::flat_map<net::SchemefulSite, net::SchemefulSite>)>
  callback() {
    return future_.GetCallback();
  }
};

TEST_P(FirstPartySetsLoaderTestWithPolicySets, EmptyPolicySetLists) {
  FirstPartySetsLoader loader(
      callback(),
      MakePolicySetInputFromMap(/*replacements=*/{}, /*additions=*/{}));
  SetEitherPublicOrManuallySpecifiedSet(loader, "https://owner1.test",
                                        {"https://member1.test"});
  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://owner1.test")),
                                   // Below are the owner self mappings.
                                   Pair(SerializesTo("https://owner1.test"),
                                        SerializesTo("https://owner1.test"))));
}

TEST_P(FirstPartySetsLoaderTestWithPolicySets,
       Replacements__NoIntersection_NoRemoval) {
  FirstPartySetsLoader loader(
      callback(),
      MakePolicySetInputFromMap(
          /*replacements=*/{{"https://owner2.test", {"https://member2.test"}}},
          /*additions=*/{}));
  SetEitherPublicOrManuallySpecifiedSet(loader, "https://owner1.test",
                                        {"https://member1.test"});

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://owner1.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://owner2.test")),
                                   // Below are the owner self mappings.
                                   Pair(SerializesTo("https://owner1.test"),
                                        SerializesTo("https://owner1.test")),
                                   Pair(SerializesTo("https://owner2.test"),
                                        SerializesTo("https://owner2.test"))));
}

// The common member between the policy and existing set is removed from its
// previous set.
TEST_P(FirstPartySetsLoaderTestWithPolicySets,
       Replacements_ReplacesExistingMember_RemovedFromFormerSet) {
  FirstPartySetsLoader loader(
      callback(),
      MakePolicySetInputFromMap(
          /*replacements=*/{{"https://owner2.test", {"https://member1b.test"}}},
          /*additions=*/{}));
  SetEitherPublicOrManuallySpecifiedSet(
      loader, "https://owner1.test",
      {"https://member1a.test", "https://member1b.test"});

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://member1a.test"),
                                        SerializesTo("https://owner1.test")),
                                   Pair(SerializesTo("https://member1b.test"),
                                        SerializesTo("https://owner2.test")),
                                   // Below are the owner self mappings.
                                   Pair(SerializesTo("https://owner1.test"),
                                        SerializesTo("https://owner1.test")),
                                   Pair(SerializesTo("https://owner2.test"),
                                        SerializesTo("https://owner2.test"))));
}

// The common owner between the policy and existing set is removed and its
// former members are removed since they are now unowned.
TEST_P(FirstPartySetsLoaderTestWithPolicySets,
       Replacements_ReplacesExistingOwner_RemovesFormerMembers) {
  FirstPartySetsLoader loader(
      callback(),
      MakePolicySetInputFromMap(
          /*replacements=*/{{"https://owner1.test", {"https://member2.test"}}},
          /*additions=*/{}));
  SetEitherPublicOrManuallySpecifiedSet(
      loader, "https://owner1.test",
      {"https://member1a.test", "https://member1b.test"});

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://owner1.test")),
                                   // Below are the owner self mappings.
                                   Pair(SerializesTo("https://owner1.test"),
                                        SerializesTo("https://owner1.test"))));
}

// The common member between the policy and existing set is removed and any
// leftover singletons are deleted.
TEST_P(FirstPartySetsLoaderTestWithPolicySets,
       Replacements_ReplacesExistingMember_RemovesSingletons) {
  FirstPartySetsLoader loader(
      callback(),
      MakePolicySetInputFromMap(
          /*replacements=*/{{"https://owner3.test", {"https://member1.test"}}},
          /*additions=*/{}));
  SetEitherPublicOrManuallySpecifiedSet(loader, "https://owner1.test",
                                        {"https://member1.test"});

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://owner3.test")),
                                   // Below are the owner self mappings.
                                   Pair(SerializesTo("https://owner3.test"),
                                        SerializesTo("https://owner3.test"))));
}

// The policy set and the existing set have nothing in common so the policy set
// gets added in without updating the existing set.
TEST_P(FirstPartySetsLoaderTestWithPolicySets,
       Additions_NoIntersection_AddsWithoutUpdating) {
  FirstPartySetsLoader loader(
      callback(), MakePolicySetInputFromMap(
                      /*replacements=*/{}, /*additions=*/{
                          {"https://owner2.test", {"https://member2.test"}}}));
  SetEitherPublicOrManuallySpecifiedSet(loader, "https://owner1.test",
                                        {"https://member1.test"});

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://owner1.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://owner2.test")),
                                   // Below are the owner self mappings.
                                   Pair(SerializesTo("https://owner1.test"),
                                        SerializesTo("https://owner1.test")),
                                   Pair(SerializesTo("https://owner2.test"),
                                        SerializesTo("https://owner2.test"))));
}

// The owner of a policy set is also a member in an existing set.
// The policy set absorbs all sites in the existing set into its members.
TEST_P(FirstPartySetsLoaderTestWithPolicySets,
       Additions_PolicyOwnerIsExistingMember_PolicySetAbsorbsExistingSet) {
  FirstPartySetsLoader loader(
      callback(),
      MakePolicySetInputFromMap(
          /*replacements=*/{}, /*additions=*/{
              {"https://member2.test",
               {"https://member2a.test", "https://member2b.test"}}}));
  SetEitherPublicOrManuallySpecifiedSet(loader, "https://owner1.test",
                                        {"https://member2.test"});

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://owner1.test"),
                                        SerializesTo("https://member2.test")),
                                   Pair(SerializesTo("https://member2b.test"),
                                        SerializesTo("https://member2.test")),
                                   Pair(SerializesTo("https://member2a.test"),
                                        SerializesTo("https://member2.test")),
                                   // Below are the owner self mappings.
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://member2.test"))));
}

// The owner of a policy set is also an owner of an existing set.
// The policy set absorbs all of its owner's existing members into its members.
TEST_P(FirstPartySetsLoaderTestWithPolicySets,
       Additions_PolicyOwnerIsExistingOwner_PolicySetAbsorbsExistingMembers) {
  FirstPartySetsLoader loader(
      callback(), MakePolicySetInputFromMap(
                      /*replacements=*/{}, /*additions=*/{
                          {"https://owner1.test", {"https://member2.test"}}}));
  SetEitherPublicOrManuallySpecifiedSet(
      loader, "https://owner1.test",
      {"https://member1.test", "https://member3.test"});

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://member3.test"),
                                        SerializesTo("https://owner1.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://owner1.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://owner1.test")),
                                   // Below are the owner self mappings.
                                   Pair(SerializesTo("https://owner1.test"),
                                        SerializesTo("https://owner1.test"))));
}

// Replacement & Addition policy set lists are required to be disjoint with
// each other.
TEST_P(FirstPartySetsLoaderTestWithPolicySets,
       BothTypes_SetListsIntersect_NoOverridesApplied) {
  FirstPartySetsLoader loader(
      callback(), MakePolicySetInputFromMap(
                      /*replacements=*/{{"https://owner0.test",
                                         {"https://new-member.test"}}},
                      /*additions=*/{{"https://owner42.test",
                                      {"https://new-member.test"}}}));
  SetEitherPublicOrManuallySpecifiedSet(
      loader, "https://owner1.test",
      {"https://member1.test", "https://member2.test"});

  // The policy set overrides are ignored since the set lists were not disjoint,
  // which violates a requirement of the policy.
  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://owner1.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://owner1.test")),
                                   // Below are the owner self mappings.
                                   Pair(SerializesTo("https://owner1.test"),
                                        SerializesTo("https://owner1.test"))));
}

// Replacement & Addition policy set lists are disjoint with each other.
TEST_P(FirstPartySetsLoaderTestWithPolicySets,
       BothTypes_SetListsDontIntersect_OverridesApplied) {
  FirstPartySetsLoader loader(
      callback(),
      MakePolicySetInputFromMap(
          /*replacements=*/{{"https://owner0.test", {"https://member1.test"}}},
          /*additions=*/{
              {"https://owner1.test", {"https://new-member1.test"}}}));
  SetEitherPublicOrManuallySpecifiedSet(
      loader, "https://owner1.test",
      {"https://member1.test", "https://member2.test"});

  // While the policy set lists are disjoint, they are able to affect the same
  // existing set.
  EXPECT_THAT(
      WaitAndGetResult(),
      UnorderedElementsAre(Pair(SerializesTo("https://member2.test"),
                                SerializesTo("https://owner1.test")),
                           Pair(SerializesTo("https://new-member1.test"),
                                SerializesTo("https://owner1.test")),
                           Pair(SerializesTo("https://member1.test"),
                                SerializesTo("https://owner0.test")),
                           // Below are the owner self mappings.
                           Pair(SerializesTo("https://owner0.test"),
                                SerializesTo("https://owner0.test")),
                           Pair(SerializesTo("https://owner1.test"),
                                SerializesTo("https://owner1.test"))));
}

INSTANTIATE_TEST_CASE_P(
    /* no label */,
    FirstPartySetsLoaderTestWithPolicySets,
    ::testing::Values(FirstPartySetsSource::kPublicSets,
                      FirstPartySetsSource::kCommandLineSet));

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
