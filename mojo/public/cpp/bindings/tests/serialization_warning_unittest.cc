// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Serialization warnings are only recorded when DLOG is enabled.
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)

#include <stddef.h>
#include <utility>

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/interfaces/bindings/tests/serialization_test_structs.mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_unions.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

using mojo::internal::ContainerValidateParams;
using mojo::internal::GetArrayOfEnumsValidator;
using mojo::internal::GetArrayValidator;
using mojo::internal::GetMapValidator;

// Creates an array of arrays of handles (2 X 3) for testing.
std::vector<std::optional<std::vector<ScopedHandle>>>
CreateTestNestedHandleArray() {
  std::vector<std::optional<std::vector<ScopedHandle>>> array(2);
  for (size_t i = 0; i < array.size(); ++i) {
    std::vector<ScopedHandle> nested_array(3);
    for (size_t j = 0; j < nested_array.size(); ++j) {
      MessagePipe pipe;
      nested_array[j] = ScopedHandle::From(std::move(pipe.handle1));
    }
    array[i].emplace(std::move(nested_array));
  }

  return array;
}

class SerializationWarningTest : public testing::Test {
 public:
  ~SerializationWarningTest() override {}

 protected:
  template <typename T>
  void TestWarning(T obj, mojo::internal::ValidationError expected_warning) {
    using MojomType = typename T::Struct::DataView;

    warning_observer_.set_last_warning(mojo::internal::VALIDATION_ERROR_NONE);

    mojo::Message message(0, 0, 0, 0, nullptr);
    mojo::internal::MessageFragment<
        typename mojo::internal::MojomTypeTraits<MojomType>::Data>
        fragment(message);
    mojo::internal::Serialize<MojomType>(obj, fragment);
    EXPECT_EQ(expected_warning, warning_observer_.last_warning());
  }

  template <typename MojomType, typename T>
  void TestArrayWarning(T obj,
                        mojo::internal::ValidationError expected_warning,
                        const ContainerValidateParams* validate_params) {
    warning_observer_.set_last_warning(mojo::internal::VALIDATION_ERROR_NONE);

    mojo::Message message(0, 0, 0, 0, nullptr);
    mojo::internal::MessageFragment<
        typename mojo::internal::MojomTypeTraits<MojomType>::Data>
        fragment(message);
    mojo::internal::Serialize<MojomType>(obj, fragment, validate_params);
    EXPECT_EQ(expected_warning, warning_observer_.last_warning());
  }

  template <typename T>
  void TestUnionWarning(T obj,
                        mojo::internal::ValidationError expected_warning) {
    using MojomType = typename T::Struct::DataView;

    warning_observer_.set_last_warning(mojo::internal::VALIDATION_ERROR_NONE);

    mojo::Message message(0, 0, 0, 0, nullptr);
    mojo::internal::MessageFragment<
        typename mojo::internal::MojomTypeTraits<MojomType>::Data>
        fragment(message);
    mojo::internal::Serialize<MojomType>(obj, fragment, false);

    EXPECT_EQ(expected_warning, warning_observer_.last_warning());
  }

  mojo::internal::SerializationWarningObserverForTesting warning_observer_;
};

