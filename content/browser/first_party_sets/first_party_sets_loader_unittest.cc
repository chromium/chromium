// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_loader.h"

#include <optional>
#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/local_set_declaration.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

namespace content {

namespace {

void SetComponentSets(FirstPartySetsLoader& loader,
                      base::Version version,
                      std::string_view content) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  base::FilePath path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("sets_file.json"));
  CHECK(base::WriteFile(path, content));

  loader.SetComponentSets(
      version, base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ));
}

}  // namespace

class FirstPartySetsLoaderTest : public ::testing::Test {
 public:
  FirstPartySetsLoaderTest() : loader_(future_.GetCallback()) {}

  FirstPartySetsLoader& loader() { return loader_; }

  net::GlobalFirstPartySets WaitAndGetResult() { return future_.Take(); }

 private:
  base::test::TaskEnvironment env_;
  base::test::TestFuture<net::GlobalFirstPartySets> future_;
  FirstPartySetsLoader loader_;
};

TEST_F(FirstPartySetsLoaderTest, IgnoresInvalidFile) {
  loader().SetManuallySpecifiedSet(net::LocalSetDeclaration());
  SetComponentSets(loader(), base::Version("1.2.3"),
                   "certainly not valid JSON");
  EXPECT_EQ(WaitAndGetResult().FindEntry(
                net::SchemefulSite(GURL("https://example.test")),
                net::FirstPartySetsContextConfig()),
            std::nullopt);
}

TEST_F(FirstPartySetsLoaderTest, IgnoresInvalidVersion) {
  loader().SetManuallySpecifiedSet(net::LocalSetDeclaration());
  SetComponentSets(
      loader(), base::Version(),
      "{\"primary\": \"https://example.test\",\"associatedSites\": "
      "[\"https://associatedsite1.test\"]}\n"
      "{\"primary\": \"https://foo.test\",\"associatedSites\": "
      "[\"https://associatedsite2.test\"]}");
  EXPECT_EQ(WaitAndGetResult().FindEntry(
                net::SchemefulSite(GURL("https://example.test")),
                net::FirstPartySetsContextConfig()),
            std::nullopt);
}

TEST_F(FirstPartySetsLoaderTest, AcceptsMultipleSets) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  SetComponentSets(
      loader(), base::Version("1.2.3"),
      "{\"primary\": \"https://example.test\",\"associatedSites\": "
      "[\"https://associatedsite1.test\"]}\n"
      "{\"primary\": \"https://foo.test\",\"associatedSites\": "
      "[\"https://associatedsite2.test\"]}");
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet(net::LocalSetDeclaration());

  EXPECT_THAT(
      WaitAndGetResult().FindEntries({example, associated1, foo, associated2},
                                     net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)),
          Pair(associated1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)),
          Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                            std::nullopt)),
          Pair(associated2,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated, 0))));
}

TEST_F(FirstPartySetsLoaderTest, SetComponentSets_Idempotent) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite example2(GURL("https://example.test"));
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite foo2(GURL("https://foo2.test"));

  SetComponentSets(loader(), base::Version("1.2.3"),
                   R"({"primary": "https://example.test",)"
                   R"("associatedSites": ["https://associatedsite1.test"]})"
                   "\n"
                   R"({"primary": "https://foo.test",)"
                   R"("associatedSites": ["https://associatedsite2.test"]})");
  SetComponentSets(loader(), base::Version("1.2.3"),
                   R"({ "primary": "https://example2.test",)"
                   R"("associatedSites": ["https://associatedsite1.test"]})"
                   "\n"
                   R"({"primary": "https://foo2.test",)"
                   R"("associatedSites": ["https://associatedsite2.test"]})");
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet(net::LocalSetDeclaration());

  // The second call to SetComponentSets should have had no effect.
  EXPECT_THAT(
      WaitAndGetResult().FindEntries({example, foo, example2, foo2},
                                     net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)),
          Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                            std::nullopt))));
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified) {
  const net::SchemefulSite bar(GURL("https://bar.test"));
  const net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  const net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  SetComponentSets(loader(), base::Version("1.2.3"),
                   R"({"primary": "https://example.test", "associatedSites": )"
                   R"(["https://associatedsite1.test"]})");
  loader().SetManuallySpecifiedSet(net::LocalSetDeclaration(
      /*set_entries=*/base::flat_map<net::SchemefulSite,
                                     net::FirstPartySetEntry>({
          {bar,
           net::FirstPartySetEntry(bar, net::SiteType::kPrimary, std::nullopt)},
          {associated2,
           net::FirstPartySetEntry(bar, net::SiteType::kAssociated, 0)},
      }),
      /*aliases=*/{}));

  EXPECT_THAT(
      WaitAndGetResult().FindEntry(associated2,
                                   net::FirstPartySetsContextConfig()),
      Optional(net::FirstPartySetEntry(bar, net::SiteType::kAssociated, 0)));
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Idempotent) {
  const net::SchemefulSite bar(GURL("https://bar.test"));
  const net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  const net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  loader().SetManuallySpecifiedSet(net::LocalSetDeclaration(
      /*set_entries=*/base::flat_map<net::SchemefulSite,
                                     net::FirstPartySetEntry>({
          {bar,
           net::FirstPartySetEntry(bar, net::SiteType::kPrimary, std::nullopt)},
          {associated1,
           net::FirstPartySetEntry(bar, net::SiteType::kAssociated, 0)},
      }),
      /*aliases=*/{}));

  // All but the first SetManuallySpecifiedSet call should be ignored.
  loader().SetManuallySpecifiedSet(net::LocalSetDeclaration(
      /*set_entries=*/base::flat_map<net::SchemefulSite,
                                     net::FirstPartySetEntry>({
          {bar,
           net::FirstPartySetEntry(bar, net::SiteType::kPrimary, std::nullopt)},
          {associated2,
           net::FirstPartySetEntry(bar, net::SiteType::kAssociated, 0)},
      }),
      /*aliases=*/{}));

  SetComponentSets(loader(), base::Version(), "");

  EXPECT_THAT(WaitAndGetResult().FindEntries(
                  {
                      associated1,
                      associated2,
                  },
                  net::FirstPartySetsContextConfig()),
              UnorderedElementsAre(
                  Pair(associated1, net::FirstPartySetEntry(
                                        bar, net::SiteType::kAssociated, 0))));
}

}  // namespace content
