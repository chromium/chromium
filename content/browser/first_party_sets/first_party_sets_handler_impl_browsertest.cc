// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"

#include <optional>

#include "base/test/test_future.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

constexpr char kSiteA[] = "https://a.test";
constexpr char kSiteB[] = "https://b.test";
constexpr char kSiteC[] = "https://c.test";
constexpr char kSiteD[] = "https://d.test";

}  // namespace

class FirstPartySetsHandlerImplBrowserTest
    : public content::ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_test_server()->AddDefaultHandlers();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        network::switches::kUseFirstPartySet,
        base::StringPrintf(R"({"primary": "%s",)"
                           R"("associatedSites": ["%s","%s"]})",
                           kSiteA, kSiteB, kSiteC));
  }

  net::GlobalFirstPartySets GetGlobalSets() {
    if (sets_.has_value()) {
      return sets_->Clone();
    }

    base::test::TestFuture<net::GlobalFirstPartySets> future_;
    std::optional<net::GlobalFirstPartySets> sets =
        FirstPartySetsHandlerImpl::GetInstance()->GetSets(
            future_.GetCallback());
    sets_ = sets.has_value() ? std::move(sets) : future_.Take();

    return sets_->Clone();
  }

 private:
  std::optional<net::GlobalFirstPartySets> sets_;
};

IN_PROC_BROWSER_TEST_F(FirstPartySetsHandlerImplBrowserTest, LocalSwitch) {
  // The First-Party Sets should be:
  // {primary: A, associatedSites: [B, C]}
  EXPECT_EQ(
      GetGlobalSets().FindEntry(net::SchemefulSite(GURL(kSiteB)),
                                net::FirstPartySetsContextConfig()),
      std::make_optional(net::FirstPartySetEntry(
          net::SchemefulSite(GURL(kSiteA)), net::SiteType::kAssociated, 0)));
  EXPECT_EQ(
      GetGlobalSets().FindEntry(net::SchemefulSite(GURL(kSiteC)),
                                net::FirstPartySetsContextConfig()),
      std::make_optional(net::FirstPartySetEntry(
          net::SchemefulSite(GURL(kSiteA)), net::SiteType::kAssociated, 1)));
}

// Verify that when both `kUseFirstPartySet` and `kUseRelatedWebsiteSet`
// switches present, `kUseRelatedWebsiteSet` takes precedence.
class FirstPartySetsHandlerImplWithNewSwitchBrowserTest
    : public FirstPartySetsHandlerImplBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Apply the old switch first.
    FirstPartySetsHandlerImplBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseRelatedWebsiteSet,
        base::StringPrintf(R"({"primary": "%s",)"
                           R"("associatedSites": ["%s","%s"]})",
                           kSiteD, kSiteB, kSiteC));
  }
};

IN_PROC_BROWSER_TEST_F(FirstPartySetsHandlerImplWithNewSwitchBrowserTest,
                       NewSwitch) {
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the new switch is applied, the expected First-Party Sets are:
  // {primary: D, associatedSites: [B, C]}
  EXPECT_EQ(
      GetGlobalSets().FindEntry(net::SchemefulSite(GURL(kSiteB)),
                                net::FirstPartySetsContextConfig()),
      std::make_optional(net::FirstPartySetEntry(
          net::SchemefulSite(GURL(kSiteD)), net::SiteType::kAssociated, 0)));
  EXPECT_EQ(
      GetGlobalSets().FindEntry(net::SchemefulSite(GURL(kSiteC)),
                                net::FirstPartySetsContextConfig()),
      std::make_optional(net::FirstPartySetEntry(
          net::SchemefulSite(GURL(kSiteD)), net::SiteType::kAssociated, 1)));

  EXPECT_EQ(GetGlobalSets().FindEntry(net::SchemefulSite(GURL(kSiteA)),
                                      net::FirstPartySetsContextConfig()),
            std::nullopt);
}

}  // namespace content
