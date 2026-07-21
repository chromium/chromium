// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_websocket_client.h"

#include <optional>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace device::enclave {
namespace {

class EnclaveWebSocketClientTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(EnclaveWebSocketClientTest, OnDropChannelLogsHistogram) {
  base::HistogramTester histogram_tester;
  network::TestNetworkContext network_context;

  EnclaveWebSocketClient client(
      GURL("https://example.com"), "access_token", std::nullopt,
      base::BindRepeating(
          [](network::mojom::NetworkContext* context) { return context; },
          &network_context),
      base::DoNothing());

  std::vector<uint8_t> data = {1, 2, 3};
  client.Write(data);

  client.OnDropChannel(/*was_clean=*/true, /*code=*/1000,
                       /*reason=*/"Normal Closure");

  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.Enclave.WebSocketCloseCode", 1000, 1);
}

TEST_F(EnclaveWebSocketClientTest, OnFailureLogsHistogram) {
  base::HistogramTester histogram_tester;
  network::TestNetworkContext network_context;

  EnclaveWebSocketClient client(
      GURL("https://example.com"), "access_token", std::nullopt,
      base::BindRepeating(
          [](network::mojom::NetworkContext* context) { return context; },
          &network_context),
      base::DoNothing());

  client.OnFailure("Failed to connect", -105, 0);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.Enclave.HttpStatusOrNetError", -105, 1);

  client.OnFailure("Forbidden", 0, 403);
  histogram_tester.ExpectBucketCount(
      "WebAuthentication.Enclave.HttpStatusOrNetError", 403, 1);
}

}  // namespace
}  // namespace device::enclave
