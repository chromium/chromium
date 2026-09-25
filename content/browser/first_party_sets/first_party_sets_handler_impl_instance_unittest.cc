// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl_instance.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/version.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::base::test::HasValue;
using ::testing::_;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

// Some of these tests overlap with FirstPartySetParser unittests, but
// overlapping test coverage isn't the worst thing.
namespace content {

namespace {

using ParseErrorType = FirstPartySetsHandler::ParseErrorType;
using ParseWarningType = FirstPartySetsHandler::ParseWarningType;

constexpr char kDelayedQueriesCountHistogram[] =
    "Cookie.FirstPartySets.Browser.DelayedQueriesCount";
constexpr char kMostDelayedQueryDeltaHistogram[] =
    "Cookie.FirstPartySets.Browser.MostDelayedQueryDelta";

base::flat_map<net::SchemefulSite, net::FirstPartySetEntry> FindEntries(
    const FirstPartySetsHandler& handler,
    const base::flat_set<net::SchemefulSite>& sites) {
  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>> got;
  got.reserve(sites.size());
  for (const auto& site : sites) {
    std::optional<net::FirstPartySetEntry> maybe_entry =
        handler.FindEntry(site);
    if (maybe_entry) {
      got.emplace_back(site, std::move(maybe_entry).value());
    }
  }
  return got;
}

}  // namespace

class FirstPartySetsHandlerImplTest : public ::testing::Test {
 public:
  explicit FirstPartySetsHandlerImplTest(bool enabled)
      : handler_(FirstPartySetsHandlerImplInstance::CreateForTesting(
            /*enabled=*/enabled,
            /*embedder_will_provide_public_sets=*/enabled)) {
    CHECK(scoped_dir_.CreateUniqueTempDir());
    CHECK(PathExists(scoped_dir_.GetPath()));
  }

  base::File WritePublicSetsFile(std::string_view content) {
    base::FilePath path =
        scoped_dir_.GetPath().Append(FILE_PATH_LITERAL("sets_file.json"));
    CHECK(base::WriteFile(path, content));

    return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  }

