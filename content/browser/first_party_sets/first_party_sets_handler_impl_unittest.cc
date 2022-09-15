// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/local_set_declaration.h"
#include "content/public/browser/first_party_sets_handler.h"
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
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

// Some of these tests overlap with FirstPartySetParser unittests, but
// overlapping test coverage isn't the worst thing.
namespace content {

namespace {

using FlattenedSets = FirstPartySetsHandlerImpl::FlattenedSets;
using SingleSet = FirstPartySetParser::SingleSet;
using ParseErrorType = FirstPartySetsHandler::ParseErrorType;
using ParseWarningType = FirstPartySetsHandler::ParseWarningType;

const char* kAdditionsField = "additions";
const char* kPrimaryField = "primary";
const char* kCctldsField = "ccTLDs";

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

BrowserContext* FakeBrowserContextGetter() {
  return nullptr;
}

FirstPartySetsHandlerImpl::FlattenedSets MakeFlattenedSetsFromMap(
    const base::flat_map<std::string, std::vector<std::string>>&
        primaries_to_associated_sites) {
  FirstPartySetsHandlerImpl::FlattenedSets result;
  for (const auto& [primary, associated_sites] :
       primaries_to_associated_sites) {
    net::SchemefulSite primary_site((GURL(primary)));
    result.insert(std::make_pair(
        primary_site,
        net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary,
                                absl::nullopt)));
    uint32_t index = 0;
    for (const std::string& associated_site : associated_sites) {
      result.insert(
          std::make_pair(net::SchemefulSite(GURL(associated_site)),
                         net::FirstPartySetEntry(
                             primary_site, net::SiteType::kAssociated, index)));
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
  for (auto& [primary, associated_sites] : input) {
    std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>> sites;
    net::SchemefulSite primary_site((GURL(primary)));
    sites.emplace_back(primary_site, net::FirstPartySetEntry(
                                         primary_site, net::SiteType::kPrimary,
                                         absl::nullopt));
    for (const std::string& associated_site : associated_sites) {
      sites.emplace_back(
          GURL(associated_site),
          net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
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

net::PublicSets GetSetsAndWait() {
  base::test::TestFuture<net::PublicSets> future;
  absl::optional<net::PublicSets> result =
      FirstPartySetsHandlerImpl::GetInstance()->GetSets(future.GetCallback());
  return result.has_value() ? std::move(result).value() : future.Take();
}

// TODO(shuuran): Return `net::PublicSets` type instead.
absl::optional<FirstPartySetsHandlerImpl::FlattenedSets>
GetPersistedPublicSetsAndWait() {
  base::test::TestFuture<
      absl::optional<FirstPartySetsHandlerImpl::FlattenedSets>>
      future;
  FirstPartySetsHandlerImpl::GetInstance()->GetPersistedPublicSetsForTesting(
      future.GetCallback());
  return future.Get();
}

}  // namespace

TEST(FirstPartySetsHandlerImpl, ValidateEnterprisePolicy_ValidPolicy) {
  base::Value input = base::JSONReader::Read(R"(
             {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite2.test"]
                  }
                ]
              }
            )")
                          .value();
  // Validation doesn't fail with an error and there are no warnings to output.
  std::pair<absl::optional<FirstPartySetsHandler::ParseError>,
            std::vector<FirstPartySetsHandler::ParseWarning>>
      opt_error_and_warnings =
          FirstPartySetsHandler::ValidateEnterprisePolicy(input.GetDict());
  EXPECT_FALSE(opt_error_and_warnings.first.has_value());
  EXPECT_THAT(opt_error_and_warnings.second, IsEmpty());
}

TEST(FirstPartySetsHandlerImpl,
     ValidateEnterprisePolicy_ValidPolicyWithWarnings) {
  // Some input that matches our policies schema but returns non-fatal warnings.
  base::Value input = base::JSONReader::Read(R"(
              {
                "replacements": [],
                "additions": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"],
                    "ccTLDs": {
                      "https://non-canonical.test": ["https://primary1.test"]
                    }
                  }
                ]
              }
            )")
                          .value();
  // Validation succeeds without errors.
  std::pair<absl::optional<FirstPartySetsHandler::ParseError>,
            std::vector<FirstPartySetsHandler::ParseWarning>>
      opt_error_and_warnings =
          FirstPartySetsHandler::ValidateEnterprisePolicy(input.GetDict());
  EXPECT_FALSE(opt_error_and_warnings.first.has_value());
  // Outputs metadata that can be used to surface a descriptive warning.
  EXPECT_EQ(opt_error_and_warnings.second,
            std::vector<FirstPartySetsHandler::ParseWarning>{
                FirstPartySetsHandler::ParseWarning(
                    ParseWarningType::kCctldKeyNotCanonical,
                    {kAdditionsField, 0, kCctldsField,
                     "https://non-canonical.test"})});
}

TEST(FirstPartySetsHandlerImpl, ValidateEnterprisePolicy_InvalidPolicy) {
  // Some input that matches our policies schema but breaks FPS invariants.
  // For more test coverage, see the ParseSetsFromEnterprisePolicy unit tests.
  base::Value input = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite2.test"]
                  }
                ]
              }
            )")
                          .value();
  // Validation fails with an error.
  std::pair<absl::optional<FirstPartySetsHandler::ParseError>,
            std::vector<FirstPartySetsHandler::ParseWarning>>
      opt_error_and_warnings =
          FirstPartySetsHandler::ValidateEnterprisePolicy(input.GetDict());
  EXPECT_TRUE(opt_error_and_warnings.first.has_value());
  // An appropriate ParseError is returned.
  EXPECT_EQ(
      opt_error_and_warnings.first.value(),
      FirstPartySetsHandler::ParseError(ParseErrorType::kNonDisjointSets,
                                        {kAdditionsField, 0, kPrimaryField}));
}

