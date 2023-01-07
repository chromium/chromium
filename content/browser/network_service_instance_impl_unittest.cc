// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/no_destructor.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// A leaky class that overrides Content Browser Client to say that shutdown has
// started.
class EarlyShutdownTestContentBrowserClient : public TestContentBrowserClient {
 public:
  static EarlyShutdownTestContentBrowserClient* GetInstance() {
    static base::NoDestructor<EarlyShutdownTestContentBrowserClient> instance;
    return instance.get();
  }

 private:
  bool IsShuttingDown() override { return true; }
};

}  // namespace

// This test exists as a regression test for https://crbug.com/1369808.
class NetworkServiceShutdownRaceTest : public testing::Test {
 public:
  NetworkServiceShutdownRaceTest() = default;

  NetworkServiceShutdownRaceTest(const NetworkServiceShutdownRaceTest&) =
      delete;
  NetworkServiceShutdownRaceTest& operator=(
      const NetworkServiceShutdownRaceTest&) = delete;

 protected:
  // Trigger a NetworkContext creation using default parameters. This posts a
  // background thread with a reply to the UI thread. This reply will race
  // shutdown.
  void CreateNetworkContext() {
    mojo::Remote<network::mojom::NetworkContext> network_context;
    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    context_params->cert_verifier_params = GetCertVerifierParams(
        cert_verifier::mojom::CertVerifierCreationParams::New());
    CreateNetworkContextInNetworkService(
        network_context.BindNewPipeAndPassReceiver(),
        std::move(context_params));
  }

 private:
  BrowserTaskEnvironment task_environment_;
};

// This should not crash.
TEST_F(NetworkServiceShutdownRaceTest, CreateNetworkContextDuringShutdown) {
  // Set browser as shutting down. Note: this never gets reset back to the old
  // client and will intentionally leak, because the pending UI tasks that cause
  // issue 1369808 are run after the test fixture has been completely torn down,
  // and require IsShuttingDown() to still return true at that point to
  // reproduce the bug.
  std::ignore = SetBrowserClientForTesting(
      EarlyShutdownTestContentBrowserClient::GetInstance());
  // Trigger the network context creation.
  CreateNetworkContext();
}

}  // namespace content
