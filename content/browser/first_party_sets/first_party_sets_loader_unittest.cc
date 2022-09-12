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
#include "content/browser/first_party_sets/local_set_declaration.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/public_sets.h"
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

MATCHER_P2(PublicSetsAre, sets_matcher, aliases_matcher, "") {
  const net::PublicSets& public_sets = arg;
  const base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>& sets =
      public_sets.entries();
  const base::flat_map<net::SchemefulSite, net::SchemefulSite>& aliases =
      public_sets.aliases();
  return testing::ExplainMatchResult(sets_matcher, sets, result_listener) &&
         testing::ExplainMatchResult(aliases_matcher, aliases, result_listener);
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

}  // namespace

class FirstPartySetsLoaderTest : public ::testing::Test {
 public:
  FirstPartySetsLoaderTest() : loader_(future_.GetCallback()) {}

  FirstPartySetsLoader& loader() { return loader_; }

  net::PublicSets WaitAndGetResult() { return future_.Take(); }

 private:
  base::test::TaskEnvironment env_;
  base::test::TestFuture<net::PublicSets> future_;
  FirstPartySetsLoader loader_;
};

TEST_F(FirstPartySetsLoaderTest, IgnoresInvalidFile) {
  loader().SetManuallySpecifiedSet(LocalSetDeclaration());
  const std::string input = "certainly not valid JSON";
  SetComponentSets(loader(), input);
  EXPECT_THAT(WaitAndGetResult(), PublicSetsAre(IsEmpty(), IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest, ParsesComponent) {
  SetComponentSets(loader(), "");
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet(LocalSetDeclaration());
  EXPECT_THAT(WaitAndGetResult(), PublicSetsAre(IsEmpty(), IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest, AcceptsMinimal) {
  const std::string input =
      "{\"primary\": \"https://example.test\",\"associatedSites\": "
      "[\"https://aaaa.test\",],}";
  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet(LocalSetDeclaration());

  EXPECT_THAT(WaitAndGetResult(),
              PublicSetsAre(
                  UnorderedElementsAre(
                      Pair(SerializesTo("https://example.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://aaaa.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0))),
                  IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest, AcceptsMultipleSets) {
  const std::string input =
      "{\"primary\": \"https://example.test\",\"associatedSites\": "
      "[\"https://associatedsite1.test\"]}\n"
      "{\"primary\": \"https://foo.test\",\"associatedSites\": "
      "[\"https://associatedsite2.test\"]}";

  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet(LocalSetDeclaration());

  EXPECT_THAT(WaitAndGetResult(),
              PublicSetsAre(
                  UnorderedElementsAre(
                      Pair(SerializesTo("https://example.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite1.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)),
                      Pair(SerializesTo("https://foo.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite2.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 0))),
                  IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest, SetComponentSets_Idempotent) {
  std::string input =
      R"({"primary": "https://example.test", "associatedSites": ["https://associatedsite1.test"]}
{"primary": "https://foo.test", "associatedSites": ["https://associatedsite2.test"]})";

  std::string input2 =
      R"({ "primary": "https://example2.test", "associatedSites":)"
      R"( ["https://associatedsite1.test"]}
{"primary": "https://foo2.test", "associatedSites": ["https://associatedsite2.test"]})";

  SetComponentSets(loader(), input);
  SetComponentSets(loader(), input2);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet(LocalSetDeclaration());

  EXPECT_THAT(WaitAndGetResult(),
              // The second call to SetComponentSets should have had no effect.
              PublicSetsAre(
                  UnorderedElementsAre(
                      Pair(SerializesTo("https://example.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite1.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)),
                      Pair(SerializesTo("https://foo.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite2.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 0))),
                  IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest, OwnerIsOnlyMember) {
  const std::string input =
      R"({"primary": "https://example.test", "associatedSites": ["https://example.test"]}
{"primary": "https://foo.test", "associatedSites": ["https://associatedsite2.test"]})";

  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet(LocalSetDeclaration());

  EXPECT_THAT(WaitAndGetResult(), PublicSetsAre(IsEmpty(), IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest, OwnerIsMember) {
  const std::string input =
      R"({"primary": "https://example.test", "associatedSites":)"
      R"( ["https://example.test", "https://associatedsite1.test"]}
{"primary": "https://foo.test", "associatedSites": ["https://associatedsite2.test"]})";
  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet(LocalSetDeclaration());

  EXPECT_THAT(WaitAndGetResult(), PublicSetsAre(IsEmpty(), IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest, RepeatedMember) {
  const std::string input =
      R"({"primary": "https://example.test", "associatedSites":)"
      R"( ["https://associatedsite1.test", "https://associatedsite2.test",)"
      R"( "https://associatedsite1.test"]}
{"primary": "https://foo.test", "associatedSites": ["https://associatedsite3.test"]})";

  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet(LocalSetDeclaration());

  EXPECT_THAT(WaitAndGetResult(), PublicSetsAre(IsEmpty(), IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_DeduplicatesOwnerOwner) {
  SetComponentSets(
      loader(),
      R"({"primary": "https://example.test", "associatedSites": )"
      R"(["https://associatedsite2.test", "https://associatedsite3.test"]})"
      "\n"
      R"({"primary": "https://bar.test",)"
      R"("associatedSites": ["https://associatedsite4.test"]})");
  loader().SetManuallySpecifiedSet(LocalSetDeclaration(
      R"({"primary": "https://example.test",)"
      R"("associatedSites":)"
      R"(["https://associatedsite1.test", "https://associatedsite2.test"]})"));

  EXPECT_THAT(WaitAndGetResult(),
              PublicSetsAre(
                  UnorderedElementsAre(
                      Pair(SerializesTo("https://example.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite1.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)),
                      Pair(SerializesTo("https://associatedsite2.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 1)),
                      Pair(SerializesTo("https://bar.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://bar.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite4.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://bar.test")),
                               net::SiteType::kAssociated, 0))),
                  IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_DeduplicatesOwnerMember) {
  SetComponentSets(
      loader(), R"({"primary": "https://foo.test",)"
                R"("associatedSites": )"
                R"(["https://associatedsite1.test", "https://example.test"]})"
                "\n"
                R"({"primary": "https://bar.test",)"
                R"("associatedSites": ["https://associatedsite2.test"]})");
  loader().SetManuallySpecifiedSet(LocalSetDeclaration(
      R"({"primary": "https://example.test",)"
      R"("associatedSites":)"
      R"(["https://associatedsite1.test", "https://associatedsite3.test"]})"));

  EXPECT_THAT(WaitAndGetResult(),
              PublicSetsAre(
                  UnorderedElementsAre(
                      Pair(SerializesTo("https://example.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite1.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)),
                      Pair(SerializesTo("https://associatedsite3.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 1)),
                      Pair(SerializesTo("https://bar.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://bar.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite2.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://bar.test")),
                               net::SiteType::kAssociated, 0))),
                  IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_DeduplicatesMemberOwner) {
  SetComponentSets(
      loader(),
      R"({"primary": "https://foo.test", "associatedSites": )"
      R"(["https://associatedsite1.test", "https://associatedsite2.test"]})"
      "\n"
      R"({"primary" : "https://associatedsite3.test",)"
      R"("associatedSites" : ["https://associatedsite4.test"]})");
  loader().SetManuallySpecifiedSet(LocalSetDeclaration(
      R"({"primary": "https://example.test",)"
      R"("associatedSites": ["https://associatedsite3.test"]})"));

  EXPECT_THAT(WaitAndGetResult(),
              PublicSetsAre(
                  UnorderedElementsAre(
                      Pair(SerializesTo("https://example.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite3.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)),
                      Pair(SerializesTo("https://foo.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite1.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 0)),
                      Pair(SerializesTo("https://associatedsite2.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 1))),
                  IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_DeduplicatesMemberMember) {
  SetComponentSets(
      loader(),
      R"({"primary": "https://foo.test", "associatedSites": )"
      R"(["https://associatedsite2.test", "https://associatedsite3.test"]})"
      "\n"
      R"({"primary": "https://bar.test",)"
      R"("associatedSites": ["https://associatedsite4.test"]})");
  loader().SetManuallySpecifiedSet(LocalSetDeclaration(
      R"({"primary": "https://example.test",)"
      R"("associatedSites":)"
      R"(["https://associatedsite1.test", "https://associatedsite2.test"]})"));

  EXPECT_THAT(WaitAndGetResult(),
              PublicSetsAre(
                  UnorderedElementsAre(
                      Pair(SerializesTo("https://example.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite1.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0)),
                      Pair(SerializesTo("https://associatedsite2.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 1)),
                      Pair(SerializesTo("https://foo.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite3.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://foo.test")),
                               net::SiteType::kAssociated, 1)),
                      Pair(SerializesTo("https://bar.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://bar.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite4.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://bar.test")),
                               net::SiteType::kAssociated, 0))),
                  IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_PrunesInducedSingletons) {
  SetComponentSets(loader(),
                   R"({"primary": "https://foo.test",)"
                   R"("associatedSites": ["https://associatedsite1.test"]})");
  loader().SetManuallySpecifiedSet(LocalSetDeclaration(
      R"({"primary": "https://example.test",)"
      R"("associatedSites": ["https://associatedsite1.test"]})"));

  // If we just erased entries that overlapped with the manually-supplied
  // set, https://foo.test would be left as a singleton set. But since we
  // disallow singleton sets, we ensure that such cases are caught and
  // removed.
  EXPECT_THAT(WaitAndGetResult(),
              PublicSetsAre(
                  UnorderedElementsAre(
                      Pair(SerializesTo("https://example.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kPrimary, absl::nullopt)),
                      Pair(SerializesTo("https://associatedsite1.test"),
                           net::FirstPartySetEntry(
                               net::SchemefulSite(GURL("https://example.test")),
                               net::SiteType::kAssociated, 0))),
                  IsEmpty()));
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_MergesAliases) {
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associated.test"));
  net::SchemefulSite associated_cctld1(GURL("https://associated.cctld1"));
  net::SchemefulSite associated_cctld2(GURL("https://associated.cctld2"));
  net::SchemefulSite foo_service(GURL("https://foo_service.test"));
  net::SchemefulSite foo_service_cctld(GURL("https://foo_service.cctld"));

  // Both the public sets and the locally-defined set define an alias for
  // https://associated.test, but both define a different set for that site too.
  // Only the locally-defined alias should be kept.
  SetComponentSets(
      loader(),
      R"({"primary": "https://foo.test",)"
      R"("associatedSites": ["https://associated.test"],)"
      R"("serviceSites": ["https://foo_service.test"],)"
      R"("ccTLDs": {)"
      R"(  "https://associated.test": ["https://associated.cctld1"],)"
      R"(  "https://foo_service.test": ["https://foo_service.cctld"],)"
      R"(})"
      R"(})");
  loader().SetManuallySpecifiedSet(LocalSetDeclaration(
      R"({"primary": "https://example.test",)"
      R"("associatedSites": ["https://associated.test"],)"
      R"("ccTLDs": {)"
      R"(  "https://associated.test": ["https://associated.cctld2"],)"
      R"(})"
      R"(})"));

  EXPECT_THAT(
      WaitAndGetResult(),
      PublicSetsAre(
          UnorderedElementsAre(
              Pair(example,
                   net::FirstPartySetEntry(example, net::SiteType::kPrimary,
                                           absl::nullopt)),
              Pair(associated, net::FirstPartySetEntry(
                                   example, net::SiteType::kAssociated, 0)),
              Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                                absl::nullopt)),
              Pair(foo_service,
                   net::FirstPartySetEntry(foo, net::SiteType::kService,
                                           absl::nullopt))),
          UnorderedElementsAre(Pair(foo_service_cctld, foo_service),
                               Pair(associated_cctld2, associated))));
}

}  // namespace content