class FirstPartySetsHandlerImplTest : public ::testing::Test {
 public:
  explicit FirstPartySetsHandlerImplTest(bool enabled) {
    FirstPartySetsHandlerImpl::GetInstance()->SetEnabledForTesting(enabled);

    CHECK(scoped_dir_.CreateUniqueTempDir());
    CHECK(PathExists(scoped_dir_.GetPath()));
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
  base::test::TaskEnvironment env_;
};

class FirstPartySetsHandlerImplEnabledTest
    : public FirstPartySetsHandlerImplTest {
 public:
  FirstPartySetsHandlerImplEnabledTest()
      : FirstPartySetsHandlerImplTest(true) {}
};

TEST_F(FirstPartySetsHandlerImplEnabledTest, EmptyDBPath) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite1.test"));

  // Empty `user_data_dir` will fail to load persisted sets, but that will not
  // prevent `on_sets_ready` from being invoked.
  FirstPartySetsHandlerImpl::GetInstance()->Init(
      /*user_data_dir=*/{},
      LocalSetDeclaration(
          R"({"primary": "https://example.test",)"
          R"("associatedSites": ["https://associatedsite1.test"]})"));

  EXPECT_THAT(
      GetSetsAndWait().FindEntries({example, associated}, /*config=*/nullptr),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, absl::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ClearSiteDataOnChangedSetsForContext_Successful) {
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  FirstPartySetsHandlerImpl::GetInstance()
      ->SetEmbedderWillProvidePublicSetsForTesting(true);
  const std::string input =
      R"({"primary": "https://foo.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  FirstPartySetsHandlerImpl::GetInstance()->SetPublicFirstPartySets(
      base::Version(), WritePublicSetsFile(input));

  FirstPartySetsHandlerImpl::GetInstance()->Init(scoped_dir_.GetPath(),
                                                 LocalSetDeclaration());
  ASSERT_THAT(
      GetSetsAndWait().FindEntries({foo, associated}, /*config=*/nullptr),
      UnorderedElementsAre(
          Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                            absl::nullopt)),
          Pair(associated,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0))));

  FirstPartySetsHandlerImpl::GetInstance()
      ->ClearSiteDataOnChangedSetsForContext(
          base::BindRepeating(&FakeBrowserContextGetter), "profile",
          /*context_config=*/nullptr, base::DoNothing());

  env().RunUntilIdle();

  EXPECT_THAT(GetPersistedPublicSetsAndWait(),
              Optional(UnorderedElementsAre(
                  Pair(foo, net::FirstPartySetEntry(
                                foo, net::SiteType::kPrimary, absl::nullopt)),
                  Pair(associated,
                       net::FirstPartySetEntry(foo, net::SiteType::kAssociated,
                                               absl::nullopt)))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ClearSiteDataOnChangedSetsForContext_EmptyDBPath) {
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  FirstPartySetsHandlerImpl::GetInstance()
      ->SetEmbedderWillProvidePublicSetsForTesting(true);
  const std::string input =
      R"({"primary": "https://foo.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  FirstPartySetsHandlerImpl::GetInstance()->SetPublicFirstPartySets(
      base::Version(), WritePublicSetsFile(input));

  FirstPartySetsHandlerImpl::GetInstance()->Init(
      /*user_data_dir=*/{}, LocalSetDeclaration());
  ASSERT_THAT(
      GetSetsAndWait().FindEntries({foo, associated}, /*config=*/nullptr),
      UnorderedElementsAre(
          Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                            absl::nullopt)),
          Pair(associated,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0))));

  FirstPartySetsHandlerImpl::GetInstance()
      ->ClearSiteDataOnChangedSetsForContext(
          base::BindRepeating(&FakeBrowserContextGetter), "profile",
          /*context_config=*/nullptr, base::DoNothing());

  env().RunUntilIdle();

  EXPECT_THAT(GetPersistedPublicSetsAndWait(), absl::nullopt);
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       GetSetsIfEnabledAndReady_AfterSetsReady) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  FirstPartySetsHandlerImpl::GetInstance()
      ->SetEmbedderWillProvidePublicSetsForTesting(true);

  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  FirstPartySetsHandlerImpl::GetInstance()->SetPublicFirstPartySets(
      base::Version(), WritePublicSetsFile(input));

  FirstPartySetsHandlerImpl::GetInstance()->Init(scoped_dir_.GetPath(),
                                                 LocalSetDeclaration());
  EXPECT_THAT(
      GetSetsAndWait().FindEntries({example, associated}, /*config=*/nullptr),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, absl::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));

  env().RunUntilIdle();

  EXPECT_THAT(
      FirstPartySetsHandlerImpl::GetInstance()
          ->GetSets(base::NullCallback())
          .value()
          .FindEntries({example, associated}, /*config=*/nullptr),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, absl::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       GetSetsIfEnabledAndReady_BeforeSetsReady) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  FirstPartySetsHandlerImpl::GetInstance()
      ->SetEmbedderWillProvidePublicSetsForTesting(true);

  // Call GetSets before the sets are ready, and before Init has been called.
  base::test::TestFuture<net::PublicSets> future;
  EXPECT_EQ(
      FirstPartySetsHandlerImpl::GetInstance()->GetSets(future.GetCallback()),
      absl::nullopt);

  FirstPartySetsHandlerImpl::GetInstance()->Init(scoped_dir_.GetPath(),
                                                 LocalSetDeclaration());

  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  FirstPartySetsHandlerImpl::GetInstance()->SetPublicFirstPartySets(
      base::Version(), WritePublicSetsFile(input));

  EXPECT_THAT(
      future.Get().FindEntries({example, associated}, /*config=*/nullptr),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, absl::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));

  EXPECT_THAT(
      FirstPartySetsHandlerImpl::GetInstance()
          ->GetSets(base::NullCallback())
          .value()
          .FindEntries({example, associated}, /*config=*/nullptr),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, absl::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));
}

