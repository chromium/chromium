// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Renderer-side tests for DNS prefetch and preconnect hint emission through
// LinkLoader / PreloadHelper, verifying that whitespace normalization in
// LinkRelAttribute works correctly with Connection-Allowlist.
//
// Connection-Allowlist enforcement happens in the browser/network service
// layer (NetworkContext::ResolveHost / PreconnectSockets), NOT in the
// renderer. The renderer correctly parses rel attributes and emits hints;
// the browser is responsible for blocking non-allowlisted hosts.
//
// See also: browser-side enforcement tests in
//   chrome/browser/predictors/loading_predictor_browsertest.cc
//   (ConnectionAllowlistLoadingPredictorBrowserTest)

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/network/public/cpp/connection_allowlist.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/link_rel_attribute.h"
#include "third_party/blink/renderer/core/loader/link_loader.h"
#include "third_party/blink/renderer/core/loader/link_loader_client.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

namespace {

class MockLinkLoaderClient final
    : public GarbageCollected<MockLinkLoaderClient>,
      public LinkLoaderClient {
 public:
  explicit MockLinkLoaderClient(bool should_load) : should_load_(should_load) {}

  void Trace(Visitor* visitor) const override {
    LinkLoaderClient::Trace(visitor);
  }

  bool ShouldLoadLink() override { return should_load_; }
  bool IsLinkCreatedByParser() override { return true; }
  void LinkLoaded() override {}
  void LinkLoadingErrored() override {}

 private:
  const bool should_load_;
};

class NetworkHintsMock : public WebPrescientNetworking {
 public:
  NetworkHintsMock() = default;

  void PrefetchDNS(const WebURL& url) override { did_dns_prefetch_ = true; }
  void Preconnect(const WebURL& url, bool allow_credentials) override {
    did_preconnect_ = true;
  }

  bool DidDnsPrefetch() const { return did_dns_prefetch_; }
  bool DidPreconnect() const { return did_preconnect_; }

 private:
  bool did_dns_prefetch_ = false;
  bool did_preconnect_ = false;
};

std::unique_ptr<PolicyContainer> CreatePolicyContainerWithAllowlist(
    const std::vector<std::string>& allowed_patterns) {
  auto policies = mojom::blink::PolicyContainerPolicies::New();

  network::ConnectionAllowlist enforced;
  for (const auto& pattern : allowed_patterns) {
    enforced.allowlist.push_back(pattern);
  }
  network::ConnectionAllowlists allowlists;
  allowlists.enforced = std::move(enforced);
  policies->connection_allowlists = std::move(allowlists);

  mojo::AssociatedRemote<mojom::blink::PolicyContainerHost> dummy_host;
  std::ignore = dummy_host.BindNewEndpointAndPassDedicatedReceiver();

  return std::make_unique<PolicyContainer>(dummy_host.Unbind(),
                                           std::move(policies));
}

}  // namespace

class ConnectionAllowlistPreloadTest : public testing::Test,
                                       private ScopedMockOverlayScrollbars {
 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

// LinkRelAttribute does not normalize leading
// ASCII whitespace, so "\tdns-prefetch" is not recognized as dns-prefetch.
// Per the HTML spec, rel tokens are split on ASCII whitespace, meaning
// "\tdns-prefetch" should be parsed as the single token "dns-prefetch".
//
// Part 1 verifies that the tab-prefixed rel IS recognized and triggers a DNS
// prefetch hint without any allowlist present.
//
// Part 2 verifies that the whitespace-normalized rel is still correctly
// recognized and emitted even when Connection-Allowlist is active. The
// renderer should emit the hint to the browser; blocking is handled by
// the browser/network service layer.
TEST_F(ConnectionAllowlistPreloadTest, WhitespaceInRelAttributeNormalized) {
  // Part 1: Verify "\tdns-prefetch" IS recognized WITHOUT an allowlist.
  {
    auto dummy_page_holder =
        std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
    dummy_page_holder->GetDocument().GetSettings()->SetDNSPrefetchingEnabled(
        true);
    dummy_page_holder->GetFrame().SetPrescientNetworkingForTesting(
        std::make_unique<NetworkHintsMock>());
    auto* mock = static_cast<NetworkHintsMock*>(
        dummy_page_holder->GetFrame().PrescientNetworking());

    Persistent<MockLinkLoaderClient> client =
        MakeGarbageCollected<MockLinkLoaderClient>(true);
    auto* loader = MakeGarbageCollected<LinkLoader>(client.Get());

    KURL href_url =
        KURL(KURL(String("http://example.com")), "https://exfil.example.com/");
    LinkLoadParameters params(LinkRelAttribute("\tdns-prefetch"),
                              kCrossOriginAttributeNotSet, String(), String(),
                              String(), String(), String(), String(),
                              network::mojom::ReferrerPolicy::kDefault,
                              href_url, String(), String(), String());
    loader->LoadLink(params, dummy_page_holder->GetDocument());

    EXPECT_TRUE(mock->DidDnsPrefetch())
        << "Tab-prefixed rel '\\tdns-prefetch' should be recognized as "
           "dns-prefetch per HTML spec whitespace normalization.";
  }

  // Part 2: With Connection-Allowlist active and the feature enabled, verify
  // that "\tdns-prefetch" is still correctly recognized and the hint is
  // emitted. The browser/network service is responsible for enforcement;
  // the renderer must not silently drop the hint due to whitespace.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{network::features::kConnectionAllowlists},
        /*disabled_features=*/{});

    auto dummy_page_holder =
        std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
    dummy_page_holder->GetDocument().GetSettings()->SetDNSPrefetchingEnabled(
        true);
    dummy_page_holder->GetFrame().SetPrescientNetworkingForTesting(
        std::make_unique<NetworkHintsMock>());
    auto* mock = static_cast<NetworkHintsMock*>(
        dummy_page_holder->GetFrame().PrescientNetworking());

    dummy_page_holder->GetDocument().GetExecutionContext()->SetPolicyContainer(
        CreatePolicyContainerWithAllowlist({"https://example.com"}));

    Persistent<MockLinkLoaderClient> client =
        MakeGarbageCollected<MockLinkLoaderClient>(true);
    auto* loader = MakeGarbageCollected<LinkLoader>(client.Get());

    KURL href_url =
        KURL(KURL(String("http://example.com")), "https://exfil.example.com/");
    LinkLoadParameters params(LinkRelAttribute("\tdns-prefetch"),
                              kCrossOriginAttributeNotSet, String(), String(),
                              String(), String(), String(), String(),
                              network::mojom::ReferrerPolicy::kDefault,
                              href_url, String(), String(), String());
    loader->LoadLink(params, dummy_page_holder->GetDocument());

    EXPECT_TRUE(mock->DidDnsPrefetch())
        << "Tab-prefixed rel '\\tdns-prefetch' should be recognized and "
           "emitted even with Connection-Allowlist active. "
           "Browser-side enforcement handles blocking.";
  }
}

}  // namespace blink
