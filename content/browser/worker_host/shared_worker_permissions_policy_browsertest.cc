// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "base/strings/escape.h"
#include "base/test/run_until.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::network::mojom::PermissionsPolicyFeature;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

constexpr char kWorkerPath[] = "/shared_worker_permissions_policy.js";

// Serves an empty worker script at `kWorkerPath`. If the request URL has a
// query, its unescaped value is sent as the `Permissions-Policy` response
// header.
std::unique_ptr<net::test_server::HttpResponse> HandleWorkerRequest(
    const net::test_server::HttpRequest& request) {
  const GURL url = request.GetURL();
  if (url.path() != kWorkerPath) {
    return nullptr;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/javascript");
  if (url.has_query()) {
    response->AddCustomHeader("Permissions-Policy",
                              base::UnescapeBinaryURLComponent(url.query()));
  }
  return response;
}

class WorkerCreatedObserver : public SharedWorkerService::Observer {
 public:
  explicit WorkerCreatedObserver(SharedWorkerService* shared_worker_service) {
    observation_.Observe(shared_worker_service);
  }

  const std::optional<blink::SharedWorkerToken>& worker_token() const {
    return worker_token_;
  }

  // SharedWorkerService::Observer:
  void OnWorkerCreated(const blink::SharedWorkerToken& token,
                       ChildProcessId,
                       const url::Origin&,
                       const base::UnguessableToken&) override {
    worker_token_ = token;
  }
  void OnBeforeWorkerDestroyed(const blink::SharedWorkerToken&) override {}
  void OnClientAdded(const blink::SharedWorkerToken&,
                     GlobalRenderFrameHostId) override {}
  void OnClientRemoved(const blink::SharedWorkerToken&,
                       GlobalRenderFrameHostId) override {}

 private:
  std::optional<blink::SharedWorkerToken> worker_token_;
  base::ScopedObservation<SharedWorkerService, SharedWorkerService::Observer>
      observation_{this};
};

}  // namespace

class SharedWorkerPermissionsPolicyBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    // Enables PermissionsPolicyForSharedWorkers both in the browser and in the
    // renderer.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleWorkerRequest));
    https_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(NavigateToURL(shell(), https_server_.GetURL("/title1.html")));
    worker_created_observer_ = std::make_unique<WorkerCreatedObserver>(
        shell()
            ->web_contents()
            ->GetBrowserContext()
            ->GetDefaultStoragePartition()
            ->GetSharedWorkerService());
  }

  void TearDownOnMainThread() override {
    worker_created_observer_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

  // Starts a shared worker whose script is served with `header` as the
  // Permissions-Policy header (or none, if `header` is empty) and that is
  // created with `allow` as the `allow` option.
  void StartWorker(const std::string& header, const std::string& allow) {
    std::string url = kWorkerPath;
    if (!header.empty()) {
      url += "?" + base::EscapeQueryParamValue(header, /*use_plus=*/false);
    }
    EXPECT_TRUE(ExecJs(
        shell(), JsReplace("new SharedWorker($1, {allow: $2});", url, allow)));
    ASSERT_TRUE(base::test::RunUntil([&]() {
      SharedWorkerHost* host = GetSharedWorkerHost();
      return host && host->GetPermissionsPolicy() != nullptr;
    }));
  }

  SharedWorkerHost* GetSharedWorkerHost() {
    if (!worker_created_observer_->worker_token().has_value()) {
      return nullptr;
    }
    auto* service =
        static_cast<SharedWorkerServiceImpl*>(shell()
                                                  ->web_contents()
                                                  ->GetBrowserContext()
                                                  ->GetDefaultStoragePartition()
                                                  ->GetSharedWorkerService());
    return service->GetSharedWorkerHostFromToken(
        *worker_created_observer_->worker_token());
  }

  // Returns the list of features allowed in the shared worker, as reported by
  // the browser-side `SharedWorkerHost`.
  std::vector<PermissionsPolicyFeature> GetAllowedFeaturesInWorker() {
    SharedWorkerHost* host = GetSharedWorkerHost();
    CHECK(host);
    CHECK(host->GetPermissionsPolicy());
    std::vector<PermissionsPolicyFeature> allowed_features;
    for (const auto& [feature, default_value] :
         host->GetPermissionsPolicy()->GetFeatureList()) {
      if (host->IsFeatureEnabled(feature)) {
        allowed_features.push_back(feature);
      }
    }
    return allowed_features;
  }

 private:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<WorkerCreatedObserver> worker_created_observer_;
};

IN_PROC_BROWSER_TEST_F(SharedWorkerPermissionsPolicyBrowserTest,
                       NoHeaderAndNoAllowOption) {
  StartWorker("", "");
  EXPECT_THAT(GetAllowedFeaturesInWorker(), IsEmpty());
  EXPECT_FALSE(GetSharedWorkerHost()->IsFeatureEnabled(
      PermissionsPolicyFeature::kSerial));
}

IN_PROC_BROWSER_TEST_F(SharedWorkerPermissionsPolicyBrowserTest, NoHeader) {
  StartWorker("", "serial; usb");
  EXPECT_THAT(GetAllowedFeaturesInWorker(),
              UnorderedElementsAre(PermissionsPolicyFeature::kSerial,
                                   PermissionsPolicyFeature::kUsb));
}

IN_PROC_BROWSER_TEST_F(SharedWorkerPermissionsPolicyBrowserTest,
                       HeaderRestrictsDelegatedFeatures) {
  StartWorker("serial=()", "serial; usb");
  EXPECT_THAT(GetAllowedFeaturesInWorker(),
              UnorderedElementsAre(PermissionsPolicyFeature::kUsb));
  EXPECT_FALSE(GetSharedWorkerHost()->IsFeatureEnabled(
      PermissionsPolicyFeature::kSerial));
  EXPECT_TRUE(
      GetSharedWorkerHost()->IsFeatureEnabled(PermissionsPolicyFeature::kUsb));
}

IN_PROC_BROWSER_TEST_F(SharedWorkerPermissionsPolicyBrowserTest,
                       HeaderCannotEnableFeaturesThatWereNotDelegated) {
  StartWorker("usb=(self)", "serial");
  EXPECT_THAT(GetAllowedFeaturesInWorker(),
              UnorderedElementsAre(PermissionsPolicyFeature::kSerial));
  EXPECT_FALSE(
      GetSharedWorkerHost()->IsFeatureEnabled(PermissionsPolicyFeature::kUsb));
}

IN_PROC_BROWSER_TEST_F(SharedWorkerPermissionsPolicyBrowserTest,
                       UnrecognizedFeatureIsIgnored) {
  StartWorker("serial=(), unknown-feature=()", "serial; usb");
  // The recognized part of the header is still applied.
  EXPECT_THAT(GetAllowedFeaturesInWorker(),
              UnorderedElementsAre(PermissionsPolicyFeature::kUsb));
}

IN_PROC_BROWSER_TEST_F(SharedWorkerPermissionsPolicyBrowserTest,
                       InvalidHeaderIsIgnored) {
  StartWorker("serial=(", "serial; usb");
  EXPECT_THAT(GetAllowedFeaturesInWorker(),
              UnorderedElementsAre(PermissionsPolicyFeature::kSerial,
                                   PermissionsPolicyFeature::kUsb));
}

}  // namespace content