class FirstPartySetsHandlerGetCustomizationForPolicyTest
    : public FirstPartySetsHandlerImplEnabledTest {
 public:
  FirstPartySetsHandlerGetCustomizationForPolicyTest() {
    FirstPartySetsHandlerImpl::GetInstance()
        ->SetEmbedderWillProvidePublicSetsForTesting(true);
    FirstPartySetsHandlerImpl::GetInstance()->Init(scoped_dir_.GetPath(),
                                                   LocalSetDeclaration());
  }

  // Writes the public list of First-Party Sets which GetCustomizationForPolicy
  // awaits.
  //
  // Initializes the First-Party Sets with the following relationship:
  //
  // [
  //   {
  //     "primary": "https://primary1.test",
  //     "associatedSites": ["https://associatedsite1.test",
  //     "https://associatedsite2.test"]
  //   }
  // ]
  void InitPublicFirstPartySets() {
    net::SchemefulSite primary1(GURL("https://primary1.test"));
    net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
    net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

    const std::string input =
        R"({"primary": "https://primary1.test", )"
        R"("associatedSites": ["https://associatedsite1.test", "https://associatedsite2.test"]})";
    ASSERT_TRUE(base::JSONReader::Read(input));
    FirstPartySetsHandlerImpl::GetInstance()->SetPublicFirstPartySets(
        base::Version(), WritePublicSetsFile(input));

    ASSERT_THAT(GetSetsAndWait().FindEntries(
                    {primary1, associated1, associated2}, /*config=*/nullptr),
                SizeIs(3));
  }

 protected:
  base::OnceCallback<void(net::FirstPartySetsContextConfig)>
  GetConfigCallback() {
    return future_.GetCallback();
  }

  net::FirstPartySetsContextConfig GetConfig() { return future_.Take(); }

 private:
  base::test::TestFuture<net::FirstPartySetsContextConfig> future_;
};

