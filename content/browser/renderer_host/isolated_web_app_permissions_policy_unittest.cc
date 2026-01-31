// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/to_vector.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using PolicyEntryPtr = blink::mojom::IsolatedAppPermissionPolicyEntryPtr;
using PermissionsPolicyFeature = network::mojom::PermissionsPolicyFeature;

class MockIsolatedWebAppContentBrowserClient : public ContentBrowserClient {
 public:
  bool ShouldUrlUseApplicationIsolationLevel(BrowserContext* browser_context,
                                             const GURL& url) override {
    return url.host() == "isolated.app";
  }

  void SetIsolatedAppPolicy(std::optional<std::vector<PolicyEntryPtr>> policy) {
    isolated_app_policy_ = std::move(policy);
  }

  std::optional<std::vector<PolicyEntryPtr>>
  GetPermissionsPolicyForIsolatedWebApp(
      BrowserContext* browser_context,
      const url::Origin& app_origin) override {
    if (!isolated_app_policy_) {
      return std::nullopt;
    }
    return base::ToVector(*isolated_app_policy_,
                          [](const auto& entry) { return entry.Clone(); });
  }

 private:
  std::optional<std::vector<PolicyEntryPtr>> isolated_app_policy_;
};

class IsolatedWebAppPermissionsPolicyTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    original_client_ = SetBrowserClientForTesting(&client_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(original_client_);
    RenderViewHostTestHarness::TearDown();
  }

  MockIsolatedWebAppContentBrowserClient& client() { return client_; }

  std::unique_ptr<NavigationSimulator> CreateBrowserInitiatedSimulator(
      const GURL& url) {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(url, web_contents());
    simulator->SetResponseHeaders(GetCoopCoepHeaders());
    return simulator;
  }

  std::unique_ptr<NavigationSimulator> CreateRendererInitiatedSimulator(
      const GURL& url) {
    auto simulator =
        NavigationSimulator::CreateRendererInitiated(url, main_rfh());
    simulator->SetResponseHeaders(GetCoopCoepHeaders());
    return simulator;
  }

  MockRenderProcessHost* GetMockRenderProcessHost() {
    return static_cast<MockRenderProcessHost*>(main_rfh()->GetProcess());
  }

  network::ParsedPermissionsPolicy CreateHeaderPolicy(
      std::initializer_list<PermissionsPolicyFeature> features) {
    return base::ToVector(features, [](PermissionsPolicyFeature feature) {
      return network::ParsedPermissionsPolicyDeclaration(
          feature, /*allowed_origins=*/{}, /*self_if_matches=*/std::nullopt,
          /*matches_all_origins=*/false, /*matches_opaque_src=*/false);
    });
  }

  std::vector<PolicyEntryPtr> CreateManifestPolicy(
      std::initializer_list<std::string> features) {
    return base::ToVector(features, [](const std::string& feature) {
      return blink::mojom::IsolatedAppPermissionPolicyEntry::New(
          feature, std::vector<std::string>());
    });
  }

 private:
  scoped_refptr<net::HttpResponseHeaders> GetCoopCoepHeaders() {
    auto headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    headers->AddHeader("Cross-Origin-Embedder-Policy", "require-corp");
    headers->AddHeader("Cross-Origin-Opener-Policy", "same-origin");
    return headers;
  }

  MockIsolatedWebAppContentBrowserClient client_;
  raw_ptr<ContentBrowserClient> original_client_;
};

TEST_F(IsolatedWebAppPermissionsPolicyTest, CompromisedRendererDetected) {
  const GURL kAppUrl("https://isolated.app");
  CreateBrowserInitiatedSimulator(kAppUrl)->Commit();

  EXPECT_TRUE(static_cast<SiteInstanceImpl*>(main_rfh()->GetSiteInstance())
                  ->GetWebExposedIsolationInfo()
                  .is_isolated_application());

  // Set up "manifest" policy: allow only 'geolocation'.
  client().SetIsolatedAppPolicy(CreateManifestPolicy({"geolocation"}));

  // Simulate renderer sending 'camera' in headers, which is not in manifest.
  auto renderer_simulator =
      CreateRendererInitiatedSimulator(kAppUrl.Resolve("/foo"));
  renderer_simulator->SetPermissionsPolicyHeader(
      CreateHeaderPolicy({PermissionsPolicyFeature::kCamera}));
  renderer_simulator->Commit();

  EXPECT_EQ(1, GetMockRenderProcessHost()->bad_msg_count());
}

