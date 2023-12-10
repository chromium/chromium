// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/default_construct_unittest.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::test::default_construct {

class TestInterface : public mojom::TestInterface {
 public:
  explicit TestInterface(mojo::PendingReceiver<mojom::TestInterface> receiver)
      : receiver_(this, std::move(receiver)) {}

  void TestMethod(const TestStruct& in, TestMethodCallback callback) override {
    std::move(callback).Run(in);
  }

  void TestMethodWithMap(const base::flat_map<uint8_t, TestStruct>& in,
                         uint8_t idx,
                         TestMethodWithMapCallback callback) override {
    std::move(callback).Run(in.at(idx));
  }

  void TestMethodWithArray(const std::vector<TestStruct>& in,
                           uint8_t idx,
                           TestMethodWithArrayCallback callback) override {
    std::move(callback).Run(in.at(idx));
  }

  void TestMethodWithFixedArray(
      const std::vector<TestStruct>& in,
      uint8_t idx,
      TestMethodWithFixedArrayCallback callback) override {
    std::move(callback).Run(in.at(idx));
  }

 private:
  mojo::Receiver<mojom::TestInterface> receiver_;
};

class DefaultConstructTest : public ::testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(DefaultConstructTest, Echo) {
  mojo::Remote<mojom::TestInterface> remote;
  TestInterface instance(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  remote->TestMethod(TestStruct(42),
                     base::BindLambdaForTesting([&](const TestStruct& out) {
                       EXPECT_EQ(out.value(), 42);
                       run_loop.Quit();
                     }));
  run_loop.Run();
}

TEST_F(DefaultConstructTest, Map) {
  mojo::Remote<mojom::TestInterface> remote;
  TestInterface instance(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  base::flat_map<uint8_t, TestStruct> test_map = {{1u, TestStruct(42)}};

  remote->TestMethodWithMap(
      std::move(test_map), 1u,
      base::BindLambdaForTesting([&](const TestStruct& out) {
        EXPECT_EQ(out.value(), 42);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(DefaultConstructTest, Array) {
  mojo::Remote<mojom::TestInterface> remote;
  TestInterface instance(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  std::vector<TestStruct> test_vec = {TestStruct(42), TestStruct(43)};

  remote->TestMethodWithArray(
      std::move(test_vec), 1u,
      base::BindLambdaForTesting([&](const TestStruct& out) {
        EXPECT_EQ(out.value(), 43);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(DefaultConstructTest, FixedArray) {
  mojo::Remote<mojom::TestInterface> remote;
  TestInterface instance(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  std::vector<TestStruct> test_vec = {TestStruct(42), TestStruct(43)};

  remote->TestMethodWithFixedArray(
      std::move(test_vec), 0u,
      base::BindLambdaForTesting([&](const TestStruct& out) {
        EXPECT_EQ(out.value(), 42);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Ensures that a non-typemapped type with a field typemapped to a type without
// a public default constructor initializes that field using
// `mojo::internal::DefaultConstructTraits::Create()` (crbug.com/1385587). Note
// that the generated Mojo code wouldn't even compile without the accompanying
// fix, so this test just covers the runtime behavior.
TEST_F(DefaultConstructTest, TypeWithPrivatelyDefaultConstructibleField) {
  mojom::TestStructContainer container;
  EXPECT_EQ(container.test_struct.value(), 0);
}

TEST(DefaultConstructOptionalTest, InitializedToNullopt) {
  auto container = mojom::OptionalTestStructContainer::New();
  EXPECT_EQ(std::nullopt, container->test_struct);
}

}  // namespace mojo::test::default_construct
