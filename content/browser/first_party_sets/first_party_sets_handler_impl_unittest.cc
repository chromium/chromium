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
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

// Some of these tests overlap with FirstPartySetParser unittests, but
// overlapping test coverage isn't the worst thing.
namespace content {
namespace {
using PolicyCustomization = FirstPartySetsHandlerImpl::PolicyCustomization;

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

FirstPartySetsHandlerImpl::FlattenedSets MakeFlattenedSetsFromMap(
    const base::flat_map<std::string, std::vector<std::string>>&
        owners_to_members) {
  FirstPartySetsHandlerImpl::FlattenedSets result;
  for (const auto& [owner, members] : owners_to_members) {
    net::SchemefulSite owner_site((GURL(owner)));
    result.insert(std::make_pair(owner_site, owner_site));
    for (const std::string& member : members) {
      net::SchemefulSite member_site((GURL(member)));
      result.insert(std::make_pair(member_site, owner_site));
    }
  }
  return result;
}

// Creates a ParsedPolicySetLists with the replacements and additions fields
// constructed from `replacements` and `additions`.
FirstPartySetParser::ParsedPolicySetLists MakeParsedPolicyFromMap(
    const base::flat_map<std::string, std::vector<std::string>>& replacements,
    const base::flat_map<std::string, std::vector<std::string>>& additions) {
  FirstPartySetParser::ParsedPolicySetLists result;
  for (auto& [owner, members] : replacements) {
    std::vector<net::SchemefulSite> member_sites;
    for (const std::string& member : members) {
      member_sites.emplace_back(GURL(member));
    }
    result.replacements.emplace_back(net::SchemefulSite(GURL(owner)),
                                     member_sites);
  }
  for (auto& [owner, members] : additions) {
    std::vector<net::SchemefulSite> member_sites;
    for (const std::string& member : members) {
      member_sites.emplace_back(GURL(member));
    }
    result.additions.emplace_back(net::SchemefulSite(GURL(owner)),
                                  member_sites);
  }
  return result;
}

FirstPartySetsHandlerImpl::FlattenedSets ParseSetsFromStream(
    const std::string& sets) {
  std::istringstream stream(sets);
  return FirstPartySetParser::ParseSetsFromStream(stream);
}

FirstPartySetsHandlerImpl::FlattenedSets GetSetsAndWait() {
  base::test::TestFuture<FirstPartySetsHandlerImpl::FlattenedSets> future;
  absl::optional<FirstPartySetsHandlerImpl::FlattenedSets> result =
      FirstPartySetsHandlerImpl::GetInstance()->GetSets(future.GetCallback());
  return result.has_value() ? result.value() : future.Get();
}
}  // namespace

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_SitesJoined) {
  FirstPartySetsHandlerImpl::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::SchemefulSite(GURL("https://example.test"))}};
  // Consistency check the reviewer-friendly format matches the input.
  ASSERT_THAT(ParseSetsFromStream(
                  R"({"owner": "https://example.test", "members": )"
                  R"(["https://member1.test", "https://member3.test"]})"),
              old_sets);

  FirstPartySetsHandlerImpl::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::SchemefulSite(GURL("https://foo.test"))},
      {net::SchemefulSite(GURL("https://member2.test")),
       net::SchemefulSite(GURL("https://foo.test"))},
  };
  // Consistency check the reviewer-friendly format matches the input.
  ASSERT_THAT(
      ParseSetsFromStream(
          R"({"owner": "https://example.test", )"
          R"("members": ["https://member1.test", "https://member3.test"]}
      {"owner": "https://foo.test", "members": ["https://member2.test"]})"),
      current_sets);

  // "https://foo.test" and "https://member2.test" joined FPSs. We don't clear
  // site data upon joining, so the computed diff should be empty set.
  EXPECT_THAT(
      FirstPartySetsHandlerImpl::ComputeSetsDiff(old_sets, current_sets),
      IsEmpty());
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_SitesLeft) {
  FirstPartySetsHandlerImpl::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::SchemefulSite(GURL("https://foo.test"))},
      {net::SchemefulSite(GURL("https://member2.test")),
       net::SchemefulSite(GURL("https://foo.test"))}};
  // Consistency check the reviewer-friendly format matches the input.
  ASSERT_THAT(
      ParseSetsFromStream(R"({"owner": "https://example.test", "members": )"
                          R"(["https://member1.test", "https://member3.test"]}
      { "owner": "https://foo.test", "members": ["https://member2.test"]})"),
      old_sets);

  FirstPartySetsHandlerImpl::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::SchemefulSite(GURL("https://example.test"))}};
  // Consistency check the reviewer-friendly format matches the input.
  ASSERT_THAT(ParseSetsFromStream(R"({"owner": "https://example.test", )"
                                  R"("members": ["https://member1.test"]})"),
              current_sets);

  // Expected diff: "https://foo.test", "https://member2.test" and
  // "https://member3.test" left FPSs.
  EXPECT_THAT(
      FirstPartySetsHandlerImpl::ComputeSetsDiff(old_sets, current_sets),
      UnorderedElementsAre(SerializesTo("https://foo.test"),
                           SerializesTo("https://member2.test"),
                           SerializesTo("https://member3.test")));
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_OwnerChanged) {
  FirstPartySetsHandlerImpl::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::SchemefulSite(GURL("https://foo.test"))},
      {net::SchemefulSite(GURL("https://member2.test")),
       net::SchemefulSite(GURL("https://foo.test"))},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::SchemefulSite(GURL("https://foo.test"))}};
  // Consistency check the reviewer-friendly format matches the input.
  ASSERT_THAT(ParseSetsFromStream(
                  R"({"owner": "https://example.test", "members": )"
                  R"(["https://member1.test"]}
      {"owner": "https://foo.test", "members": )"
                  R"(["https://member2.test", "https://member3.test"]})"),
              old_sets);

  FirstPartySetsHandlerImpl::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member3.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::SchemefulSite(GURL("https://foo.test"))},
      {net::SchemefulSite(GURL("https://member2.test")),
       net::SchemefulSite(GURL("https://foo.test"))}};
  // Consistency check the reviewer-friendly format matches the input.
  ASSERT_THAT(
      ParseSetsFromStream(R"({"owner": "https://example.test", "members": )"
                          R"(["https://member1.test", "https://member3.test"]}
      {"owner": "https://foo.test", "members": ["https://member2.test"]})"),
      current_sets);

  // Expected diff: "https://member3.test" changed owner.
  EXPECT_THAT(
      FirstPartySetsHandlerImpl::ComputeSetsDiff(old_sets, current_sets),
      UnorderedElementsAre(SerializesTo("https://member3.test")));
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_OwnerLeft) {
  FirstPartySetsHandlerImpl::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://bar.test")),
       net::SchemefulSite(GURL("https://example.test"))}};
  // Consistency check the reviewer-friendly format matches the input.
  ASSERT_THAT(
      ParseSetsFromStream(R"({"owner": "https://example.test", "members": )"
                          R"(["https://foo.test", "https://bar.test"]})"),
      old_sets);

  FirstPartySetsHandlerImpl::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://foo.test")),
       net::SchemefulSite(GURL("https://foo.test"))},
      {net::SchemefulSite(GURL("https://bar.test")),
       net::SchemefulSite(GURL("https://foo.test"))}};
  // Consistency check the reviewer-friendly format matches the input.
  ASSERT_THAT(ParseSetsFromStream(R"(
      {"owner": "https://foo.test", "members": ["https://bar.test"]})"),
              current_sets);

  // Expected diff: "https://example.test" left FPSs, "https://foo.test" and
  // "https://bar.test" changed owner.
  // It would be valid to only have example.test in the diff, but our logic
  // isn't sophisticated enough yet to know that foo.test and bar.test don't
  // need to be included in the result.
  EXPECT_THAT(
      FirstPartySetsHandlerImpl::ComputeSetsDiff(old_sets, current_sets),
      UnorderedElementsAre(SerializesTo("https://example.test"),
                           SerializesTo("https://foo.test"),
                           SerializesTo("https://bar.test")));
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_OwnerMemberRotate) {
  FirstPartySetsHandlerImpl::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::SchemefulSite(GURL("https://example.test"))}};
  // Consistency check the reviewer-friendly format matches the input.
  ASSERT_THAT(
      ParseSetsFromStream(R"({"owner": "https://example.test", "members": )"
                          R"(["https://foo.test"]})"),
      old_sets);

  FirstPartySetsHandlerImpl::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://foo.test"))},
      {net::SchemefulSite(GURL("https://foo.test")),
       net::SchemefulSite(GURL("https://foo.test"))}};
  // Consistency check the reviewer-friendly format matches the input.
  ASSERT_THAT(
      ParseSetsFromStream(
          R"({"owner": "https://foo.test", "members": ["https://example.test"]})"),
      current_sets);

  // Expected diff: "https://example.test" and "https://foo.test" changed owner.
  // It would be valid to not include example.test and foo.test in the result,
  // but our logic isn't sophisticated enough yet to know that.ß
  EXPECT_THAT(
      FirstPartySetsHandlerImpl::ComputeSetsDiff(old_sets, current_sets),
      UnorderedElementsAre(SerializesTo("https://example.test"),
                           SerializesTo("https://foo.test")));
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_EmptyOldSets) {
  // Empty old_sets.
  FirstPartySetsHandlerImpl::FlattenedSets current_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::SchemefulSite(GURL("https://example.test"))}};
  // Consistency check the reviewer-friendly format matches the input.
  ASSERT_THAT(ParseSetsFromStream(R"({"owner": "https://example.test", )"
                                  R"("members": ["https://member1.test"]})"),
              current_sets);

  EXPECT_THAT(FirstPartySetsHandlerImpl::ComputeSetsDiff({}, current_sets),
              IsEmpty());
}

