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

constexpr char kAdditionsField[] = "additions";
constexpr char kPrimaryField[] = "primary";
constexpr char kCctldsField[] = "ccTLDs";

constexpr char kFirstPartySetsClearSiteDataOutcomeHistogram[] =
    "FirstPartySets.Initialization.ClearSiteDataOutcome";

constexpr char kDelayedQueriesCountHistogram[] =
    "Cookie.FirstPartySets.Browser.DelayedQueriesCount";
constexpr char kMostDelayedQueryDeltaHistogram[] =
    "Cookie.FirstPartySets.Browser.MostDelayedQueryDelta";

}  // namespace

TEST(FirstPartySetsHandlerImplInstance, ValidateEnterprisePolicy_ValidPolicy) {
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
  auto [success, warnings] =
      FirstPartySetsHandler::ValidateEnterprisePolicy(input.GetDict());
  EXPECT_TRUE(success.has_value());
  EXPECT_THAT(warnings, IsEmpty());
}

TEST(FirstPartySetsHandlerImplInstance,
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
  auto [success, warnings] =
      FirstPartySetsHandler::ValidateEnterprisePolicy(input.GetDict());
  EXPECT_TRUE(success.has_value());
  // Outputs metadata that can be used to surface a descriptive warning.
  EXPECT_THAT(
      warnings,
      UnorderedElementsAre(FirstPartySetsHandler::ParseWarning(
          ParseWarningType::kCctldKeyNotCanonical,
          {kAdditionsField, 0, kCctldsField, "https://non-canonical.test"})));
}

