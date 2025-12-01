// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>

#include "base/dcheck_is_on.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/send_validation_serialization.h"
#include "mojo/public/cpp/bindings/lib/send_validation_type.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/serialization_death.test-mojom.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/test_support/validation_errors_test_util.h"
#include "mojo/public/interfaces/bindings/tests/serialization_test_structs.test-mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_unions.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::test {
namespace {

using mojo::internal::ContainerValidateParams;
using mojo::internal::GetArrayOfEnumsValidator;
using mojo::internal::GetArrayValidator;
using mojo::internal::GetMapValidator;

// Creates an array of arrays of handles (2 X 3) for testing.
std::vector<std::optional<std::vector<ScopedHandle>>>
CreateTestNestedHandleArray() {
  std::vector<std::optional<std::vector<ScopedHandle>>> array(2);
  for (auto& i : array) {
    std::vector<ScopedHandle> nested_array(3);
    for (auto& j : nested_array) {
      MessagePipe pipe;
      j = ScopedHandle::From(std::move(pipe.handle1));
    }
    i.emplace(std::move(nested_array));
  }

  return array;
}

// There are other tests that need to be able to check bad messages are properly
// handled which requires suppressing Serialization Checks. These tests will
// ensure death happens.
class SerializationDeathTest : public testing::Test {
 public:
  ~SerializationDeathTest() override = default;

 protected:
  template <typename T>
  void TestDeath(T obj, mojo::internal::ValidationError expected_warning) {
    using MojomType = typename T::Struct::DataView;

    mojo::Message message(0, 0, 0, 0, nullptr);
    mojo::internal::MessageFragment<
        typename mojo::internal::MojomTypeTraits<MojomType>::Data>
        fragment(message);
    if (expected_warning != mojo::internal::VALIDATION_ERROR_NONE) {
      EXPECT_CHECK_DEATH(
          (mojo::internal::Serialize<MojomType,
                                     mojo::internal::SendValidation::kFatal>(
              obj, fragment)));
    } else {
      mojo::internal::Serialize<MojomType,
                                mojo::internal::SendValidation::kFatal>(
          obj, fragment);
    }
  }

  template <typename MojomType, typename T>
  void TestArrayDeath(T obj,
                      mojo::internal::ValidationError expected_warning,
                      const ContainerValidateParams* validate_params) {
    mojo::Message message(0, 0, 0, 0, nullptr);
    mojo::internal::MessageFragment<
        typename mojo::internal::MojomTypeTraits<MojomType>::Data>
        fragment(message);

    if (expected_warning != mojo::internal::VALIDATION_ERROR_NONE) {
      EXPECT_CHECK_DEATH(
          (mojo::internal::Serialize<MojomType,
                                     mojo::internal::SendValidation::kFatal>(
              obj, fragment, validate_params)));
    } else {
      mojo::internal::Serialize<MojomType,
                                mojo::internal::SendValidation::kFatal>(
          obj, fragment, validate_params);
    }
  }

  template <typename MojomType, typename T>
  void TestMapDeath(T obj,
                    mojo::internal::ValidationError expected_warning,
                    const ContainerValidateParams* validate_params) {
    mojo::Message message(0, 0, 0, 0, nullptr);
    mojo::internal::MessageFragment<
        typename mojo::internal::MojomTypeTraits<MojomType>::Data>
        fragment(message);

    if (expected_warning != mojo::internal::VALIDATION_ERROR_NONE) {
      EXPECT_CHECK_DEATH(
          (mojo::internal::Serialize<MojomType,
                                     mojo::internal::SendValidation::kFatal>(
              obj, fragment, validate_params)));
    } else {
      mojo::internal::Serialize<MojomType,
                                mojo::internal::SendValidation::kFatal>(
          obj, fragment, validate_params);
    }
  }