TEST_F(FirstPartySetsHandlerGetCustomizationForPolicyTest,
       DefaultOverridesPolicy_DefaultCustomizations) {
  base::Value policy = base::JSONReader::Read(R"({})").value();
  FirstPartySetsHandlerImpl::GetInstance()->GetCustomizationForPolicy(
      policy.GetDict(), GetConfigCallback());

  InitPublicFirstPartySets();
  EXPECT_EQ(GetConfig(), net::FirstPartySetsContextConfig());
}

TEST_F(FirstPartySetsHandlerGetCustomizationForPolicyTest,
       MalformedOverridesPolicy_DefaultCustomizations) {
  base::Value policy = base::JSONReader::Read(R"({
    "replacements": 123,
    "additions": true
  })")
                           .value();
  FirstPartySetsHandlerImpl::GetInstance()->GetCustomizationForPolicy(
      policy.GetDict(), GetConfigCallback());

  InitPublicFirstPartySets();
  EXPECT_EQ(GetConfig(), net::FirstPartySetsContextConfig());
}

TEST_F(FirstPartySetsHandlerGetCustomizationForPolicyTest,
       NonDefaultOverridesPolicy_NonDefaultCustomizations) {
  base::Value policy = base::JSONReader::Read(R"(
                {
                "replacements": [
                  {
                    "primary": "https://associatedsite1.test",
                    "associatedSites": ["https://primary3.test"]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite2.test"]
                  }
                ]
              }
            )")
                           .value();
  FirstPartySetsHandlerImpl::GetInstance()->GetCustomizationForPolicy(
      policy.GetDict(), GetConfigCallback());

  InitPublicFirstPartySets();
  EXPECT_THAT(
      GetConfig().customizations(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://primary1.test"),
               Optional(net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://primary2.test")),
                   net::SiteType::kAssociated, absl::nullopt))),
          Pair(SerializesTo("https://associatedsite1.test"),
               Optional(net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://associatedsite1.test")),
                   net::SiteType::kPrimary, absl::nullopt))),
          Pair(SerializesTo("https://primary3.test"),
               Optional(net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://associatedsite1.test")),
                   net::SiteType::kAssociated, absl::nullopt))),
          Pair(SerializesTo("https://associatedsite2.test"),
               Optional(net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://primary2.test")),
                   net::SiteType::kAssociated, absl::nullopt))),
          Pair(SerializesTo("https://primary2.test"),
               Optional(net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://primary2.test")),
                   net::SiteType::kPrimary, absl::nullopt)))));
}