TEST(FirstPartySetsHandlerImplInstance,
     ValidateEnterprisePolicy_InvalidPolicy) {
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
  // Validation fails with an error and an appropriate ParseError is returned.
  EXPECT_THAT(
      FirstPartySetsHandler::ValidateEnterprisePolicy(input.GetDict()).first,
      base::test::ErrorIs(FirstPartySetsHandler::ParseError(
          ParseErrorType::kNonDisjointSets,
          {kAdditionsField, 0, kPrimaryField})));
}

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

  net::FirstPartySetsContextConfig GetContextConfigForPolicy(
      const base::Value::Dict* policy) {
    base::test::TestFuture<net::FirstPartySetsContextConfig> future;
    handler().GetContextConfigForPolicy(policy, future.GetCallback());
    return future.Take();
  }

  void ClearSiteDataOnChangedSetsForContextAndWait(
      FirstPartySetsHandlerImplInstance& handler,
      BrowserContext* context,
      const std::string& browser_context_id,
      net::FirstPartySetsContextConfig context_config) {
    base::RunLoop run_loop;
    handler.ClearSiteDataOnChangedSetsForContext(
        base::BindLambdaForTesting([&]() { return context; }),
        browser_context_id, std::move(context_config),
        base::BindLambdaForTesting(
            [&](net::FirstPartySetsContextConfig,
                net::FirstPartySetsCacheFilter) { run_loop.Quit(); }));
    run_loop.Run();
  }

  std::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
  GetPersistedSetsAndWait(FirstPartySetsHandlerImplInstance& handler,
                          const std::string& browser_context_id) {
    base::test::TestFuture<std::optional<
        std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>>
        future;
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
      const std::string& browser_context_id,
      net::FirstPartySetsContextConfig context_config) {
    ClearSiteDataOnChangedSetsForContextAndWait(
        handler(), context, browser_context_id, std::move(context_config));
  }

  std::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
  GetPersistedSetsAndWait(const std::string& browser_context_id) {
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

  EXPECT_EQ(GetContextConfigForPolicy(nullptr),
            net::FirstPartySetsContextConfig());

  base::Value policy = base::JSONReader::Read(R"(
                {
                "replacements": [
                  {
                    "primary": "https://primary.test",
                    "associatedSites": ["https://associated.test"]
                  }
                ]
              }
            )")
                           .value();
  EXPECT_EQ(GetContextConfigForPolicy(&policy.GetDict()),
            net::FirstPartySetsContextConfig());

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

  EXPECT_THAT(GetSetsAndWait().FindEntries(
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

  EXPECT_THAT(
      GetSetsAndWait().FindEntries({example, associated},
                                   net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));
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
  ASSERT_TRUE(base::JSONReader::Read(input));

  handler.Init(scoped_dir_.GetPath(),
               FirstPartySetParser::ParseFromCommandLine(input));

  // Should not yet be recorded.
  histogram.ExpectTotalCount(kFirstPartySetsClearSiteDataOutcomeHistogram, 0);
  ClearSiteDataOnChangedSetsForContextAndWait(
      handler, context(), browser_context_id,
      net::FirstPartySetsContextConfig());

  std::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
      persisted = GetPersistedSetsAndWait(handler, browser_context_id);
  EXPECT_TRUE(persisted.has_value());
  EXPECT_THAT(
      persisted->first.FindEntries({foo, associated}, persisted->second),
      UnorderedElementsAre(
          Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                            std::nullopt)),
          Pair(associated,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated,
                                       std::nullopt))));
  histogram.ExpectUniqueSample(kFirstPartySetsClearSiteDataOutcomeHistogram,
                               /*sample=*/true, 1);
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
    ASSERT_TRUE(base::JSONReader::Read(input));
    handler.SetPublicFirstPartySets(base::Version("0.0.1"),
                                    WritePublicSetsFile(input));

    handler.Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

    EXPECT_THAT(
        HasEntryInBrowserContextsClearedAndWait(handler, browser_context_id),
        Optional(false));

    // Should not yet be recorded.
    histogram.ExpectTotalCount(kFirstPartySetsClearSiteDataOutcomeHistogram, 0);
    ClearSiteDataOnChangedSetsForContextAndWait(
        handler, context(), browser_context_id,
        net::FirstPartySetsContextConfig());
    std::optional<
        std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
        persisted = GetPersistedSetsAndWait(handler, browser_context_id);
    EXPECT_TRUE(persisted.has_value());
    EXPECT_THAT(
        persisted->first.FindEntries({foo, associated}, persisted->second),
        UnorderedElementsAre(
            Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                              std::nullopt)),
            Pair(associated,
                 net::FirstPartySetEntry(foo, net::SiteType::kAssociated,
                                         std::nullopt))));
    EXPECT_THAT(
        HasEntryInBrowserContextsClearedAndWait(handler, browser_context_id),
        Optional(true));

    histogram.ExpectUniqueSample(kFirstPartySetsClearSiteDataOutcomeHistogram,
                                 /*sample=*/true, 1);

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
    ASSERT_TRUE(base::JSONReader::Read(input));
    // The new public sets need to be associated with a different version.
    handler.SetPublicFirstPartySets(base::Version("0.0.2"),
                                    WritePublicSetsFile(input));

    handler.Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

    // Should not yet be recorded.
    histogram.ExpectTotalCount(kFirstPartySetsClearSiteDataOutcomeHistogram, 0);
    ClearSiteDataOnChangedSetsForContextAndWait(
        handler, context(), browser_context_id,
        net::FirstPartySetsContextConfig());
    std::optional<
        std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
        persisted = GetPersistedSetsAndWait(handler, browser_context_id);
    EXPECT_TRUE(persisted.has_value());
    EXPECT_THAT(
        persisted->first.FindEntries({foo, associated2}, persisted->second),
        UnorderedElementsAre(
            Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                              std::nullopt)),
            Pair(associated2,
                 net::FirstPartySetEntry(foo, net::SiteType::kAssociated,
                                         std::nullopt))));
    EXPECT_THAT(
        HasEntryInBrowserContextsClearedAndWait(handler, browser_context_id),
        Optional(true));

    histogram.ExpectUniqueSample(kFirstPartySetsClearSiteDataOutcomeHistogram,
                                 /*sample=*/true, 1);
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
  ASSERT_TRUE(base::JSONReader::Read(input));
  handler().SetPublicFirstPartySets(base::Version("0.0.1"),
                                    WritePublicSetsFile(input));

  handler().Init(
      /*user_data_dir=*/{}, net::LocalSetDeclaration());
  ASSERT_THAT(GetSetsAndWait().FindEntries({foo, associated},
                                           net::FirstPartySetsContextConfig()),
              UnorderedElementsAre(
                  Pair(foo, net::FirstPartySetEntry(
                                foo, net::SiteType::kPrimary, std::nullopt)),
                  Pair(associated, net::FirstPartySetEntry(
                                       foo, net::SiteType::kAssociated, 0))));

  ClearSiteDataOnChangedSetsForContextAndWait(
      context(), browser_context_id, net::FirstPartySetsContextConfig());

  EXPECT_EQ(GetPersistedSetsAndWait(browser_context_id), std::nullopt);
  // Should not be recorded.
  histogram.ExpectTotalCount(kFirstPartySetsClearSiteDataOutcomeHistogram, 0);
  histogram.ExpectTotalCount(kDelayedQueriesCountHistogram, 1);
  histogram.ExpectTotalCount(kMostDelayedQueryDeltaHistogram, 1);
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ClearSiteDataOnChangedSetsForContext_BeforeSetsReady) {
  base::HistogramTester histogram;

  handler().Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

  const std::string browser_context_id = "profile";
  base::test::TestFuture<net::FirstPartySetsContextConfig,
                         net::FirstPartySetsCacheFilter>
      future;
  handler().ClearSiteDataOnChangedSetsForContext(
      base::BindLambdaForTesting([&]() { return context(); }),
      browser_context_id, net::FirstPartySetsContextConfig(),
      future.GetCallback());

  handler().SetPublicFirstPartySets(
      base::Version("0.0.1"),
      WritePublicSetsFile(
          R"({"primary": "https://foo.test", )"
          R"("associatedSites": ["https://associatedsite.test"]})"));

  EXPECT_TRUE(future.Wait());

  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  std::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
      persisted = GetPersistedSetsAndWait(browser_context_id);
  EXPECT_TRUE(persisted.has_value());
  EXPECT_THAT(
      persisted->first.FindEntries({foo, associated}, persisted->second),
      UnorderedElementsAre(
          Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                            std::nullopt)),
          Pair(associated,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated,
                                       std::nullopt))));
  histogram.ExpectUniqueSample(kFirstPartySetsClearSiteDataOutcomeHistogram,
                               /*sample=*/true, 1);
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
  ASSERT_TRUE(base::JSONReader::Read(input));
  handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                    WritePublicSetsFile(input));

  handler().Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());

  // Wait until initialization is complete.
  GetSetsAndWait();

  EXPECT_THAT(
      handler()
          .GetSets(base::NullCallback())
          .value()
          .FindEntries({example, associated},
                       net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));
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
  ASSERT_TRUE(base::JSONReader::Read(input));
  handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                    WritePublicSetsFile(input));

  EXPECT_THAT(
      future.Get().FindEntries({example, associated},
                               net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));

  EXPECT_THAT(
      handler()
          .GetSets(base::NullCallback())
          .value()
          .FindEntries({example, associated},
                       net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));
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

  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  handler().ComputeFirstPartySetMetadata(example, &associated,
                                         net::FirstPartySetsContextConfig(),
                                         future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_NE(future.Take(), net::FirstPartySetMetadata());
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ComputeFirstPartySetMetadata_AsynchronousResult) {
  // Send query before the sets are ready.
  base::test::TestFuture<net::FirstPartySetMetadata> future;
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));
  handler().ComputeFirstPartySetMetadata(example, &associated,
                                         net::FirstPartySetsContextConfig(),
                                         future.GetCallback());
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
  ASSERT_TRUE(base::JSONReader::Read(input));
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
  EXPECT_THAT(
      set_entries,
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));
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
  ASSERT_TRUE(base::JSONReader::Read(input));
  handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                    WritePublicSetsFile(input));
  // Wait for initialization is done.
  GetSetsAndWait();

  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>
      set_entries;
  // Calling ForEachEffectiveSetEntry with context config which add a new
  // associated site https://associatedsite2.test to the above set.
  EXPECT_TRUE(handler().ForEachEffectiveSetEntry(
      net::FirstPartySetsContextConfig(
          {{associated2,
            net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                example, net::SiteType::kAssociated, std::nullopt))}}),
      [&](const net::SchemefulSite& site,
          const net::FirstPartySetEntry& entry) {
        set_entries.emplace_back(site, entry);
        return true;
      }));
  EXPECT_THAT(
      set_entries,
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, std::nullopt)),
          Pair(associated1,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated, 0)),
          Pair(associated2,
               net::FirstPartySetEntry(example, net::SiteType::kAssociated,
                                       std::nullopt))));
}

