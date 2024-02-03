// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/fuchsia_cdm_provisioning_fetcher_impl.h"

#include <fuchsia/media/drm/cpp/fidl.h>
#include <memory>

#include "base/fuchsia/mem_buffer_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

namespace drm = ::fuchsia::media::drm;

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::WithArgs;

// This is a mock for the Chromium media::ProvisionFetcher (and not Fuchsia's
// similarly named ProvisioningFetcher protocol).
class MockProvisionFetcher : public ProvisionFetcher {
 public:
  MockProvisionFetcher() = default;
  ~MockProvisionFetcher() override = default;

  MOCK_METHOD(void,
              Retrieve,
              (const GURL& default_url,
               const std::string& request_data,
               ResponseCB response_cb),
              (override));
};

constexpr char kTestDefaultUrl[] = "http://test_default_url.com/";
constexpr char kTestRequest[] = "test_request_message";
constexpr char kTestResponse[] = "test_response_message";

drm::ProvisioningRequest CreateProvisioningRequest(
    fidl::StringPtr default_provisioning_server_url,
    const std::string& message) {
  drm::ProvisioningRequest request;
  request.default_provisioning_server_url =
      std::move(default_provisioning_server_url);
  request.message = base::MemBufferFromString(message, "provisioning_request");
  return request;
}

class FuchsiaCdmProvisioningFetcherImplTest : public ::testing::Test {
 public:
  FuchsiaCdmProvisioningFetcherImplTest() = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
};

TEST_F(FuchsiaCdmProvisioningFetcherImplTest, Fetch) {
  FuchsiaCdmProvisioningFetcherImpl fetcher(base::BindLambdaForTesting([]() {
    auto mock_provision_fetcher = std::make_unique<MockProvisionFetcher>();
    EXPECT_CALL(*mock_provision_fetcher,
                Retrieve(Eq(kTestDefaultUrl), Eq(kTestRequest), _))
        .WillOnce(WithArgs<2>(
            Invoke([](ProvisionFetcher::ResponseCB response_callback) {
              std::move(response_callback).Run(true, kTestResponse);
            })));

    return std::unique_ptr<ProvisionFetcher>(std::move(mock_provision_fetcher));
  }));

  fetcher.Bind(base::MakeExpectedNotRunClosure(FROM_HERE));

  std::optional<std::string> response_message;
  fetcher.Fetch(CreateProvisioningRequest(kTestDefaultUrl, kTestRequest),
                [&](drm::ProvisioningResponse response) {
                  response_message =
                      base::StringFromMemBuffer(response.message);
                });
  ASSERT_TRUE(response_message.has_value());
  EXPECT_EQ(*response_message, kTestResponse);
}

TEST_F(FuchsiaCdmProvisioningFetcherImplTest, RetrieveFails) {
  FuchsiaCdmProvisioningFetcherImpl fetcher(base::BindLambdaForTesting([]() {
    auto mock_provision_fetcher = std::make_unique<MockProvisionFetcher>();
    EXPECT_CALL(*mock_provision_fetcher, Retrieve(_, _, _))
        .WillOnce(WithArgs<2>(
            Invoke([](ProvisionFetcher::ResponseCB response_callback) {
              std::move(response_callback).Run(false, "");
            })));

    return std::unique_ptr<ProvisionFetcher>(std::move(mock_provision_fetcher));
  }));

  fetcher.Bind(base::MakeExpectedNotRunClosure(FROM_HERE));

  std::optional<std::string> response_message;
  fetcher.Fetch(CreateProvisioningRequest(kTestDefaultUrl, kTestRequest),
                [&](drm::ProvisioningResponse response) {
                  response_message =
                      base::StringFromMemBuffer(response.message);
                });
  ASSERT_TRUE(response_message.has_value());
  EXPECT_TRUE(response_message->empty());
}

TEST_F(FuchsiaCdmProvisioningFetcherImplTest, NoDefaultProvisioningUrl) {
  FuchsiaCdmProvisioningFetcherImpl fetcher(base::BindLambdaForTesting([]() {
    auto mock_provision_fetcher = std::make_unique<MockProvisionFetcher>();
    EXPECT_CALL(*mock_provision_fetcher, Retrieve(_, _, _))
        .WillOnce(WithArgs<2>(
            Invoke([](ProvisionFetcher::ResponseCB response_callback) {
              std::move(response_callback).Run(true, kTestResponse);
            })));

    return std::unique_ptr<ProvisionFetcher>(std::move(mock_provision_fetcher));
  }));

  fetcher.Bind(base::MakeExpectedRunClosure(FROM_HERE));

  fetcher.Fetch(CreateProvisioningRequest({}, kTestRequest),
                [](drm::ProvisioningResponse response) { FAIL(); });
}

TEST_F(FuchsiaCdmProvisioningFetcherImplTest, MultipleRequestsFails) {
  FuchsiaCdmProvisioningFetcherImpl fetcher(base::BindLambdaForTesting([]() {
    auto mock_provision_fetcher = std::make_unique<MockProvisionFetcher>();
    EXPECT_CALL(*mock_provision_fetcher, Retrieve(_, _, _)).Times(1);

    return std::unique_ptr<ProvisionFetcher>(std::move(mock_provision_fetcher));
  }));

  fetcher.Bind(base::MakeExpectedRunClosure(FROM_HERE));

  fetcher.Fetch(CreateProvisioningRequest(kTestDefaultUrl, kTestRequest),
                [](drm::ProvisioningResponse) { FAIL(); });
  fetcher.Fetch(CreateProvisioningRequest(kTestDefaultUrl, kTestRequest),
                [](drm::ProvisioningResponse) { FAIL(); });
}

}  // namespace

}  // namespace media