TEST(FirstPartySetsProfilePolicyCustomizations, EmptyPolicySetLists) {
  EXPECT_EQ(FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
                net::PublicSets(
                    /*entries=*/MakeFlattenedSetsFromMap(
                        {{"https://primary1.test",
                          {"https://associatedsite1.test"}}}),
                    /*aliases=*/{}),
                MakeParsedPolicyFromMap({}, {})),
            net::FirstPartySetsContextConfig());
}

TEST(FirstPartySetsProfilePolicyCustomizations,
     Replacements_NoIntersection_NoRemoval) {
  net::FirstPartySetsContextConfig config =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          net::PublicSets(
              /*entries=*/MakeFlattenedSetsFromMap(
                  {{"https://primary1.test",
                    {"https://associatedsite1.test"}}}),
              /*aliases=*/{}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://primary2.test",
                                 {"https://associatedsite2.test"}}},
              /*additions=*/{}));
  EXPECT_THAT(config.customizations(),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://associatedsite2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary2.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://primary2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary2.test")),
                           net::SiteType::kPrimary, absl::nullopt)))));
}

// The common associated site between the policy and existing set is removed
// from its previous set.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Replacements_ReplacesExistingAssociatedSite_RemovedFromFormerSet) {
  net::FirstPartySetsContextConfig config =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          net::PublicSets(
              /*entries=*/MakeFlattenedSetsFromMap(
                  {{"https://primary1.test",
                    {"https://associatedsite1a.test",
                     "https://associatedsite1b.test"}}}),
              /*aliases=*/{}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://primary2.test",
                                 {"https://associatedsite1b.test"}}},
              /*additions=*/{}));
  EXPECT_THAT(config.customizations(),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://associatedsite1b.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary2.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://primary2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary2.test")),
                           net::SiteType::kPrimary, absl::nullopt)))));
}

// The common primary between the policy and existing set is removed and its
// former associated sites are removed since they are now unowned.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Replacements_ReplacesExistingPrimary_RemovesFormerAssociatedSites) {
  net::FirstPartySetsContextConfig config =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          net::PublicSets(
              /*entries=*/MakeFlattenedSetsFromMap(
                  {{"https://primary1.test",
                    {"https://associatedsite1a.test",
                     "https://associatedsite1b.test"}}}),
              /*aliases=*/{}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://primary1.test",
                                 {"https://associatedsite2.test"}}},
              /*additions=*/{}));
  EXPECT_THAT(
      config.customizations(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://associatedsite2.test"),
               Optional(net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://primary1.test")),
                   net::SiteType::kAssociated, absl::nullopt))),
          Pair(SerializesTo("https://primary1.test"),
               Optional(net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://primary1.test")),
                   net::SiteType::kPrimary, absl::nullopt))),
          Pair(SerializesTo("https://associatedsite1a.test"), absl::nullopt),
          Pair(SerializesTo("https://associatedsite1b.test"), absl::nullopt)));
}

// The common associated site between the policy and existing set is removed and
// any leftover singletons are deleted.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Replacements_ReplacesExistingAssociatedSite_RemovesSingletons) {
  net::FirstPartySetsContextConfig config =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          net::PublicSets(
              /*entries=*/MakeFlattenedSetsFromMap(
                  {{"https://primary1.test",
                    {"https://associatedsite1.test"}}}),
              /*aliases=*/{}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://primary3.test",
                                 {"https://associatedsite1.test"}}},
              /*additions=*/{}));
  EXPECT_THAT(config.customizations(),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://associatedsite1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary3.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://primary3.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary3.test")),
                           net::SiteType::kPrimary, absl::nullopt))),
                  Pair(SerializesTo("https://primary1.test"), absl::nullopt)));
}