TEST(FirstPartySetsHandlerImpl, ComputeSetsDiff_EmptyCurrentSets) {
  // Empty current sets.
  FirstPartySetsHandlerImpl::FlattenedSets old_sets = {
      {net::SchemefulSite(GURL("https://example.test")),
       net::SchemefulSite(GURL("https://example.test"))},
      {net::SchemefulSite(GURL("https://member1.test")),
       net::SchemefulSite(GURL("https://example.test"))}};
  // Consistency check the reviewer-friendly format matches the input.
  ASSERT_THAT(ParseSetsFromStream(R"({"owner": "https://example.test", )"
                                  R"("members": ["https://member1.test"]})"),
              old_sets);

  EXPECT_THAT(FirstPartySetsHandlerImpl::ComputeSetsDiff(old_sets, {}),
              UnorderedElementsAre(SerializesTo("https://example.test"),
                                   SerializesTo("https://member1.test")));
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
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test"))));
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

  auto expected_sets = UnorderedElementsAre(
      Pair(SerializesTo("https://example.test"),
           SerializesTo("https://example.test")),
      Pair(SerializesTo("https://member1.test"),
           SerializesTo("https://example.test")),
      Pair(SerializesTo("https://foo.test"), SerializesTo("https://foo.test")),
      Pair(SerializesTo("https://member2.test"),
           SerializesTo("https://foo.test")));

  // Persisted sets are expected to be loaded with the provided path.
  FirstPartySetsHandlerImpl::GetInstance()->Init(
      scoped_dir_.GetPath(),
      /*flag_value=*/"https://example.test,https://member1.test");
  EXPECT_THAT(GetSetsAndWait(), expected_sets);

  env().RunUntilIdle();

  std::string got;
  ASSERT_TRUE(base::ReadFileToString(persisted_sets_path_, &got));
  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(got),
              expected_sets);
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

  auto expected_sets = UnorderedElementsAre(
      Pair(SerializesTo("https://example.test"),
           SerializesTo("https://example.test")),
      Pair(SerializesTo("https://member1.test"),
           SerializesTo("https://example.test")),
      Pair(SerializesTo("https://foo.test"), SerializesTo("https://foo.test")),
      Pair(SerializesTo("https://member2.test"),
           SerializesTo("https://foo.test")));

  // Persisted sets are expected to be loaded with the provided path.
  FirstPartySetsHandlerImpl::GetInstance()->Init(
      scoped_dir_.GetPath(),
      /*flag_value=*/"https://example.test,https://member1.test");
  EXPECT_THAT(GetSetsAndWait(), expected_sets);

  env().RunUntilIdle();

  std::string got;
  ASSERT_TRUE(base::ReadFileToString(persisted_sets_path_, &got));
  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(got),
              expected_sets);
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

  auto expected_sets =
      UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                SerializesTo("https://example.test")),
                           Pair(SerializesTo("https://member.test"),
                                SerializesTo("https://example.test")));

  // Persisted sets are expected to be loaded with the provided path.
  FirstPartySetsHandlerImpl::GetInstance()->Init(scoped_dir_.GetPath(),
                                                 /*flag_value=*/"");
  EXPECT_THAT(GetSetsAndWait(), expected_sets);

  env().RunUntilIdle();

  std::string got;
  ASSERT_TRUE(base::ReadFileToString(persisted_sets_path_, &got));
  EXPECT_THAT(FirstPartySetParser::DeserializeFirstPartySets(got),
              expected_sets);

  EXPECT_THAT(
      FirstPartySetsHandlerImpl::GetInstance()->GetSets(
          base::BindLambdaForTesting(
              [](FirstPartySetsHandlerImpl::FlattenedSets) { FAIL(); })),
      testing::Optional(expected_sets));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       GetSetsIfEnabledAndReady_BeforeSetsReady) {
  FirstPartySetsHandlerImpl::GetInstance()
      ->SetEmbedderWillProvidePublicSetsForTesting(true);
  ASSERT_TRUE(base::WriteFile(persisted_sets_path_, "{}"));

  // Call GetSets before the sets are ready, and before Init has been called.
  base::test::TestFuture<FirstPartySetsHandlerImpl::FlattenedSets> future;
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
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member.test"),
                                        SerializesTo("https://example.test"))));

  EXPECT_THAT(
      FirstPartySetsHandlerImpl::GetInstance()->GetSets(
          base::BindLambdaForTesting(
              [](FirstPartySetsHandlerImpl::FlattenedSets) { FAIL(); })),
      testing::Optional(
          UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                    SerializesTo("https://example.test")),
                               Pair(SerializesTo("https://member.test"),
                                    SerializesTo("https://example.test")))));
}

