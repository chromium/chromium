// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/cryptauth_cmtg_device_key_provider.h"

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {

class CryptauthCmtgDeviceKeyProviderTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(CryptauthCmtgDeviceKeyProviderTest, GetDeviceKeysSuccess) {
  base::HistogramTester histogram_tester;
  CryptauthCmtgDeviceKeyProvider provider;
  base::test::TestFuture<base::expected<std::vector<std::vector<uint8_t>>,
                                        CmtgDeviceKeyProvider::Error>>
      future;

  // TODO(crbug.com/485888879): Update tests to verify real network responses
  // when Cryptauth network service is implemented.
  auto request = provider.GetDeviceKeys(future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->size(), 1u);
  EXPECT_EQ(future.Get()->front().size(), 32u);

  histogram_tester.ExpectUniqueSample("WebAuthentication.CmtgDeviceKeys.Result",
                                      CmtgDeviceKeysResult::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "WebAuthentication.CmtgDeviceKeys.RequestDuration", 1);
}

TEST_F(CryptauthCmtgDeviceKeyProviderTest, CancelsWhenRequestDestroyed) {
  base::HistogramTester histogram_tester;
  CryptauthCmtgDeviceKeyProvider provider;
  {
    auto request = provider.GetDeviceKeys(
        base::BindOnce([](base::expected<std::vector<std::vector<uint8_t>>,
                                         CmtgDeviceKeyProvider::Error> res) {
          FAIL() << "Callback should have been cancelled.";
        }));
    // Destroy request immediately before task runs.
  }
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectTotalCount("WebAuthentication.CmtgDeviceKeys.Result",
                                    0);
  histogram_tester.ExpectTotalCount(
      "WebAuthentication.CmtgDeviceKeys.RequestDuration", 0);
}

}  // namespace webauthn