// The policy set and the existing set have nothing in common so the policy set
// gets added in without updating the existing set.
TEST(FirstPartySetsProfilePolicyCustomizations,
     Additions_NoIntersection_AddsWithoutUpdating) {
  net::FirstPartySetsContextConfig config =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          net::PublicSets(
              /*entries=*/MakeFlattenedSetsFromMap(
                  {{"https://primary1.test",
                    {"https://associatedsite1.test"}}}),
              /*aliases=*/{}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{},
              /*additions=*/{{"https://primary2.test",
                              {"https://associatedsite2.test"}}}));
  EXPECT_THAT(config.customizations(),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://associatedsite2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary2.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://primary2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary2.test")),
                           net::SiteType::kPrimary, absl::nullopt)))));
}

// The primary of a policy set is also an associated site in an existing set.
// The policy set absorbs all sites in the existing set into its
// associated sites.
TEST(
    FirstPartySetsProfilePolicyCustomizations,
    Additions_PolicyPrimaryIsExistingAssociatedSite_PolicySetAbsorbsExistingSet) {
  net::FirstPartySetsContextConfig config =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          net::PublicSets(
              /*entries=*/MakeFlattenedSetsFromMap(
                  {{"https://primary1.test",
                    {"https://associatedsite2.test"}}}),
              /*aliases=*/{}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{},
              /*additions=*/{{"https://associatedsite2.test",
                              {"https://associatedsite2a.test",
                               "https://associatedsite2b.test"}}}));
  EXPECT_THAT(
      config.customizations(),
      UnorderedElementsAre(
          Pair(SerializesTo("https://primary1.test"),
               Optional(net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://associatedsite2.test")),
                   net::SiteType::kAssociated, absl::nullopt))),
          Pair(SerializesTo("https://associatedsite2a.test"),
               Optional(net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://associatedsite2.test")),
                   net::SiteType::kAssociated, absl::nullopt))),
          Pair(SerializesTo("https://associatedsite2b.test"),
               Optional(net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://associatedsite2.test")),
                   net::SiteType::kAssociated, absl::nullopt))),
          Pair(SerializesTo("https://associatedsite2.test"),
               Optional(net::FirstPartySetEntry(
                   net::SchemefulSite(GURL("https://associatedsite2.test")),
                   net::SiteType::kPrimary, absl::nullopt)))));
}

// The primary of a policy set is also a primary of an existing set.
// The policy set absorbs all of its primary's existing associated sites into
// its associated sites.
TEST(
    FirstPartySetsProfilePolicyCustomizations,
    Additions_PolicyPrimaryIsExistingPrimary_PolicySetAbsorbsExistingAssociatedSites) {
  net::FirstPartySetsContextConfig config =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          net::PublicSets(
              /*entries=*/MakeFlattenedSetsFromMap(
                  {{"https://primary1.test",
                    {"https://associatedsite1.test",
                     "https://associatedsite3.test"}}}),
              /*aliases=*/{}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{},
              /*additions=*/{{"https://primary1.test",
                              {"https://associatedsite2.test"}}}));
  EXPECT_THAT(config.customizations(),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://associatedsite2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary1.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://associatedsite1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary1.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://associatedsite3.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary1.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://primary1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary1.test")),
                           net::SiteType::kPrimary, absl::nullopt)))));
}