TEST(FirstPartySetsProfilePolicyCustomizations, EmptyPolicySetLists) {
  EXPECT_THAT(FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
                  MakeFlattenedSetsFromMap(
                      {{"https://owner1.test", {"https://member1.test"}}}),
                  MakeParsedPolicyFromMap({}, {})),
              FirstPartySetsHandlerImpl::PolicyCustomization());
}

TEST(FirstPartySetsProfilePolicyCustomizations,
     Replacements__NoIntersection_NoRemoval) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakeFlattenedSetsFromMap(
              {{"https://owner1.test", {"https://member1.test"}}}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://owner2.test",
                                 {"https://member2.test"}}},
              /*additions=*/{}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member2.test"),
                       Optional(SerializesTo("https://owner2.test"))),
                  Pair(SerializesTo("https://owner2.test"),
                       Optional(SerializesTo("https://owner2.test")))));
}

// The common member between the policy and existing set is removed from its
// previous set.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Replacements_ReplacesExistingMember_RemovedFromFormerSet) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakeFlattenedSetsFromMap(
              {{"https://owner1.test",
                {"https://member1a.test", "https://member1b.test"}}}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://owner2.test",
                                 {"https://member1b.test"}}},
              /*additions=*/{}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1b.test"),
                       Optional(SerializesTo("https://owner2.test"))),
                  Pair(SerializesTo("https://owner2.test"),
                       Optional(SerializesTo("https://owner2.test")))));
}

