// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/local_trust_token_operation_delegate_impl.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/test/test_network_context_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

using ::testing::Field;
using ::testing::Pointee;

TEST(LocalTrustTokenOperationDelegateImpl, HandlesNullClient) {
  LocalTrustTokenOperationDelegateImpl delegate(base::BindRepeating(
      []() -> mojom::NetworkContextClient* { return nullptr; }));

  auto request = mojom::FulfillTrustTokenIssuanceRequest::New();

  base::MockOnceCallback<void(mojom::FulfillTrustTokenIssuanceAnswerPtr)>
      callback;

  EXPECT_CALL(callback,
              Run(Pointee(Field(
                  &mojom::FulfillTrustTokenIssuanceAnswer::status,
                  mojom::FulfillTrustTokenIssuanceAnswer::Status::kNotFound))));

  delegate.FulfillIssuance(std::move(request), callback.Get());
}

class RequestCapturingNetworkContextClient : public TestNetworkContextClient {
 public:
  explicit RequestCapturingNetworkContextClient(
      mojom::FulfillTrustTokenIssuanceRequestPtr& request_out)
      : request_out_(request_out) {}

  void OnTrustTokenIssuanceDivertedToSystem(
      mojom::FulfillTrustTokenIssuanceRequestPtr request,
      OnTrustTokenIssuanceDivertedToSystemCallback callback) override {
    request_out_ = std::move(request);
    std::move(callback).Run(mojom::FulfillTrustTokenIssuanceAnswer::New(
        mojom::FulfillTrustTokenIssuanceAnswer::Status::kOk,
        "Here's your answer"));
  }

 private:
  mojom::FulfillTrustTokenIssuanceRequestPtr& request_out_;
};

TEST(LocalTrustTokenOperationDelegateImpl, RelaysRequestAndAnswer) {
  base::test::TaskEnvironment task_environment;
  mojom::FulfillTrustTokenIssuanceRequestPtr relayed_request;
  auto client =
      std::make_unique<RequestCapturingNetworkContextClient>(relayed_request);
  base::RunLoop run_loop;

  LocalTrustTokenOperationDelegateImpl delegate(base::BindLambdaForTesting(
      [&client]() -> mojom::NetworkContextClient* { return client.get(); }));

  auto request = mojom::FulfillTrustTokenIssuanceRequest::New();
  request->request = "Please give me an answer";

  delegate.FulfillIssuance(
      std::move(request),
      base::BindLambdaForTesting(
          [&run_loop](mojom::FulfillTrustTokenIssuanceAnswerPtr answer) {
            run_loop.Quit();
            ASSERT_TRUE(answer);
            EXPECT_EQ(answer->status,
                      mojom::FulfillTrustTokenIssuanceAnswer::Status::kOk);
            EXPECT_EQ(answer->response, "Here's your answer");
          }));
  run_loop.Run();

  ASSERT_TRUE(relayed_request);
  EXPECT_EQ(relayed_request->request, "Please give me an answer");
}

}  // namespace

}  // namespace network