TEST(FirstPartySetsProfilePolicyCustomizations,
     TransitiveOverlap_TwoCommonPrimarys) {
  net::SchemefulSite primary0(GURL("https://primary0.test"));
  net::SchemefulSite associated_site0(GURL("https://associatedsite0.test"));
  net::SchemefulSite primary1(GURL("https://primary1.test"));
  net::SchemefulSite associated_site1(GURL("https://associatedsite1.test"));
  net::SchemefulSite primary2(GURL("https://primary2.test"));
  net::SchemefulSite associated_site2(GURL("https://associatedsite2.test"));
  net::SchemefulSite primary42(GURL("https://primary42.test"));
  net::SchemefulSite associated_site42(GURL("https://associatedsite42.test"));
  // {primary1, {associated_site1}} and {primary2, {associated_site2}}
  // transitively overlap with the existing set. primary1 takes primaryship of
  // the normalized addition set since it was provided first. The other addition
  // sets are unaffected.
  EXPECT_THAT(
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          net::PublicSets(
              /*entries=*/MakeFlattenedSetsFromMap(
                  {{"https://primary1.test", {"https://primary2.test"}}}),
              /*aliases=*/{}),
          FirstPartySetParser::ParsedPolicySetLists(
              /*replacement_list=*/{},
              {
                  SingleSet({{primary0, net::FirstPartySetEntry(
                                            primary0, net::SiteType::kPrimary,
                                            absl::nullopt)},
                             {associated_site0,
                              net::FirstPartySetEntry(
                                  primary0, net::SiteType::kAssociated,
                                  absl::nullopt)}}),
                  SingleSet({{primary1, net::FirstPartySetEntry(
                                            primary1, net::SiteType::kPrimary,
                                            absl::nullopt)},
                             {associated_site1,
                              net::FirstPartySetEntry(
                                  primary1, net::SiteType::kAssociated,
                                  absl::nullopt)}}),
                  SingleSet({{primary2, net::FirstPartySetEntry(
                                            primary2, net::SiteType::kPrimary,
                                            absl::nullopt)},
                             {associated_site2,
                              net::FirstPartySetEntry(
                                  primary2, net::SiteType::kAssociated,
                                  absl::nullopt)}}),
                  SingleSet({{primary42, net::FirstPartySetEntry(
                                             primary42, net::SiteType::kPrimary,
                                             absl::nullopt)},
                             {associated_site42,
                              net::FirstPartySetEntry(
                                  primary42, net::SiteType::kAssociated,
                                  absl::nullopt)}}),
              }))
          .customizations(),
      UnorderedElementsAre(
          Pair(associated_site0,
               absl::make_optional(net::FirstPartySetEntry(
                   primary0, net::SiteType::kAssociated, absl::nullopt))),
          Pair(associated_site1,
               absl::make_optional(net::FirstPartySetEntry(
                   primary1, net::SiteType::kAssociated, absl::nullopt))),
          Pair(associated_site2,
               absl::make_optional(net::FirstPartySetEntry(
                   primary1, net::SiteType::kAssociated, absl::nullopt))),
          Pair(associated_site42,
               absl::make_optional(net::FirstPartySetEntry(
                   primary42, net::SiteType::kAssociated, absl::nullopt))),
          Pair(primary0,
               absl::make_optional(net::FirstPartySetEntry(
                   primary0, net::SiteType::kPrimary, absl::nullopt))),
          Pair(primary1,
               absl::make_optional(net::FirstPartySetEntry(
                   primary1, net::SiteType::kPrimary, absl::nullopt))),
          Pair(primary2,
               absl::make_optional(net::FirstPartySetEntry(
                   primary1, net::SiteType::kAssociated, absl::nullopt))),
          Pair(primary42,
               absl::make_optional(net::FirstPartySetEntry(
                   primary42, net::SiteType::kPrimary, absl::nullopt)))));
}

