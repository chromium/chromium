// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/interfaces/bindings/tests/struct_with_traits.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class LazySerializationTest : public testing::Test {
 public:
  LazySerializationTest() = default;
  ~LazySerializationTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(LazySerializationTest);
};

class TestUnserializedStructImpl : public test::TestUnserializedStruct {
 public:
  explicit TestUnserializedStructImpl(
      PendingReceiver<test::TestUnserializedStruct> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~TestUnserializedStructImpl() override = default;

  // test::TestUnserializedStruct:
  void PassUnserializedStruct(
      const test::StructWithUnreachableTraitsImpl& s,
      PassUnserializedStructCallback callback) override {
    std::move(callback).Run(s);
  }

 private:
  Receiver<test::TestUnserializedStruct> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestUnserializedStructImpl);
};

class ForceSerializeTesterImpl : public test::ForceSerializeTester {
 public:
  ForceSerializeTesterImpl(PendingReceiver<test::ForceSerializeTester> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~ForceSerializeTesterImpl() override = default;

  // test::ForceSerializeTester:
  void SendForceSerializedStruct(
      const test::StructForceSerializeImpl& s,
      SendForceSerializedStructCallback callback) override {
    std::move(callback).Run(s);
  }

  void SendNestedForceSerializedStruct(
      const test::StructNestedForceSerializeImpl& s,
      SendNestedForceSerializedStructCallback callback) override {
    std::move(callback).Run(s);
  }

 private:
  Receiver<test::ForceSerializeTester> receiver_;

  DISALLOW_COPY_AND_ASSIGN(ForceSerializeTesterImpl);
};

TEST_F(LazySerializationTest, NeverSerialize) {
  // Basic sanity check to ensure that no messages are serialized by default in
  // environments where lazy serialization is supported, on an interface which
  // supports lazy serialization, and where both ends of the interface are in
  // the same process.

  Remote<test::TestUnserializedStruct> remote;
  TestUnserializedStructImpl impl(remote.BindNewPipeAndPassReceiver());

  const int32_t kTestMagicNumber = 42;

  test::StructWithUnreachableTraitsImpl data;
  EXPECT_EQ(0, data.magic_number);
  data.magic_number = kTestMagicNumber;

  // Send our data over the pipe and wait for it to come back. The value should
  // be preserved. We know the data was never serialized because the
  // StructTraits for this type will DCHECK if executed in any capacity.
  base::RunLoop loop;
  remote->PassUnserializedStruct(
      data, base::BindLambdaForTesting(
                [&](const test::StructWithUnreachableTraitsImpl& passed) {
                  EXPECT_EQ(kTestMagicNumber, passed.magic_number);
                  loop.Quit();
                }));
  loop.Run();
}

TEST_F(LazySerializationTest, ForceSerialize) {
  // Verifies that the [force_serialize] attribute works as intended: i.e., even
  // with lazy serialization enabled, messages which carry a force-serialized
  // type will always serialize at call time.

  Remote<test::ForceSerializeTester> tester;
  ForceSerializeTesterImpl impl(tester.BindNewPipeAndPassReceiver());

  constexpr int32_t kTestValue = 42;

  base::RunLoop loop;
  test::StructForceSerializeImpl in;
  in.set_value(kTestValue);
  EXPECT_FALSE(in.was_serialized());
  EXPECT_FALSE(in.was_deserialized());
  tester->SendForceSerializedStruct(
      in, base::BindLambdaForTesting(
              [&](const test::StructForceSerializeImpl& passed) {
                EXPECT_EQ(kTestValue, passed.value());
                EXPECT_TRUE(passed.was_deserialized());
                EXPECT_FALSE(passed.was_serialized());
                loop.Quit();
              }));
  EXPECT_TRUE(in.was_serialized());
  EXPECT_FALSE(in.was_deserialized());
  loop.Run();
  EXPECT_TRUE(in.was_serialized());
  EXPECT_FALSE(in.was_deserialized());
}

TEST_F(LazySerializationTest, ForceSerializeNested) {
  // Verifies that the [force_serialize] attribute works as intended in a nested
  // context, i.e. when a force-serialized type is contained within a
  // non-force-serialized type,

  Remote<test::ForceSerializeTester> tester;
  ForceSerializeTesterImpl impl(tester.BindNewPipeAndPassReceiver());

  constexpr int32_t kTestValue = 42;

  base::RunLoop loop;
  test::StructNestedForceSerializeImpl in;
  in.force().set_value(kTestValue);
  EXPECT_FALSE(in.was_serialized());
  EXPECT_FALSE(in.was_deserialized());
  tester->SendNestedForceSerializedStruct(
      in, base::BindLambdaForTesting(
              [&](const test::StructNestedForceSerializeImpl& passed) {
                EXPECT_EQ(kTestValue, passed.force().value());
                EXPECT_TRUE(passed.was_deserialized());
                EXPECT_FALSE(passed.was_serialized());
                loop.Quit();
              }));
  EXPECT_TRUE(in.was_serialized());
  EXPECT_FALSE(in.was_deserialized());
  loop.Run();
  EXPECT_TRUE(in.was_serialized());
  EXPECT_FALSE(in.was_deserialized());
}

}  // namespace
}  // namespace mojo
