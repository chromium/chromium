// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/interfaces/bindings/tests/sample_import.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class ProviderImpl : public sample::Provider {
 public:
  explicit ProviderImpl(PendingReceiver<sample::Provider> receiver)
      : receiver_(this, std::move(receiver)) {}

  void EchoString(const std::string& a, EchoStringCallback callback) override {
    std::move(callback).Run(a);
  }

  void EchoStrings(const std::string& a,
                   const std::string& b,
                   EchoStringsCallback callback) override {
    std::move(callback).Run(a, b);
  }

  void EchoMessagePipeHandle(ScopedMessagePipeHandle a,
                             EchoMessagePipeHandleCallback callback) override {
    std::move(callback).Run(std::move(a));
  }

  void EchoEnum(sample::Enum a, EchoEnumCallback callback) override {
    std::move(callback).Run(a);
  }

  void EchoInt(int32_t a, EchoIntCallback callback) override {
    std::move(callback).Run(a);
  }

 private:
  Receiver<sample::Provider> receiver_;
};

using RequestResponseTest = BindingsTestBase;

TEST_P(RequestResponseTest, EchoString) {
  Remote<sample::Provider> provider;
  ProviderImpl provider_impl(provider.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  constexpr const char kTestMessage[] = "hello";
  provider->EchoString(kTestMessage, base::BindLambdaForTesting(
                                         [&](const std::string& response) {
                                           EXPECT_EQ(kTestMessage, response);
                                           run_loop.Quit();
                                         }));
  run_loop.Run();
}

TEST_P(RequestResponseTest, EchoStrings) {
  Remote<sample::Provider> provider;
  ProviderImpl provider_impl(provider.BindNewPipeAndPassReceiver());

  std::string buf;
  base::RunLoop run_loop;
  constexpr const char kTestMessage1[] = "hello";
  constexpr const char kTestMessage2[] = "hello";
  provider->EchoStrings(
      kTestMessage1, kTestMessage2,
      base::BindLambdaForTesting(
          [&](const std::string& response1, const std::string& response2) {
            EXPECT_EQ(kTestMessage1, response1);
            EXPECT_EQ(kTestMessage2, response2);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_P(RequestResponseTest, EchoMessagePipeHandle) {
  Remote<sample::Provider> provider;
  ProviderImpl provider_impl(provider.BindNewPipeAndPassReceiver());

  MessagePipe pipe;
  base::RunLoop run_loop;
  constexpr const char kTestMessage[] = "hello";
  provider->EchoMessagePipeHandle(
      std::move(pipe.handle1),
      base::BindLambdaForTesting([&](ScopedMessagePipeHandle handle) {
        WriteTextMessage(handle.get(), kTestMessage);

        std::string value;
        ReadTextMessage(pipe.handle0.get(), &value);
        EXPECT_EQ(kTestMessage, value);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_P(RequestResponseTest, EchoEnum) {
  Remote<sample::Provider> provider;
  ProviderImpl provider_impl(provider.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  provider->EchoEnum(sample::Enum::VALUE,
                     base::BindLambdaForTesting([&](sample::Enum value) {
                       EXPECT_EQ(sample::Enum::VALUE, value);
                       run_loop.Quit();
                     }));
  run_loop.Run();
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(RequestResponseTest);

}  // namespace
}  // namespace test
}  // namespace mojo