TEST_F(SerializationWarningTest, HandleInStruct) {
  Struct2Ptr test_struct(Struct2::New());
  EXPECT_FALSE(test_struct->hdl.is_valid());

  TestWarning(std::move(test_struct),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE);

  test_struct = Struct2::New();
  MessagePipe pipe;
  test_struct->hdl = ScopedHandle::From(std::move(pipe.handle1));

  TestWarning(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, StructInStruct) {
  Struct3Ptr test_struct(Struct3::New());
  EXPECT_TRUE(!test_struct->struct_1);

  TestWarning(std::move(test_struct),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct3::New();
  test_struct->struct_1 = Struct1::New();

  TestWarning(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, ArrayOfStructsInStruct) {
  Struct4Ptr test_struct(Struct4::New());
  test_struct->data.resize(1);

  TestWarning(std::move(test_struct),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct4::New();
  test_struct->data.resize(0);

  TestWarning(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);

  test_struct = Struct4::New();
  test_struct->data.resize(1);
  test_struct->data[0] = Struct1::New();

  TestWarning(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, FixedArrayOfStructsInStruct) {
  Struct5Ptr test_struct(Struct5::New());
  test_struct->pair.resize(1);
  test_struct->pair[0] = Struct1::New();

  TestWarning(std::move(test_struct),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER);

  test_struct = Struct5::New();
  test_struct->pair.resize(2);
  test_struct->pair[0] = Struct1::New();
  test_struct->pair[1] = Struct1::New();

  TestWarning(std::move(test_struct), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, ArrayOfArraysOfHandles) {
  using MojomType = ArrayDataView<ArrayDataView<ScopedHandle>>;
  auto test_array = CreateTestNestedHandleArray();
  test_array[0] = std::nullopt;
  (*test_array[1])[0] = ScopedHandle();

  constexpr const ContainerValidateParams& validate_params_0 =
      GetArrayValidator<0, true, &GetArrayValidator<0, true, nullptr>()>();
  TestArrayWarning<MojomType>(std::move(test_array),
                              mojo::internal::VALIDATION_ERROR_NONE,
                              &validate_params_0);

  test_array = CreateTestNestedHandleArray();
  test_array[0] = std::nullopt;
  constexpr const ContainerValidateParams& validate_params_1 =
      GetArrayValidator<0, false, &GetArrayValidator<0, true, nullptr>()>();
  TestArrayWarning<MojomType>(
      std::move(test_array),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
      &validate_params_1);

  test_array = CreateTestNestedHandleArray();
  (*test_array[1])[0] = ScopedHandle();
  constexpr const ContainerValidateParams& validate_params_2 =
      GetArrayValidator<0, true, &GetArrayValidator<0, false, nullptr>()>();
  TestArrayWarning<MojomType>(
      std::move(test_array),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
      &validate_params_2);
}

TEST_F(SerializationWarningTest, ArrayOfStrings) {
  using MojomType = ArrayDataView<StringDataView>;

  std::vector<std::string> test_array(3);
  for (size_t i = 0; i < test_array.size(); ++i)
    test_array[i] = "hello";

  constexpr const ContainerValidateParams& validate_params_0 =
      GetArrayValidator<0, true, &GetArrayValidator<0, false, nullptr>()>();
  TestArrayWarning<MojomType>(std::move(test_array),
                              mojo::internal::VALIDATION_ERROR_NONE,
                              &validate_params_0);

  std::vector<std::optional<std::string>> optional_test_array(3);
  constexpr const ContainerValidateParams& validate_params_1 =
      GetArrayValidator<0, false, &GetArrayValidator<0, false, nullptr>()>();
  TestArrayWarning<MojomType>(
      std::move(optional_test_array),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
      &validate_params_1);

  test_array = std::vector<std::string>(2);
  constexpr const ContainerValidateParams& validate_params_2 =
      GetArrayValidator<3, true, &GetArrayValidator<0, false, nullptr>()>();
  TestArrayWarning<MojomType>(
      std::move(test_array),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
      &validate_params_2);
}

TEST_F(SerializationWarningTest, StructInUnion) {
  DummyStructPtr dummy(nullptr);
  ObjectUnionPtr obj = ObjectUnion::NewFDummy(std::move(dummy));

  TestUnionWarning(std::move(obj),
                   mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);
}

TEST_F(SerializationWarningTest, UnionInUnion) {
  PodUnionPtr pod(nullptr);
  ObjectUnionPtr obj = ObjectUnion::NewFPodUnion(std::move(pod));

  TestUnionWarning(std::move(obj),
                   mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);
}

TEST_F(SerializationWarningTest, HandleInUnion) {
  ScopedMessagePipeHandle pipe;
  HandleUnionPtr handle = HandleUnion::NewFMessagePipe(std::move(pipe));

  TestUnionWarning(std::move(handle),
                   mojo::internal::VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE);
}

}  // namespace
}  // namespace test
}  // namespace mojo

#endif