// The common owner between the policy and existing set is removed and its
// former members are removed since they are now unowned.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Replacements_ReplacesExistingOwner_RemovesFormerMembers) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakeFlattenedSetsFromMap(
              {{"https://owner1.test",
                {"https://member1a.test", "https://member1b.test"}}}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://owner1.test",
                                 {"https://member2.test"}}},
              /*additions=*/{}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member2.test"),
                       Optional(SerializesTo("https://owner1.test"))),
                  Pair(SerializesTo("https://owner1.test"),
                       Optional(SerializesTo("https://owner1.test"))),
                  Pair(SerializesTo("https://member1a.test"), absl::nullopt),
                  Pair(SerializesTo("https://member1b.test"), absl::nullopt)));
}

// The common member between the policy and existing set is removed and any
// leftover singletons are deleted.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Replacements_ReplacesExistingMember_RemovesSingletons) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakeFlattenedSetsFromMap(
              {{"https://owner1.test", {"https://member1.test"}}}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://owner3.test",
                                 {"https://member1.test"}}},
              /*additions=*/{}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       Optional(SerializesTo("https://owner3.test"))),
                  Pair(SerializesTo("https://owner3.test"),
                       Optional(SerializesTo("https://owner3.test"))),
                  Pair(SerializesTo("https://owner1.test"), absl::nullopt)));
}

