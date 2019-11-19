// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gcd_rest_client.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class GcdRestClientTest : public testing::Test {
 public:
  GcdRestClientTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        default_token_getter_(OAuthTokenGetter::SUCCESS,
                              "<fake_user_email>",
                              "<fake_access_token>") {}

  void OnRequestComplete(GcdRestClient::Result result) {
    ++counter_;
    last_result_ = result;
  }

  std::unique_ptr<base::DictionaryValue> MakePatchDetails(int id) {
    std::unique_ptr<base::DictionaryValue> patch_details(
        new base::DictionaryValue);
    patch_details->SetInteger("id", id);
    return patch_details;
  }

  void CreateClient(OAuthTokenGetter* token_getter = nullptr) {
    if (!token_getter) {
      token_getter = &default_token_getter_;
    }
    client_.reset(new GcdRestClient("http://gcd_base_url", "<gcd_device_id>",
                                    test_shared_url_loader_factory_,
                                    token_getter));
    client_->SetClockForTest(&clock_);
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  FakeOAuthTokenGetter default_token_getter_;
  base::SimpleTestClock clock_;
  std::unique_ptr<GcdRestClient> client_;
  int counter_ = 0;
  GcdRestClient::Result last_result_ = GcdRestClient::OTHER_ERROR;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(GcdRestClientTest, NetworkErrorGettingToken) {
  FakeOAuthTokenGetter token_getter(OAuthTokenGetter::NETWORK_ERROR, "", "");
  CreateClient(&token_getter);

  client_->PatchState(MakePatchDetails(0),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, counter_);
  EXPECT_EQ(GcdRestClient::NETWORK_ERROR, last_result_);
}

TEST_F(GcdRestClientTest, AuthErrorGettingToken) {
  FakeOAuthTokenGetter token_getter(OAuthTokenGetter::AUTH_ERROR, "", "");
  CreateClient(&token_getter);

  client_->PatchState(MakePatchDetails(0),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, counter_);
  EXPECT_EQ(GcdRestClient::OTHER_ERROR, last_result_);
}

TEST_F(GcdRestClientTest, NetworkErrorOnPost) {
  CreateClient();

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        test_url_loader_factory_.AddResponse(
            request.url, network::mojom::URLResponseHead::New(), std::string(),
            network::URLLoaderCompletionStatus(net::ERR_FAILED));
      }));

  client_->PatchState(MakePatchDetails(0),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, counter_);
  EXPECT_EQ(GcdRestClient::NETWORK_ERROR, last_result_);
}

TEST_F(GcdRestClientTest, OtherErrorOnPost) {
  CreateClient();

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        test_url_loader_factory_.AddResponse(request.url.spec(), std::string(),
                                             net::HTTP_INTERNAL_SERVER_ERROR);
      }));

  client_->PatchState(MakePatchDetails(0),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, counter_);
  EXPECT_EQ(GcdRestClient::OTHER_ERROR, last_result_);
}

TEST_F(GcdRestClientTest, NoSuchHost) {
  CreateClient();

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        test_url_loader_factory_.AddResponse(request.url.spec(), std::string(),
                                             net::HTTP_NOT_FOUND);
      }));

  client_->PatchState(MakePatchDetails(0),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, counter_);
  EXPECT_EQ(GcdRestClient::NO_SUCH_HOST, last_result_);
}

TEST_F(GcdRestClientTest, Succeed) {
  CreateClient();

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ("http://gcd_base_url/devices/%3Cgcd_device_id%3E/patchState",
                  request.url.spec());
        EXPECT_EQ(
            "{\"patches\":[{\"patch\":{\"id\":0},\"timeMs\":0.0}],"
            "\"requestTimeMs\":0.0}",
            network::GetUploadData(request));
        DCHECK(
            request.headers.HasHeader(net::HttpRequestHeaders::kContentType));
        std::string upload_content_type;
        request.headers.GetHeader(net::HttpRequestHeaders::kContentType,
                                  &upload_content_type);
        EXPECT_EQ("application/json", upload_content_type);

        test_url_loader_factory_.AddResponse(request.url.spec(), std::string(),
                                             net::HTTP_OK);
      }));

  client_->PatchState(MakePatchDetails(0),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, counter_);
  EXPECT_EQ(GcdRestClient::SUCCESS, last_result_);
}

}  // namespace remoting