  void WaitUntilInitComplete() {
    base::test::TestFuture<void> future;
    if (!handler().WhenInitComplete(future.GetCallback())) {
      EXPECT_TRUE(future.Wait());
    }
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  FirstPartySetsHandlerImplInstance& handler() { return handler_; }

 protected:
  base::ScopedTempDir scoped_dir_;

 private:
  BrowserTaskEnvironment env_;
  base::HistogramTester histogram_tester_;
  FirstPartySetsHandlerImplInstance handler_;
};

class FirstPartySetsHandlerImplDisabledTest
    : public FirstPartySetsHandlerImplTest {
 public:
  FirstPartySetsHandlerImplDisabledTest()
      : FirstPartySetsHandlerImplTest(/*enabled=*/false) {}
};

TEST_F(FirstPartySetsHandlerImplDisabledTest, InitMetrics) {
  histogram_tester().ExpectTotalCount(kDelayedQueriesCountHistogram, 1);
  histogram_tester().ExpectTotalCount(kMostDelayedQueryDeltaHistogram, 1);
}

TEST_F(FirstPartySetsHandlerImplDisabledTest, InitImmediately) {
  // Should already be able to answer queries, even before Init is called.
  EXPECT_TRUE(handler().WhenInitComplete(base::NullCallback()));

  // The public sets should be ignored, since the handler is disabled.
  handler().Init(/*user_data_dir=*/{});

  handler().SetPublicFirstPartySets(
      base::Version("0.0.1"),
      WritePublicSetsFile(
          R"({"primary": "https://example.test", )"
          R"("associatedSites": ["https://associatedsite2.test"]})"));

  WaitUntilInitComplete();

  EXPECT_THAT(
      FindEntries(handler(),
                  {
                      net::SchemefulSite(GURL("https://example.test")),
                      net::SchemefulSite(GURL("https://associatedsite1.test")),
                      net::SchemefulSite(GURL("https://associatedsite2.test")),
                  }),
      IsEmpty());
}

TEST_F(FirstPartySetsHandlerImplDisabledTest, Init_DeletesDatabaseIfExists) {
  base::FilePath db_path =
      scoped_dir_.GetPath().Append(FILE_PATH_LITERAL("first_party_sets.db"));
  ASSERT_TRUE(base::WriteFile(db_path, "dummy content"));
  ASSERT_TRUE(base::PathExists(db_path));

  handler().Init(scoped_dir_.GetPath());
  base::ThreadPoolInstance::Get()->FlushForTesting();

  EXPECT_FALSE(base::PathExists(db_path));
}

class FirstPartySetsHandlerImplEnabledTest
    : public FirstPartySetsHandlerImplTest {
 public:
  FirstPartySetsHandlerImplEnabledTest()
      : FirstPartySetsHandlerImplTest(/*enabled=*/true) {}
};

TEST_F(FirstPartySetsHandlerImplEnabledTest, EmptyDBPath) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite1.test"));

  handler().SetPublicFirstPartySets(
      base::Version("0.0.1"),
      WritePublicSetsFile(
          R"({"primary": "https://example.test",)"
          R"("associatedSites": ["https://associatedsite1.test"]})"));

  // Empty `user_data_dir` will fail to load persisted sets, but that will not
  // prevent `on_sets_ready` from being invoked.
  handler().Init(/*user_data_dir=*/{});

  WaitUntilInitComplete();

  EXPECT_THAT(FindEntries(handler(), {example, associated}),
              UnorderedElementsAre(
                  Pair(example, net::FirstPartySetEntry(
                                    example, net::SiteType::kPrimary)),
                  Pair(associated, net::FirstPartySetEntry(
                                       example, net::SiteType::kAssociated))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest, Init_DeletesDatabaseIfExists) {
  base::FilePath db_path =
      scoped_dir_.GetPath().Append(FILE_PATH_LITERAL("first_party_sets.db"));
  ASSERT_TRUE(base::WriteFile(db_path, "dummy content"));
  ASSERT_TRUE(base::PathExists(db_path));

  handler().Init(scoped_dir_.GetPath());
  base::ThreadPoolInstance::Get()->FlushForTesting();

  EXPECT_FALSE(base::PathExists(db_path));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest, Init_DatabaseDoesNotExist) {
  base::FilePath db_path =
      scoped_dir_.GetPath().Append(FILE_PATH_LITERAL("first_party_sets.db"));
  ASSERT_FALSE(base::PathExists(db_path));

  handler().Init(scoped_dir_.GetPath());
  base::ThreadPoolInstance::Get()->FlushForTesting();

  EXPECT_FALSE(base::PathExists(db_path));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest, WhenInitComplete_AfterSetsReady) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(
      base::JSONReader::Read(input, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                    WritePublicSetsFile(input));

  handler().Init(scoped_dir_.GetPath());

  WaitUntilInitComplete();

  EXPECT_THAT(FindEntries(handler(), {example, associated}),
              UnorderedElementsAre(
                  Pair(example, net::FirstPartySetEntry(
                                    example, net::SiteType::kPrimary)),
                  Pair(associated, net::FirstPartySetEntry(
                                       example, net::SiteType::kAssociated))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest, WhenInitComplete_BeforeSetsReady) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  // Call WhenInitComplete before the sets are ready, and before Init has been
  // called.
  base::test::TestFuture<void> future;
  EXPECT_FALSE(handler().WhenInitComplete(future.GetCallback()));

  handler().Init(scoped_dir_.GetPath());

  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(
      base::JSONReader::Read(input, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                    WritePublicSetsFile(input));

  EXPECT_TRUE(future.Wait());

  EXPECT_TRUE(handler().WhenInitComplete(base::NullCallback()));

  EXPECT_THAT(FindEntries(handler(), {example, associated}),
              UnorderedElementsAre(
                  Pair(example, net::FirstPartySetEntry(
                                    example, net::SiteType::kPrimary)),
                  Pair(associated, net::FirstPartySetEntry(
                                       example, net::SiteType::kAssociated))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ComputeFirstPartySetMetadata_SynchronousResult) {
  handler().Init(scoped_dir_.GetPath());

  handler().SetPublicFirstPartySets(
      base::Version("1.2.3"),
      WritePublicSetsFile(
          R"({"primary": "https://example.test", )"
          R"("associatedSites": ["https://associatedsite.test"]})"));

  WaitUntilInitComplete();

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  handler().ComputeFirstPartySetMetadata(
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://associatedsite.test")),
      future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_NE(future.Take(), net::FirstPartySetMetadata());
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ComputeFirstPartySetMetadata_AsynchronousResult) {
  // Send query before the sets are ready.
  base::test::TestFuture<net::FirstPartySetMetadata> future;
  handler().ComputeFirstPartySetMetadata(
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://associatedsite.test")),
      future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  handler().Init(scoped_dir_.GetPath());

  handler().SetPublicFirstPartySets(
      base::Version("1.2.3"),
      WritePublicSetsFile(
          R"({"primary": "https://example.test", )"
          R"("associatedSites": ["https://associatedsite.test"]})"));

  EXPECT_NE(future.Get(), net::FirstPartySetMetadata());
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ForEachEffectiveSetEntry_BeforeSetsReady) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  // Verifies calling ForEachEffectiveSetEntry before the sets are ready returns
  // false.
  EXPECT_FALSE(handler().ForEachEffectiveSetEntry(
      [&](const net::SchemefulSite& site,
          const net::FirstPartySetEntry& entry) {
        NOTREACHED();
        return true;
      }));

  handler().Init(scoped_dir_.GetPath());

  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(
      base::JSONReader::Read(input, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                    WritePublicSetsFile(input));
  WaitUntilInitComplete();

  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>
      set_entries;
  EXPECT_TRUE(handler().ForEachEffectiveSetEntry(
      [&](const net::SchemefulSite& site,
          const net::FirstPartySetEntry& entry) {
        set_entries.emplace_back(site, entry);
        return true;
      }));
  EXPECT_THAT(set_entries,
              UnorderedElementsAre(
                  Pair(example, net::FirstPartySetEntry(
                                    example, net::SiteType::kPrimary)),
                  Pair(associated, net::FirstPartySetEntry(
                                       example, net::SiteType::kAssociated))));
}

}  // namespace content
