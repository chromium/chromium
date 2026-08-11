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
#include "content/public/test/test_browser_context.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/local_set_declaration.h"
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
    const net::GlobalFirstPartySets& sets,
    const base::flat_set<net::SchemefulSite>& sites,
    const net::FirstPartySetsContextConfig& config) {
  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>> got;
  got.reserve(sites.size());
  for (const auto& site : sites) {
    std::optional<net::FirstPartySetEntry> maybe_entry =
        sets.FindEntry(site, config);
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

  net::GlobalFirstPartySets GetSetsAndWait(
      FirstPartySetsHandlerImplInstance& handler) {
    base::test::TestFuture<net::GlobalFirstPartySets> future;
    std::optional<net::GlobalFirstPartySets> result =
        handler.GetSets(future.GetCallback());
    return result.has_value() ? std::move(result).value() : future.Take();
  }

  void ClearSiteDataOnChangedSetsForContextAndWait(
      FirstPartySetsHandlerImplInstance& handler,
      BrowserContext* context,
      const std::string& browser_context_id) {
    base::RunLoop run_loop;
    handler.ClearSiteDataOnChangedSetsForContext(
        base::BindLambdaForTesting([&]() { return context; }),
        browser_context_id,
        base::BindLambdaForTesting(
            [&](net::FirstPartySetsCacheFilter) { run_loop.Quit(); }));
    run_loop.Run();
  }

  std::optional<net::GlobalFirstPartySets> GetPersistedSetsAndWait(
      FirstPartySetsHandlerImplInstance& handler,
      const std::string& browser_context_id) {
    base::test::TestFuture<std::optional<net::GlobalFirstPartySets>> future;
    handler.GetPersistedSetsForTesting(browser_context_id,
                                       future.GetCallback());
    return future.Take();
  }

  std::optional<bool> HasEntryInBrowserContextsClearedAndWait(
      FirstPartySetsHandlerImplInstance& handler,
      const std::string& browser_context_id) {
    base::test::TestFuture<std::optional<bool>> future;
    handler.HasBrowserContextClearedForTesting(browser_context_id,
                                               future.GetCallback());
    return future.Take();
  }

  net::GlobalFirstPartySets GetSetsAndWait() {
    return GetSetsAndWait(handler());
  }

  void ClearSiteDataOnChangedSetsForContextAndWait(
      BrowserContext* context,
      const std::string& browser_context_id) {
    ClearSiteDataOnChangedSetsForContextAndWait(handler(), context,
                                                browser_context_id);
  }

  std::optional<net::GlobalFirstPartySets> GetPersistedSetsAndWait(
      const std::string& browser_context_id) {
    return GetPersistedSetsAndWait(handler(), browser_context_id);
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  FirstPartySetsHandlerImplInstance& handler() { return handler_; }

  BrowserContext* context() { return &context_; }

 protected:
  base::ScopedTempDir scoped_dir_;

 private:
  BrowserTaskEnvironment env_;
  TestBrowserContext context_;
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
  EXPECT_THAT(handler().GetSets(base::NullCallback()), Optional(_));



  // The local set declaration should be ignored, since the handler is disabled.
  handler().Init(
      /*user_data_dir=*/{},
      FirstPartySetParser::ParseFromCommandLine(
          R"({"primary": "https://example.test",)"
          R"("associatedSites": ["https://associatedsite1.test"]})"));

  // The public sets should be ignored, since the handler is disabled.
  handler().SetPublicFirstPartySets(
      base::Version("0.0.1"),
      WritePublicSetsFile(
          R"({"primary": "https://example.test", )"
          R"("associatedSites": ["https://associatedsite2.test"]})"));

  EXPECT_THAT(
      FindEntries(GetSetsAndWait(),
                  {
                      net::SchemefulSite(GURL("https://example.test")),
                      net::SchemefulSite(GURL("https://associatedsite1.test")),
                      net::SchemefulSite(GURL("https://associatedsite2.test")),
                  },
                  net::FirstPartySetsContextConfig()),
      IsEmpty());
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

  handler().SetPublicFirstPartySets(base::Version("0.0.1"),
                                    WritePublicSetsFile(""));

  // Empty `user_data_dir` will fail to load persisted sets, but that will not
  // prevent `on_sets_ready` from being invoked.
  handler().Init(
      /*user_data_dir=*/{},
      FirstPartySetParser::ParseFromCommandLine(
          R"({"primary": "https://example.test",)"
          R"("associatedSites": ["https://associatedsite1.test"]})"));

  EXPECT_THAT(FindEntries(GetSetsAndWait(), {example, associated},
                          net::FirstPartySetsContextConfig()),
              UnorderedElementsAre(
                  Pair(example, net::FirstPartySetEntry(
                                    example, net::SiteType::kPrimary)),
                  Pair(associated, net::FirstPartySetEntry(
                                       example, net::SiteType::kAssociated))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ClearSiteDataOnChangedSetsForContext_ManualSet_Successful) {
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  const std::string browser_context_id = "profile";

  base::HistogramTester histogram;
  FirstPartySetsHandlerImplInstance handler =
      FirstPartySetsHandlerImplInstance::CreateForTesting(true, false);
  const std::string input =
      R"({"primary": "https://foo.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(
      base::JSONReader::Read(input, base::JSON_PARSE_CHROMIUM_EXTENSIONS));

  handler.Init(scoped_dir_.GetPath(),
               FirstPartySetParser::ParseFromCommandLine(input));

  ClearSiteDataOnChangedSetsForContextAndWait(handler, context(),
                                              browser_context_id);

  std::optional<net::GlobalFirstPartySets> persisted =
      GetPersistedSetsAndWait(handler, browser_context_id);
  ASSERT_TRUE(persisted.has_value());
  EXPECT_THAT(
      FindEntries(*persisted, {foo, associated},
                  net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary)),
          Pair(associated,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated))));
  histogram.ExpectTotalCount(kDelayedQueriesCountHistogram, 1);
  histogram.ExpectTotalCount(kMostDelayedQueryDeltaHistogram, 1);
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ClearSiteDataOnChangedSetsForContext_PublicSetsWithDiff_Successful) {
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  const std::string browser_context_id = "profile";

  {
    base::HistogramTester histogram;
    FirstPartySetsHandlerImplInstance handler =
        FirstPartySetsHandlerImplInstance::CreateForTesting(true, true);
    const std::string input =
        R"({"primary": "https://foo.test", )"
        R"("associatedSites": ["https://associatedsite.test"]})";
    ASSERT_TRUE(
        base::JSONReader::Read(input, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
    handler.SetPublicFirstPartySets(base::Version("0.0.1"),
                                    WritePublicSetsFile(input));

    handler.Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

    EXPECT_THAT(
        HasEntryInBrowserContextsClearedAndWait(handler, browser_context_id),
        Optional(false));

    ClearSiteDataOnChangedSetsForContextAndWait(handler, context(),
                                                browser_context_id);
    std::optional<net::GlobalFirstPartySets> persisted =
        GetPersistedSetsAndWait(handler, browser_context_id);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_THAT(
        FindEntries(*persisted, {foo, associated},
                    net::FirstPartySetsContextConfig()),
        UnorderedElementsAre(
            Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary)),
            Pair(associated,
                 net::FirstPartySetEntry(foo, net::SiteType::kAssociated))));
    EXPECT_THAT(
        HasEntryInBrowserContextsClearedAndWait(handler, browser_context_id),
        Optional(true));

    // Make sure the database is closed properly before being opened again.
    handler.SynchronouslyResetDBHelperForTesting();
  }

  // Verify FPS transition clearing is working for non-empty sites-to-clear
  // list.
  {
    base::HistogramTester histogram;
    FirstPartySetsHandlerImplInstance handler =
        FirstPartySetsHandlerImplInstance::CreateForTesting(true, true);
    const std::string input =
        R"({"primary": "https://foo.test", )"
        R"("associatedSites": ["https://associatedsite2.test"]})";
    ASSERT_TRUE(
        base::JSONReader::Read(input, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
    // The new public sets need to be associated with a different version.
    handler.SetPublicFirstPartySets(base::Version("0.0.2"),
                                    WritePublicSetsFile(input));

    handler.Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

    ClearSiteDataOnChangedSetsForContextAndWait(handler, context(),
                                                browser_context_id);
    std::optional<net::GlobalFirstPartySets> persisted =
        GetPersistedSetsAndWait(handler, browser_context_id);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_THAT(
        FindEntries(*persisted, {foo, associated2},
                    net::FirstPartySetsContextConfig()),
        UnorderedElementsAre(
            Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary)),
            Pair(associated2,
                 net::FirstPartySetEntry(foo, net::SiteType::kAssociated))));
    EXPECT_THAT(
        HasEntryInBrowserContextsClearedAndWait(handler, browser_context_id),
        Optional(true));
  }
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ClearSiteDataOnChangedSetsForContext_EmptyDBPath) {
  base::HistogramTester histogram;
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  const std::string browser_context_id = "profile";
  const std::string input =
      R"({"primary": "https://foo.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(
      base::JSONReader::Read(input, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  handler().SetPublicFirstPartySets(base::Version("0.0.1"),
                                    WritePublicSetsFile(input));

  handler().Init(
      /*user_data_dir=*/{}, net::LocalSetDeclaration());
  ASSERT_THAT(
      FindEntries(GetSetsAndWait(), {foo, associated},
                  net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary)),
          Pair(associated,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated))));

  ClearSiteDataOnChangedSetsForContextAndWait(context(), browser_context_id);

  EXPECT_EQ(GetPersistedSetsAndWait(browser_context_id), std::nullopt);
  histogram.ExpectTotalCount(kDelayedQueriesCountHistogram, 1);
  histogram.ExpectTotalCount(kMostDelayedQueryDeltaHistogram, 1);
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ClearSiteDataOnChangedSetsForContext_BeforeSetsReady) {
  base::HistogramTester histogram;

  handler().Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

  const std::string browser_context_id = "profile";
  base::test::TestFuture<net::FirstPartySetsCacheFilter> future;
  handler().ClearSiteDataOnChangedSetsForContext(
      base::BindLambdaForTesting([&]() { return context(); }),
      browser_context_id, future.GetCallback());

  handler().SetPublicFirstPartySets(
      base::Version("0.0.1"),
      WritePublicSetsFile(
          R"({"primary": "https://foo.test", )"
          R"("associatedSites": ["https://associatedsite.test"]})"));

  EXPECT_TRUE(future.Wait());

  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  std::optional<net::GlobalFirstPartySets> persisted =
      GetPersistedSetsAndWait(browser_context_id);
  ASSERT_TRUE(persisted.has_value());
  EXPECT_THAT(
      FindEntries(*persisted, {foo, associated},
                  net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary)),
          Pair(associated,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated))));
  histogram.ExpectTotalCount(kDelayedQueriesCountHistogram, 1);
  histogram.ExpectTotalCount(kMostDelayedQueryDeltaHistogram, 1);
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       GetSetsIfEnabledAndReady_AfterSetsReady) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(
      base::JSONReader::Read(input, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                    WritePublicSetsFile(input));

  handler().Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

  // Wait until initialization is complete.
  GetSetsAndWait();

  EXPECT_THAT(
      FindEntries(handler().GetSets(base::NullCallback()).value(),
                  {example, associated}, net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(example,
               net::FirstPartySetEntry(example, net::SiteType::kPrimary)),
          Pair(associated,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       GetSetsIfEnabledAndReady_BeforeSetsReady) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  // Call GetSets before the sets are ready, and before Init has been called.
  base::test::TestFuture<net::GlobalFirstPartySets> future;
  EXPECT_EQ(handler().GetSets(future.GetCallback()), std::nullopt);

  handler().Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(
      base::JSONReader::Read(input, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                    WritePublicSetsFile(input));

  EXPECT_THAT(FindEntries(future.Get(), {example, associated},
                          net::FirstPartySetsContextConfig()),
              UnorderedElementsAre(
                  Pair(example, net::FirstPartySetEntry(
                                    example, net::SiteType::kPrimary)),
                  Pair(associated, net::FirstPartySetEntry(
                                       example, net::SiteType::kAssociated))));

  EXPECT_THAT(
      FindEntries(handler().GetSets(base::NullCallback()).value(),
                  {example, associated}, net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(example,
               net::FirstPartySetEntry(example, net::SiteType::kPrimary)),
          Pair(associated,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ComputeFirstPartySetMetadata_SynchronousResult) {
  handler().Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

  handler().SetPublicFirstPartySets(
      base::Version("1.2.3"),
      WritePublicSetsFile(
          R"({"primary": "https://example.test", )"
          R"("associatedSites": ["https://associatedsite.test"]})"));

  // Exploit another helper to wait until the public sets file has been read.
  GetSetsAndWait();

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  handler().ComputeFirstPartySetMetadata(
      net::SchemefulSite(GURL("https://example.test")),
      net::SchemefulSite(GURL("https://associatedsite.test")),
      net::FirstPartySetsContextConfig(), future.GetCallback());
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
      net::FirstPartySetsContextConfig(), future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  handler().Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

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
      net::FirstPartySetsContextConfig(),
      [&](const net::SchemefulSite& site,
          const net::FirstPartySetEntry& entry) {
        NOTREACHED();
        return true;
      }));

  handler().Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(
      base::JSONReader::Read(input, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                    WritePublicSetsFile(input));
  // Wait for initialization is done.
  GetSetsAndWait();

  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>
      set_entries;
  EXPECT_TRUE(handler().ForEachEffectiveSetEntry(
      net::FirstPartySetsContextConfig(),
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

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ForEachEffectiveSetEntry_WithNonEmptyConfig) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  handler().Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://associatedsite1.test"]})";
  ASSERT_TRUE(
      base::JSONReader::Read(input, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                    WritePublicSetsFile(input));
  // Wait for initialization is done.
  GetSetsAndWait();

  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>
      set_entries;
  // Calling ForEachEffectiveSetEntry with context config which add a new
  // associated site https://associatedsite2.test to the above set.
  EXPECT_TRUE(handler().ForEachEffectiveSetEntry(
      net::FirstPartySetsContextConfig::Create(
          {{associated2,
            net::FirstPartySetEntryOverride(
                net::FirstPartySetEntry(example, net::SiteType::kAssociated))}})
          .value(),
      [&](const net::SchemefulSite& site,
          const net::FirstPartySetEntry& entry) {
        set_entries.emplace_back(site, entry);
        return true;
      }));
  EXPECT_THAT(
      set_entries,
      UnorderedElementsAre(
          Pair(example,
               net::FirstPartySetEntry(example, net::SiteType::kPrimary)),
          Pair(associated1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated)),
          Pair(associated2,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated))));
}



}  // namespace content
