// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_service_instance_impl.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
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

TEST(NetworkServiceInstanceImplParseCommandLineTest,
     ParseNetLogMaximumFileNoSwitch) {
  base::CommandLine command_line{base::CommandLine::NO_PROGRAM};
  EXPECT_EQ(GetNetLogMaximumFileSizeFromCommandLineForTesting(command_line),
            std::numeric_limits<uint64_t>::max());
}

TEST(NetworkServiceInstanceImplParseCommandLineTest,
     ParseNetLogMaximumFileSizeZero) {
  base::CommandLine command_line{base::CommandLine::NO_PROGRAM};
  command_line.AppendSwitchASCII("net-log-max-size-mb", "0");
  EXPECT_EQ(GetNetLogMaximumFileSizeFromCommandLineForTesting(command_line),
            0u);
}

TEST(NetworkServiceInstanceImplParseCommandLineTest,
     ParseNetLogMaximumFileSizeSmall) {
  base::CommandLine command_line{base::CommandLine::NO_PROGRAM};
  command_line.AppendSwitchASCII("net-log-max-size-mb", "42");
  EXPECT_EQ(GetNetLogMaximumFileSizeFromCommandLineForTesting(command_line),
            42u * 1024 * 1024);
}

// Regression test for <https://crbug.com/352496169>.
TEST(NetworkServiceInstanceImplParseCommandLineTest,
     ParseNetLogMaximumFileSizeLargeButInRange) {
  constexpr uint64_t kTestCases[] = {
      1 << 12,
      std::numeric_limits<uint32_t>::max(),
  };

  for (uint64_t test_case : kTestCases) {
    base::CommandLine command_line{base::CommandLine::NO_PROGRAM};
    command_line.AppendSwitchASCII("net-log-max-size-mb",
                                   base::NumberToString(test_case));
    EXPECT_EQ(GetNetLogMaximumFileSizeFromCommandLineForTesting(command_line),
              test_case * 1024 * 1024);
  }
}

TEST(NetworkServiceInstanceImplParseCommandLineTest,
     ParseNetLogMaximumFileSizeTooLarge) {
  constexpr uint64_t kTooLarge =
      uint64_t{std::numeric_limits<uint32_t>::max()} + 1;
  base::CommandLine command_line{base::CommandLine::NO_PROGRAM};
  command_line.AppendSwitchASCII("net-log-max-size-mb",
                                 base::NumberToString(kTooLarge));
  EXPECT_EQ(GetNetLogMaximumFileSizeFromCommandLineForTesting(command_line),
            std::numeric_limits<uint64_t>::max());
}

TEST(NetworkServiceInstanceImplParseCommandLineTest,
     ParseNetLogMaximumFileSizeNotNumeric) {
  constexpr std::string_view kTestCases[] = {"",    " ",     "-", "-0",
                                             "-42", "hello", "\a"};
  for (std::string_view test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Test case: " << test_case);
    base::CommandLine command_line{base::CommandLine::NO_PROGRAM};
    command_line.AppendSwitchASCII("net-log-max-size-mb", test_case);
    EXPECT_EQ(GetNetLogMaximumFileSizeFromCommandLineForTesting(command_line),
              std::numeric_limits<uint64_t>::max());
  }
}

}  // namespace content