TEST(FirstPartySetsProfilePolicyCustomizations,
     TransitiveOverlap_TwoCommonAssociatedSites) {
  net::SchemefulSite primary0(GURL("https://primary0.test"));
  net::SchemefulSite associated_site0(GURL("https://associatedsite0.test"));
  net::SchemefulSite primary1(GURL("https://primary1.test"));
  net::SchemefulSite associated_site1(GURL("https://associatedsite1.test"));
  net::SchemefulSite primary2(GURL("https://primary2.test"));
  net::SchemefulSite associated_site2(GURL("https://associatedsite2.test"));
  net::SchemefulSite primary42(GURL("https://primary42.test"));
  net::SchemefulSite associated_site42(GURL("https://associatedsite42.test"));
  // {primary1, {associated_site1}} and {primary2, {associated_site2}}
  // transitively overlap with the existing set. primary2 takes primaryship of
  // the normalized addition set since it was provided first. The other addition
  // sets are unaffected.
  EXPECT_THAT(
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          net::PublicSets(
              /*entries=*/MakeFlattenedSetsFromMap(
                  {{"https://primary2.test", {"https://primary1.test"}}}),
              /*aliases=*/{}),
          FirstPartySetParser::ParsedPolicySetLists(
              /*replacement_list=*/{},
              {
                  SingleSet({{primary0, net::FirstPartySetEntry(
                                            primary0, net::SiteType::kPrimary,
                                            absl::nullopt)},
                             {associated_site0,
                              net::FirstPartySetEntry(
                                  primary0, net::SiteType::kAssociated,
                                  absl::nullopt)}}),
                  SingleSet({{primary2, net::FirstPartySetEntry(
                                            primary2, net::SiteType::kPrimary,
                                            absl::nullopt)},
                             {associated_site2,
                              net::FirstPartySetEntry(
                                  primary2, net::SiteType::kAssociated,
                                  absl::nullopt)}}),
                  SingleSet({{primary1, net::FirstPartySetEntry(
                                            primary1, net::SiteType::kPrimary,
                                            absl::nullopt)},
                             {associated_site1,
                              net::FirstPartySetEntry(
                                  primary1, net::SiteType::kAssociated,
                                  absl::nullopt)}}),
                  SingleSet({{primary42, net::FirstPartySetEntry(
                                             primary42, net::SiteType::kPrimary,
                                             absl::nullopt)},
                             {associated_site42,
                              net::FirstPartySetEntry(
                                  primary42, net::SiteType::kAssociated,
                                  absl::nullopt)}}),
              }))
          .customizations(),
      UnorderedElementsAre(
          Pair(associated_site0,
               absl::make_optional(net::FirstPartySetEntry(
                   primary0, net::SiteType::kAssociated, absl::nullopt))),
          Pair(associated_site1,
               absl::make_optional(net::FirstPartySetEntry(
                   primary2, net::SiteType::kAssociated, absl::nullopt))),
          Pair(associated_site2,
               absl::make_optional(net::FirstPartySetEntry(
                   primary2, net::SiteType::kAssociated, absl::nullopt))),
          Pair(associated_site42,
               absl::make_optional(net::FirstPartySetEntry(
                   primary42, net::SiteType::kAssociated, absl::nullopt))),
          Pair(primary0,
               absl::make_optional(net::FirstPartySetEntry(
                   primary0, net::SiteType::kPrimary, absl::nullopt))),
          Pair(primary1,
               absl::make_optional(net::FirstPartySetEntry(
                   primary2, net::SiteType::kAssociated, absl::nullopt))),
          Pair(primary2,
               absl::make_optional(net::FirstPartySetEntry(
                   primary2, net::SiteType::kPrimary, absl::nullopt))),
          Pair(primary42,
               absl::make_optional(net::FirstPartySetEntry(
                   primary42, net::SiteType::kPrimary, absl::nullopt)))));
}

// Existing set overlaps with both replacement and addition set.
TEST(FirstPartySetsProfilePolicyCustomizations,
     ReplacementsAndAdditions_SetListsOverlapWithSameExistingSet) {
  net::FirstPartySetsContextConfig config =
      FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
          net::PublicSets(
              /*entries=*/MakeFlattenedSetsFromMap(
                  {{"https://primary1.test",
                    {"https://associatedsite1.test",
                     "https://associatedsite2.test"}}}),
              /*aliases=*/{}),
          MakeParsedPolicyFromMap(
              /*replacements=*/{{"https://primary0.test",
                                 {"https://associatedsite1.test"}}},
              /*additions=*/{{"https://primary1.test",
                              {"https://new-associatedsite1.test"}}}));
  EXPECT_THAT(config.customizations(),
              UnorderedElementsAre(
                  Pair(SerializesTo("https://associatedsite1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary0.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://primary0.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary0.test")),
                           net::SiteType::kPrimary, absl::nullopt))),
                  Pair(SerializesTo("https://new-associatedsite1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary1.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://associatedsite2.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary1.test")),
                           net::SiteType::kAssociated, absl::nullopt))),
                  Pair(SerializesTo("https://primary1.test"),
                       Optional(net::FirstPartySetEntry(
                           net::SchemefulSite(GURL("https://primary1.test")),
                           net::SiteType::kPrimary, absl::nullopt)))));
}
}  // namespace content