TEST_F(IsolatedWebAppPermissionsPolicyTest, ValidRendererPolicyAccepted) {
  const GURL kAppUrl("https://isolated.app");
  CreateBrowserInitiatedSimulator(kAppUrl)->Commit();

  client().SetIsolatedAppPolicy(CreateManifestPolicy({"camera"}));

  auto renderer_simulator =
      CreateRendererInitiatedSimulator(kAppUrl.Resolve("/foo"));
  renderer_simulator->SetPermissionsPolicyHeader(
      CreateHeaderPolicy({PermissionsPolicyFeature::kCamera}));
  renderer_simulator->Commit();

  EXPECT_EQ(0, GetMockRenderProcessHost()->bad_msg_count());
}

TEST_F(IsolatedWebAppPermissionsPolicyTest, MultiplePolicies_ValidSubset) {
  const GURL kAppUrl("https://isolated.app");
  CreateBrowserInitiatedSimulator(kAppUrl)->Commit();

  client().SetIsolatedAppPolicy(CreateManifestPolicy({"camera", "microphone"}));

  auto renderer_simulator =
      CreateRendererInitiatedSimulator(kAppUrl.Resolve("/foo"));
  renderer_simulator->SetPermissionsPolicyHeader(
      CreateHeaderPolicy({PermissionsPolicyFeature::kCamera}));
  renderer_simulator->Commit();

  EXPECT_EQ(0, GetMockRenderProcessHost()->bad_msg_count());
}

TEST_F(IsolatedWebAppPermissionsPolicyTest, MultiplePolicies_InvalidSuperset) {
  const GURL kAppUrl("https://isolated.app");
  CreateBrowserInitiatedSimulator(kAppUrl)->Commit();

  client().SetIsolatedAppPolicy(CreateManifestPolicy({"camera"}));

  auto renderer_simulator =
      CreateRendererInitiatedSimulator(kAppUrl.Resolve("/foo"));
  renderer_simulator->SetPermissionsPolicyHeader(
      CreateHeaderPolicy({PermissionsPolicyFeature::kCamera,
                          PermissionsPolicyFeature::kMicrophone}));
  renderer_simulator->Commit();

  EXPECT_EQ(1, GetMockRenderProcessHost()->bad_msg_count());
}

TEST_F(IsolatedWebAppPermissionsPolicyTest, EmptyRendererPolicyAccepted) {
  const GURL kAppUrl("https://isolated.app");
  CreateBrowserInitiatedSimulator(kAppUrl)->Commit();

  client().SetIsolatedAppPolicy(CreateManifestPolicy({"camera"}));

  auto renderer_simulator =
      CreateRendererInitiatedSimulator(kAppUrl.Resolve("/foo"));
  renderer_simulator->SetPermissionsPolicyHeader({});
  renderer_simulator->Commit();

  EXPECT_EQ(0, GetMockRenderProcessHost()->bad_msg_count());
}

TEST_F(IsolatedWebAppPermissionsPolicyTest,
       MissingManifestPolicyTriggersBadMessage) {
  const GURL kAppUrl("https://isolated.app");
  CreateBrowserInitiatedSimulator(kAppUrl)->Commit();

  auto renderer_simulator =
      CreateRendererInitiatedSimulator(kAppUrl.Resolve("/foo"));
  renderer_simulator->SetPermissionsPolicyHeader(
      CreateHeaderPolicy({PermissionsPolicyFeature::kCamera}));
  renderer_simulator->Commit();

  EXPECT_EQ(1, GetMockRenderProcessHost()->bad_msg_count());
}

TEST_F(IsolatedWebAppPermissionsPolicyTest,
       EmptyManifestPolicyRejectsAnyRendererPolicy) {
  const GURL kAppUrl("https://isolated.app");
  CreateBrowserInitiatedSimulator(kAppUrl)->Commit();

  client().SetIsolatedAppPolicy({});

  auto renderer_simulator =
      CreateRendererInitiatedSimulator(kAppUrl.Resolve("/foo"));
  renderer_simulator->SetPermissionsPolicyHeader(
      CreateHeaderPolicy({PermissionsPolicyFeature::kCamera}));
  renderer_simulator->Commit();

  EXPECT_EQ(1, GetMockRenderProcessHost()->bad_msg_count());
}

}  // namespace
}  // namespace content
