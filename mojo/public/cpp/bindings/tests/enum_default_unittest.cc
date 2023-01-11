// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/tests/enum_default_unittest.test-mojom-forward.h"
#include "mojo/public/cpp/bindings/tests/enum_default_unittest.test-mojom-shared.h"
#include "mojo/public/cpp/bindings/tests/enum_default_unittest.test-mojom.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace enum_default_unittest {

class EnumDefaultTest : public ::testing::Test {
  base::test::SingleThreadTaskEnvironment task_environment_;
};

class TestInterfaceImpl : public mojom::TestInterface {
 public:
  explicit TestInterfaceImpl(PendingReceiver<TestInterface> receiver)
      : receiver_(this, std::move(receiver)) {}

  void EchoWithDefault(mojom::ExtensibleEnumWithDefault in,
                       EchoWithDefaultCallback callback) override {
    EXPECT_EQ(mojom::ExtensibleEnumWithDefault::kFirst, in);
    std::move(callback).Run(static_cast<mojom::ExtensibleEnumWithDefault>(100));
  }

  void EchoWithoutDefault(mojom::ExtensibleEnumWithoutDefault in,
                          EchoWithoutDefaultCallback callback) override {
    EXPECT_EQ(static_cast<mojom::ExtensibleEnumWithoutDefault>(100), in);
    std::move(callback).Run(in);
  }

  void EchoStructWithDefault(mojom::StructWithExtensibleEnumWithDefaultPtr in,
                             EchoStructWithDefaultCallback callback) override {
    EXPECT_EQ(mojom::ExtensibleEnumWithDefault::kFirst, in->value);
    std::move(callback).Run(std::move(in));
  }

  void EchoStructWithoutDefault(
      mojom::StructWithExtensibleEnumWithoutDefaultPtr in,
      EchoStructWithoutDefaultCallback callback) override {
    EXPECT_EQ(static_cast<mojom::ExtensibleEnumWithoutDefault>(100), in->value);
    in->value = static_cast<mojom::ExtensibleEnumWithoutDefault>(100);
    std::move(callback).Run(std::move(in));
  }

  void EchoUnionWithDefault(mojom::UnionWithExtensibleEnumWithDefaultPtr in,
                            EchoUnionWithDefaultCallback callback) override {
    EXPECT_EQ(mojom::ExtensibleEnumWithDefault::kFirst, in->get_value());
    in->set_value(static_cast<mojom::ExtensibleEnumWithDefault>(100));
    std::move(callback).Run(std::move(in));
  }

  void EchoUnionWithoutDefault(
      mojom::UnionWithExtensibleEnumWithoutDefaultPtr in,
      EchoUnionWithoutDefaultCallback callback) override {
    EXPECT_EQ(static_cast<mojom::ExtensibleEnumWithoutDefault>(100),
              in->get_value());
    std::move(callback).Run(std::move(in));
  }

 private:
  Receiver<TestInterface> receiver_;
};

TEST_F(EnumDefaultTest, MethodBareParamWithDefault) {
  Remote<mojom::TestInterface> remote;
  TestInterfaceImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  remote->EchoWithDefault(
      static_cast<mojom::ExtensibleEnumWithDefault>(100),
      base::BindLambdaForTesting([&](mojom::ExtensibleEnumWithDefault result) {
        EXPECT_EQ(mojom::ExtensibleEnumWithDefault::kFirst, result);
        run_loop.Quit();
      }));
}

TEST_F(EnumDefaultTest, MethodBareParamWithoutDefault) {
  Remote<mojom::TestInterface> remote;
  TestInterfaceImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  remote->EchoWithoutDefault(
      static_cast<mojom::ExtensibleEnumWithoutDefault>(100),
      base::BindLambdaForTesting(
          [&](mojom::ExtensibleEnumWithoutDefault result) {
            EXPECT_EQ(static_cast<mojom::ExtensibleEnumWithoutDefault>(100),
                      result);
            run_loop.Quit();
          }));
}

TEST_F(EnumDefaultTest, MethodStructParamWithDefault) {
  Remote<mojom::TestInterface> remote;
  TestInterfaceImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  auto value = mojom::StructWithExtensibleEnumWithDefault::New(
      static_cast<mojom::ExtensibleEnumWithDefault>(100));
  remote->EchoStructWithDefault(
      std::move(value),
      base::BindLambdaForTesting(
          [&](mojom::StructWithExtensibleEnumWithDefaultPtr result) {
            EXPECT_EQ(mojom::ExtensibleEnumWithDefault::kFirst, result->value);
            run_loop.Quit();
          }));
}

TEST_F(EnumDefaultTest, MethodStructParamWithoutDefault) {
  Remote<mojom::TestInterface> remote;
  TestInterfaceImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  auto value = mojom::StructWithExtensibleEnumWithoutDefault::New(
      static_cast<mojom::ExtensibleEnumWithoutDefault>(100));
  remote->EchoStructWithoutDefault(
      std::move(value),
      base::BindLambdaForTesting(
          [&](mojom::StructWithExtensibleEnumWithoutDefaultPtr result) {
            EXPECT_EQ(static_cast<mojom::ExtensibleEnumWithoutDefault>(100),
                      result->value);
            run_loop.Quit();
          }));
}

TEST_F(EnumDefaultTest, MethodUnionParamWithDefault) {
  Remote<mojom::TestInterface> remote;
  TestInterfaceImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  auto value = mojom::UnionWithExtensibleEnumWithDefault::NewValue(
      static_cast<mojom::ExtensibleEnumWithDefault>(100));
  remote->EchoUnionWithDefault(
      std::move(value),
      base::BindLambdaForTesting(
          [&](mojom::UnionWithExtensibleEnumWithDefaultPtr result) {
            EXPECT_EQ(mojom::ExtensibleEnumWithDefault::kFirst,
                      result->get_value());
            run_loop.Quit();
          }));
}

TEST_F(EnumDefaultTest, MethodUnionParamWithoutDefault) {
  Remote<mojom::TestInterface> remote;
  TestInterfaceImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  auto value = mojom::UnionWithExtensibleEnumWithoutDefault::NewValue(
      static_cast<mojom::ExtensibleEnumWithoutDefault>(100));
  remote->EchoUnionWithoutDefault(
      std::move(value),
      base::BindLambdaForTesting(
          [&](mojom::UnionWithExtensibleEnumWithoutDefaultPtr result) {
            EXPECT_EQ(static_cast<mojom::ExtensibleEnumWithoutDefault>(100),
                      result->get_value());
            run_loop.Quit();
          }));
}

TEST_F(EnumDefaultTest, DefaultValueDoesNotAffectInitializer) {
  {
    // With no initializer specified, an enum field should be zero-initialized,
    // even if zero is not a valid enumerator value. The default enumerator
    // value specified in the enum should also not affect this.
    auto s = mojom::StructWithoutInitializer::New();
    EXPECT_EQ(static_cast<mojom::EnumWithoutZeroValue>(0), s->value);
  }

  {
    // With an initializer specified, an enum field should match the initializer
    // value. The default enumerator value specified in the enum should also not
    // affect this.
    auto s = mojom::StructWithInitializer::New();
    EXPECT_EQ(mojom::EnumWithoutZeroValue::kSecond, s->value);
  }
}

}  // namespace enum_default_unittest
}  // namespace test
}  // namespace mojo
