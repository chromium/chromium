// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/tests/result_response.test-mojom.h"
#include "result_response_unittest_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::test {
namespace {

class InterfaceImpl : public mojom::TestResultInterface {
 public:
  explicit InterfaceImpl(
      mojo::PendingReceiver<mojom::TestResultInterface> receiver)
      : receiver_(this, std::move(receiver)) {}
  InterfaceImpl(const InterfaceImpl&) = delete;
  InterfaceImpl& operator=(const InterfaceImpl&) = delete;
  ~InterfaceImpl() override = default;

  // mojom::TestResultInterface
  void TestSuccess(int32_t value, TestSuccessCallback cb) override {
    std::move(cb).Run(base::ok(value));
  }

  void TestFailure(const std::string& value, TestFailureCallback cb) override {
    std::move(cb).Run(base::unexpected(value));
  }

 private:
  mojo::Receiver<mojom::TestResultInterface> receiver_;
};

class TraitInterfaceImpl : public mojom::TestResultInterfaceWithTrait {
 public:
  explicit TraitInterfaceImpl(
      mojo::PendingReceiver<mojom::TestResultInterfaceWithTrait> receiver)
      : receiver_(this, std::move(receiver)) {}
  TraitInterfaceImpl(const TraitInterfaceImpl&) = delete;
  TraitInterfaceImpl& operator=(const TraitInterfaceImpl&) = delete;
  ~TraitInterfaceImpl() override = default;

  // mojom::TestResultTraitInterface
  void TestSuccess(TestSuccessCallback cb) override {
    std::move(cb).Run(base::ok(MappedResultValue{1}));
  }

  void TestFailure(TestFailureCallback cb) override {
    MappedResultError err;
    err.is_game_over_ = true;
    err.reason_ = "meltdown!";
    std::move(cb).Run(base::unexpected(err));
  }

 private:
  mojo::Receiver<mojom::TestResultInterfaceWithTrait> receiver_;
};

using ResultResponseTest = BindingsTestBase;

TEST_P(ResultResponseTest, TestResult) {
  mojo::Remote<mojom::TestResultInterface> remote;
  InterfaceImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  remote->TestSuccess(
      1, base::BindLambdaForTesting([&](base::expected<int32_t, bool> result) {
        ASSERT_EQ(1, result.value());
        loop.Quit();
      }));
  loop.Run();
}

TEST_P(ResultResponseTest, TestFailure) {
  mojo::Remote<mojom::TestResultInterface> remote;
  InterfaceImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  remote->TestFailure(
      "fail",
      base::BindLambdaForTesting([&](base::expected<bool, std::string> result) {
        ASSERT_EQ("fail", result.error());
        loop.Quit();
      }));
  loop.Run();
}

TEST_P(ResultResponseTest, TestSuccessTrait) {
  mojo::Remote<mojom::TestResultInterfaceWithTrait> remote;
  TraitInterfaceImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  remote->TestSuccess(base::BindLambdaForTesting(
      [&](base::expected<MappedResultValue, MappedResultError> result) {
        ASSERT_EQ(1, result.value().magic_value);
        loop.Quit();
      }));
  loop.Run();
}

TEST_P(ResultResponseTest, TestFailureTrait) {
  mojo::Remote<mojom::TestResultInterfaceWithTrait> remote;
  TraitInterfaceImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  remote->TestFailure(base::BindLambdaForTesting(
      [&](base::expected<MappedResultValue, MappedResultError> result) {
        ASSERT_TRUE(result.error().is_game_over_);
        ASSERT_EQ(result.error().reason_, "meltdown!");
        loop.Quit();
      }));
  loop.Run();
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(ResultResponseTest);

}  // namespace
}  // namespace mojo::test