class FirstPartySetsHandlerGetContextConfigForPolicyTest
    : public FirstPartySetsHandlerImplEnabledTest {
 public:
  FirstPartySetsHandlerGetContextConfigForPolicyTest() {
    handler().Init(scoped_dir_.GetPath(), net::LocalSetDeclaration());
  }

  // Writes the public list of First-Party Sets which GetContextConfigForPolicy
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
    handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                      WritePublicSetsFile(input));

    ASSERT_THAT(
        GetSetsAndWait().FindEntries({primary1, associated1, associated2},
                                     net::FirstPartySetsContextConfig()),
        SizeIs(3));
  }
};

TEST_F(FirstPartySetsHandlerGetContextConfigForPolicyTest,
       DefaultOverridesPolicy_DefaultContextConfigs) {
  base::Value policy = base::JSONReader::Read(R"({})").value();
  base::test::TestFuture<net::FirstPartySetsContextConfig> future;
  handler().GetContextConfigForPolicy(&policy.GetDict(), future.GetCallback());

  InitPublicFirstPartySets();
  EXPECT_EQ(future.Take(), net::FirstPartySetsContextConfig());
}

TEST_F(FirstPartySetsHandlerGetContextConfigForPolicyTest,
       MalformedOverridesPolicy_DefaultContextConfigs) {
  base::Value policy = base::JSONReader::Read(R"({
    "replacements": 123,
    "additions": true
  })")
                           .value();
  base::test::TestFuture<net::FirstPartySetsContextConfig> future;
  handler().GetContextConfigForPolicy(&policy.GetDict(), future.GetCallback());

  InitPublicFirstPartySets();
  EXPECT_EQ(future.Take(), net::FirstPartySetsContextConfig());
}

TEST_F(FirstPartySetsHandlerGetContextConfigForPolicyTest,
       NonDefaultOverridesPolicy_NonDefaultContextConfigs) {
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
  base::test::TestFuture<net::FirstPartySetsContextConfig> future;
  handler().GetContextConfigForPolicy(&policy.GetDict(), future.GetCallback());

  InitPublicFirstPartySets();
  // We don't care what the customizations are, here; we only care that they're
  // not a no-op.
  EXPECT_FALSE(future.Take().empty());
  EXPECT_EQ(GetContextConfigForPolicy(nullptr),
            net::FirstPartySetsContextConfig());
}

}  // namespace content