  template <typename T>
  void TestUnionDeath(T obj, mojo::internal::ValidationError expected_warning) {
    using MojomType = typename T::Struct::DataView;

    mojo::Message message(0, 0, 0, 0, nullptr);
    mojo::internal::MessageFragment<
        typename mojo::internal::MojomTypeTraits<MojomType>::Data>
        fragment(message);

    if (expected_warning != mojo::internal::VALIDATION_ERROR_NONE) {
      EXPECT_CHECK_DEATH(
          (mojo::internal::Serialize<MojomType,
                                     mojo::internal::SendValidation::kFatal>(
              obj, fragment, false)));
    } else {
      mojo::internal::Serialize<MojomType,
                                mojo::internal::SendValidation::kFatal>(
          obj, fragment, false);
    }
  }
};

TEST_F(SerializationDeathTest, HandleInStruct) {
  Struct2Ptr test_struct(Struct2::New());
  EXPECT_FALSE(test_struct->hdl.is_valid());

  TestDeath(std::move(test_struct),
            mojo::internal::VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE);

  test_struct = Struct2::New();
  MessagePipe pipe;
  test_struct->hdl = ScopedHandle::From(std::move(pipe.handle1));

  TestDeath(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationDeathTest, StructInStruct) {
  Struct3Ptr test_struct(Struct3::New());
  EXPECT_TRUE(!test_struct->struct_1);

  TestDeath(std::move(test_struct),
            mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct3::New();
  test_struct->struct_1 = Struct1::New();

  TestDeath(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationDeathTest, ArrayOfStructsInStruct) {
  Struct4Ptr test_struct(Struct4::New());
  test_struct->data.resize(1);

  TestDeath(std::move(test_struct),
            mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct4::New();
  test_struct->data.resize(0);

  TestDeath(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);

  test_struct = Struct4::New();
  test_struct->data.resize(1);
  test_struct->data[0] = Struct1::New();

  TestDeath(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationDeathTest, FixedArrayOfStructsInStruct) {
  Struct5Ptr test_struct(Struct5::New());
  test_struct->pair.resize(1);
  test_struct->pair[0] = Struct1::New();

  TestDeath(std::move(test_struct),
            mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER);

  test_struct = Struct5::New();
  test_struct->pair.resize(2);
  test_struct->pair[0] = Struct1::New();
  test_struct->pair[1] = Struct1::New();

  TestDeath(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationDeathTest, ArrayOfArraysOfHandles) {
  using MojomType = ArrayDataView<ArrayDataView<ScopedHandle>>;
  auto test_array = CreateTestNestedHandleArray();
  test_array[0] = std::nullopt;
  (*test_array[1])[0] = ScopedHandle();

  constexpr const ContainerValidateParams& validate_params_0 =
      GetArrayValidator<0, true, &GetArrayValidator<0, true, nullptr>()>();
  TestArrayDeath<MojomType>(std::move(test_array),
                            mojo::internal::VALIDATION_ERROR_NONE,
                            &validate_params_0);

  test_array = CreateTestNestedHandleArray();
  test_array[0] = std::nullopt;
  constexpr const ContainerValidateParams& validate_params_1 =
      GetArrayValidator<0, false, &GetArrayValidator<0, true, nullptr>()>();
  TestArrayDeath<MojomType>(
      std::move(test_array),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
      &validate_params_1);

  test_array = CreateTestNestedHandleArray();
  (*test_array[1])[0] = ScopedHandle();
  constexpr const ContainerValidateParams& validate_params_2 =
      GetArrayValidator<0, true, &GetArrayValidator<0, false, nullptr>()>();
  TestArrayDeath<MojomType>(
      std::move(test_array),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
      &validate_params_2);
}

TEST_F(SerializationDeathTest, ArrayOfStrings) {
  using MojomType = ArrayDataView<StringDataView>;

  std::vector<std::string> test_array(3);
  for (auto& i : test_array) {
    i = "hello";
  }

  constexpr const ContainerValidateParams& validate_params_0 =
      GetArrayValidator<0, true, &GetArrayValidator<0, false, nullptr>()>();
  TestArrayDeath<MojomType>(std::move(test_array),
                            mojo::internal::VALIDATION_ERROR_NONE,
                            &validate_params_0);

  std::vector<std::optional<std::string>> optional_test_array(3);
  constexpr const ContainerValidateParams& validate_params_1 =
      GetArrayValidator<0, false, &GetArrayValidator<0, false, nullptr>()>();
  TestArrayDeath<MojomType>(
      std::move(optional_test_array),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
      &validate_params_1);

  test_array = std::vector<std::string>(2);
  constexpr const ContainerValidateParams& validate_params_2 =
      GetArrayValidator<3, true, &GetArrayValidator<0, false, nullptr>()>();
  TestArrayDeath<MojomType>(
      std::move(test_array),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
      &validate_params_2);
}

TEST_F(SerializationDeathTest, StructInUnion) {
  DummyStructPtr dummy(nullptr);
  ObjectUnionPtr obj = ObjectUnion::NewFDummy(std::move(dummy));

  TestUnionDeath(std::move(obj),
                 mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);
}

TEST_F(SerializationDeathTest, UnionInUnion) {
  PodUnionPtr pod(nullptr);
  ObjectUnionPtr obj = ObjectUnion::NewFPodUnion(std::move(pod));

  TestUnionDeath(std::move(obj),
                 mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);
}

TEST_F(SerializationDeathTest, HandleInUnion) {
  ScopedMessagePipeHandle pipe;
  HandleUnionPtr handle = HandleUnion::NewFMessagePipe(std::move(pipe));

  TestUnionDeath(std::move(handle),
                 mojo::internal::VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE);
}

TEST_F(SerializationDeathTest, MapSerialization) {
  using MojomType = MapDataView<int32_t, StringDataView>;

  std::map<int32_t, std::string> valid_map = {
      {1, "value1"}, {2, "value2"}, {3, "value3"}};

  constexpr const ContainerValidateParams& validate_params =
      GetMapValidator<*&GetArrayValidator<0, true, nullptr>(),
                      *&GetArrayValidator<0, true, nullptr>()>();
  TestMapDeath<MojomType>(std::move(valid_map),
                          mojo::internal::VALIDATION_ERROR_NONE,
                          &validate_params);
}

TEST_F(SerializationDeathTest, MapSerializationNullValue) {
  using MojomType = MapDataView<int32_t, StringDataView>;

  std::map<int32_t, std::optional<std::string>> null_key_map = {
      {1, "value1"},
      {2, std::nullopt},  // Invalid: null value.
      {3, "value3"}};

  constexpr const ContainerValidateParams& validate_params =
      GetMapValidator<*&GetArrayValidator<0, true, nullptr>(),
                      *&GetArrayValidator<0, false, nullptr>()>();
  TestMapDeath<MojomType>(
      std::move(null_key_map),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
      &validate_params);
}

class SerializationDeathFeatureImpl
    : public serialization_death::mojom::SerializationDeath {
 public:
  explicit SerializationDeathFeatureImpl(
      mojo::PendingReceiver<serialization_death::mojom::SerializationDeath>
          receiver)
      : receiver_(this, std::move(receiver)), assoc_receiver_(nullptr) {}
  explicit SerializationDeathFeatureImpl(
      mojo::PendingAssociatedReceiver<
          serialization_death::mojom::SerializationDeath> receiver)
      : receiver_(nullptr), assoc_receiver_(this, std::move(receiver)) {}

  ~SerializationDeathFeatureImpl() override = default;

  SerializationDeathFeatureImpl(const SerializationDeathFeatureImpl&) = delete;
  SerializationDeathFeatureImpl& operator=(
      const SerializationDeathFeatureImpl&) = delete;

  // serialization_death::mojom::SerializationDeath overrides.
  void HandleInStructMethodShouldDie(
      serialization_death::mojom::BadStructPtr bad_struct) override {
    // This should not be reached
    CHECK(false);
  }

  void HandleInStructMethodShouldWarn(
      serialization_death::mojom::BadStructPtr bad_struct) override {
    // This should not be reached
    CHECK(false);
  }

 private:
  mojo::Receiver<serialization_death::mojom::SerializationDeath> receiver_;
  mojo::AssociatedReceiver<serialization_death::mojom::SerializationDeath>
      assoc_receiver_;
};

class SerializationDeathFeatureTest : public testing::Test {
 public:
  ~SerializationDeathFeatureTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::internal::SerializationWarningObserverForTesting warning_observer_;
};

TEST_F(SerializationDeathFeatureTest, TestSendValidationWithFeatureOn) {
  // Validate this is an invalid struct
  serialization_death::mojom::BadStructPtr test_struct(
      serialization_death::mojom::BadStruct::New());
  EXPECT_FALSE(test_struct->hdl.is_valid());

  base::RunLoop run_loop;

  Remote<serialization_death::mojom::SerializationDeath> remote;
  auto pending_receiver = remote.BindNewPipeAndPassReceiver();
  warning_observer_.set_last_warning(mojo::internal::VALIDATION_ERROR_NONE);
  warning_observer_.clear_send_validation();

  {
    remote->HandleInStructMethodShouldDie(std::move(test_struct));
  }

  // Receiver can now process messages, the message will be rejected
  SerializationDeathFeatureImpl impl(std::move(pending_receiver));
  remote.set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(warning_observer_.last_warning() ==
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE);
  EXPECT_TRUE(warning_observer_.send_side_validation().has_value());
  EXPECT_TRUE(warning_observer_.send_side_validation().value() ==
              mojo::internal::SendValidation::kFatal);
}

TEST_F(SerializationDeathFeatureTest, TestSendValidationWithFeatureOff) {
  // Validate this is an invalid struct
  serialization_death::mojom::BadStructPtr test_struct(
      serialization_death::mojom::BadStruct::New());
  EXPECT_FALSE(test_struct->hdl.is_valid());

  base::RunLoop run_loop;

  Remote<serialization_death::mojom::SerializationDeath> remote;
  auto pending_receiver = remote.BindNewPipeAndPassReceiver();
  warning_observer_.set_last_warning(mojo::internal::VALIDATION_ERROR_NONE);
  warning_observer_.clear_send_validation();

  {
    remote->HandleInStructMethodShouldWarn(std::move(test_struct));
  }

  // Receiver can now process messages, the message will be rejected
  SerializationDeathFeatureImpl impl(std::move(pending_receiver));
  remote.set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();

  // Warning only happens when DCHECK is on
#if DCHECK_IS_ON()
  EXPECT_TRUE(warning_observer_.last_warning() ==
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE);
  EXPECT_TRUE(warning_observer_.send_side_validation().has_value());
  EXPECT_TRUE(warning_observer_.send_side_validation().value() ==
              mojo::internal::SendValidation::kWarning);
#else
  // This should not have done anything.
  EXPECT_TRUE(warning_observer_.last_warning() ==
              mojo::internal::VALIDATION_ERROR_NONE);
  EXPECT_FALSE(warning_observer_.send_side_validation().has_value());
#endif  // DCHECK_IS_ON()
}

}  // namespace
}  // namespace mojo::test
