// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "sandbox/mac/mojom/seatbelt_extension_token_mojom_traits.h"
#include "sandbox/mac/mojom/traits_test_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace {

class StructTraitsTest : public testing::Test,
                         public sandbox::mac::mojom::TraitsTestService {
 public:
  StructTraitsTest() : receiver_(this, remote_.BindNewPipeAndPassReceiver()) {}

  sandbox::mac::mojom::TraitsTestService* interface() { return remote_.get(); }

 private:
  // TraitsTestService:
  void EchoSeatbeltExtensionToken(
      sandbox::SeatbeltExtensionToken token,
      EchoSeatbeltExtensionTokenCallback callback) override {
    std::move(callback).Run(std::move(token));
  }

  base::test::TaskEnvironment task_environment_;

  mojo::Remote<sandbox::mac::mojom::TraitsTestService> remote_;
  mojo::Receiver<sandbox::mac::mojom::TraitsTestService> receiver_;
};

TEST_F(StructTraitsTest, SeatbeltExtensionToken) {
  auto input = sandbox::SeatbeltExtensionToken::CreateForTesting("hello world");
  sandbox::SeatbeltExtensionToken output;

  interface()->EchoSeatbeltExtensionToken(std::move(input), &output);
  EXPECT_EQ("hello world", output.token());
  EXPECT_TRUE(input.token().empty());
}

}  // namespace
}  // namespace sandbox