// The policy set and the existing set have nothing in common so the policy set
// gets added in without updating the existing set.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Additions_NoIntersection_AddsWithoutUpdating) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakeFlattenedSetsFromMap(
              {{"https://owner1.test", {"https://member1.test"}}}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{},
              /*additions=*/{
                  {"https://owner2.test", {"https://member2.test"}}}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member2.test"),
                       Optional(SerializesTo("https://owner2.test"))),
                  Pair(SerializesTo("https://owner2.test"),
                       Optional(SerializesTo("https://owner2.test")))));
}

// The owner of a policy set is also a member in an existing set.
// The policy set absorbs all sites in the existing set into its members.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Additions_PolicyOwnerIsExistingMember_PolicySetAbsorbsExistingSet) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakeFlattenedSetsFromMap(
              {{"https://owner1.test", {"https://member2.test"}}}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{},
              /*additions=*/{
                  {"https://member2.test",
                   {"https://member2a.test", "https://member2b.test"}}}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://owner1.test"),
                       Optional(SerializesTo("https://member2.test"))),
                  Pair(SerializesTo("https://member2a.test"),
                       Optional(SerializesTo("https://member2.test"))),
                  Pair(SerializesTo("https://member2b.test"),
                       Optional(SerializesTo("https://member2.test"))),
                  Pair(SerializesTo("https://member2.test"),
                       Optional(SerializesTo("https://member2.test")))));
}

// The owner of a policy set is also an owner of an existing set.
// The policy set absorbs all of its owner's existing members into its members.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Additions_PolicyOwnerIsExistingOwner_PolicySetAbsorbsExistingMembers) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakeFlattenedSetsFromMap(
              {{"https://owner1.test",
                {"https://member1.test", "https://member3.test"}}}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{},
              /*additions=*/{
                  {"https://owner1.test", {"https://member2.test"}}}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member2.test"),
                       Optional(SerializesTo("https://owner1.test"))),
                  Pair(SerializesTo("https://member1.test"),
                       Optional(SerializesTo("https://owner1.test"))),
                  Pair(SerializesTo("https://member3.test"),
                       Optional(SerializesTo("https://owner1.test"))),
                  Pair(SerializesTo("https://owner1.test"),
                       Optional(SerializesTo("https://owner1.test")))));
}

// Existing set overlaps with both replacement and addition set.
TEST(FirstPartySetsProfilePolicyCustomizations,
     ReplacementsAndAdditions_SetListsOverlapWithSameExistingSet) {
  PolicyCustomization customization =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          MakeFlattenedSetsFromMap(
              {{"https://owner1.test",
                {"https://member1.test", "https://member2.test"}}}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://owner0.test",
                                 {"https://member1.test"}}},
              /*additions=*/{
                  {"https://owner1.test", {"https://new-member1.test"}}}));
  EXPECT_THAT(customization,
              UnorderedElementsAre(
                  Pair(SerializesTo("https://member1.test"),
                       Optional(SerializesTo("https://owner0.test"))),
                  Pair(SerializesTo("https://owner0.test"),
                       Optional(SerializesTo("https://owner0.test"))),
                  Pair(SerializesTo("https://new-member1.test"),
                       Optional(SerializesTo("https://owner1.test"))),
                  Pair(SerializesTo("https://member2.test"),
                       Optional(SerializesTo("https://owner1.test"))),
                  Pair(SerializesTo("https://owner1.test"),
                       Optional(SerializesTo("https://owner1.test")))));
}
}  // namespace content
