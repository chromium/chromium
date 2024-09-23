// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/union_unittest.test-mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/interfaces/bindings/tests/test_structs.mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_unions.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace union_unittest {

template <typename InputType, typename DataType>
size_t SerializeStruct(InputType& input,
                       mojo::Message* message,
                       DataType** out_data) {
  using StructType = typename InputType::Struct;
  using DataViewType = typename StructType::DataView;
  *message = mojo::Message(0, 0, 0, 0, nullptr);
  const size_t payload_start = message->payload_buffer()->cursor();
  mojo::internal::MessageFragment<DataType> fragment(*message);
  mojo::internal::Serialize<DataViewType>(input, fragment);
  *out_data = fragment.is_null() ? nullptr : fragment.data();
  return message->payload_buffer()->cursor() - payload_start;
}

template <typename InputType, typename DataType>
size_t SerializeUnion(InputType& input,
                      mojo::Message* message,
                      DataType** out_data = nullptr) {
  using StructType = typename InputType::Struct;
  using DataViewType = typename StructType::DataView;
  *message = mojo::Message(0, 0, 0, 0, nullptr);
  const size_t payload_start = message->payload_buffer()->cursor();
  mojo::internal::MessageFragment<DataType> fragment(*message);
  mojo::internal::Serialize<DataViewType>(input, fragment, false);
  *out_data = fragment.is_null() ? nullptr : fragment.data();
  return message->payload_buffer()->cursor() - payload_start;
}

template <typename DataViewType, bool nullable_elements, typename InputType>
size_t SerializeArray(InputType& input,
                      mojo::Message* message,
                      typename DataViewType::Data_** out_data) {
  *message = mojo::Message(0, 0, 0, 0, nullptr);
  const size_t payload_start = message->payload_buffer()->cursor();

  mojo::internal::MessageFragment<typename DataViewType::Data_> fragment(
      *message);
  constexpr const mojo::internal::ContainerValidateParams& validate_params =
      mojo::internal::GetArrayValidator<0, nullable_elements, nullptr>();
  mojo::internal::Serialize<DataViewType>(input, fragment, &validate_params);
  *out_data = fragment.is_null() ? nullptr : fragment.data();
  return message->payload_buffer()->cursor() - payload_start;
}

TEST(UnionTest, PlainOldDataGetterSetter) {
  PodUnionPtr pod;

  pod = PodUnion::NewFInt8(10);
  EXPECT_EQ(10, pod->get_f_int8());
  EXPECT_TRUE(pod->is_f_int8());
  EXPECT_FALSE(pod->is_f_int8_other());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFInt8);

  pod->set_f_uint8(11);
  EXPECT_EQ(11, pod->get_f_uint8());
  EXPECT_TRUE(pod->is_f_uint8());
  EXPECT_FALSE(pod->is_f_int8());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFUint8);

  pod->set_f_int16(12);
  EXPECT_EQ(12, pod->get_f_int16());
  EXPECT_TRUE(pod->is_f_int16());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFInt16);

  pod->set_f_uint16(13);
  EXPECT_EQ(13, pod->get_f_uint16());
  EXPECT_TRUE(pod->is_f_uint16());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFUint16);

  pod->set_f_int32(14);
  EXPECT_EQ(14, pod->get_f_int32());
  EXPECT_TRUE(pod->is_f_int32());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFInt32);

  pod->set_f_uint32(uint32_t{15});
  EXPECT_EQ(uint32_t{15}, pod->get_f_uint32());
  EXPECT_TRUE(pod->is_f_uint32());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFUint32);

  pod->set_f_int64(16);
  EXPECT_EQ(16, pod->get_f_int64());
  EXPECT_TRUE(pod->is_f_int64());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFInt64);

  pod->set_f_uint64(uint64_t{17});
  EXPECT_EQ(uint64_t{17}, pod->get_f_uint64());
  EXPECT_TRUE(pod->is_f_uint64());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFUint64);

  pod->set_f_float(1.5);
  EXPECT_EQ(1.5, pod->get_f_float());
  EXPECT_TRUE(pod->is_f_float());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFFloat);

  pod->set_f_double(1.9);
  EXPECT_EQ(1.9, pod->get_f_double());
  EXPECT_TRUE(pod->is_f_double());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFDouble);

  pod->set_f_bool(true);
  EXPECT_TRUE(pod->get_f_bool());
  pod->set_f_bool(false);
  EXPECT_FALSE(pod->get_f_bool());
  EXPECT_TRUE(pod->is_f_bool());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFBool);

  pod->set_f_enum(AnEnum::SECOND);
  EXPECT_EQ(AnEnum::SECOND, pod->get_f_enum());
  EXPECT_TRUE(pod->is_f_enum());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFEnum);
}

TEST(UnionTest, PlainOldDataFactoryFunction) {
  PodUnionPtr pod = PodUnion::NewFInt8(11);
  EXPECT_EQ(11, pod->get_f_int8());
  EXPECT_TRUE(pod->is_f_int8());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFInt8);

  pod = PodUnion::NewFInt16(12);
  EXPECT_EQ(12, pod->get_f_int16());
  EXPECT_TRUE(pod->is_f_int16());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFInt16);

  pod = PodUnion::NewFUint16(13);
  EXPECT_EQ(13, pod->get_f_uint16());
  EXPECT_TRUE(pod->is_f_uint16());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFUint16);

  pod = PodUnion::NewFInt32(14);
  EXPECT_EQ(14, pod->get_f_int32());
  EXPECT_TRUE(pod->is_f_int32());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFInt32);

  pod = PodUnion::NewFUint32(15);
  EXPECT_EQ(uint32_t{15}, pod->get_f_uint32());
  EXPECT_TRUE(pod->is_f_uint32());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFUint32);

  pod = PodUnion::NewFInt64(16);
  EXPECT_EQ(16, pod->get_f_int64());
  EXPECT_TRUE(pod->is_f_int64());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFInt64);

  pod = PodUnion::NewFUint64(17);
  EXPECT_EQ(uint64_t{17}, pod->get_f_uint64());
  EXPECT_TRUE(pod->is_f_uint64());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFUint64);

  pod = PodUnion::NewFFloat(1.5);
  EXPECT_EQ(1.5, pod->get_f_float());
  EXPECT_TRUE(pod->is_f_float());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFFloat);

  pod = PodUnion::NewFDouble(1.9);
  EXPECT_EQ(1.9, pod->get_f_double());
  EXPECT_TRUE(pod->is_f_double());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFDouble);

  pod = PodUnion::NewFBool(true);
  EXPECT_TRUE(pod->get_f_bool());
  pod = PodUnion::NewFBool(false);
  EXPECT_FALSE(pod->get_f_bool());
  EXPECT_TRUE(pod->is_f_bool());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFBool);

  pod = PodUnion::NewFEnum(AnEnum::SECOND);
  EXPECT_EQ(AnEnum::SECOND, pod->get_f_enum());
  EXPECT_TRUE(pod->is_f_enum());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFEnum);

  pod = PodUnion::NewFEnum(AnEnum::FIRST);
  EXPECT_EQ(AnEnum::FIRST, pod->get_f_enum());
  EXPECT_TRUE(pod->is_f_enum());
  EXPECT_EQ(pod->which(), PodUnion::Tag::kFEnum);
}

TEST(UnionTest, PodEquals) {
  PodUnionPtr pod1(PodUnion::NewFInt8(10));
  PodUnionPtr pod2(PodUnion::NewFInt8(10));
  EXPECT_TRUE(pod1.Equals(pod2));

  pod2->set_f_int8(11);
  EXPECT_FALSE(pod1.Equals(pod2));

  pod2->set_f_int8_other(10);
  EXPECT_FALSE(pod1.Equals(pod2));
}

TEST(UnionTest, PodClone) {
  PodUnionPtr pod(PodUnion::NewFInt8(10));

  PodUnionPtr pod_clone = pod.Clone();
  EXPECT_EQ(10, pod_clone->get_f_int8());
  EXPECT_TRUE(pod_clone->is_f_int8());
  EXPECT_EQ(pod_clone->which(), PodUnion::Tag::kFInt8);
}

TEST(UnionTest, PodSerialization) {
  PodUnionPtr pod1(PodUnion::NewFInt8(10));

  mojo::Message message;
  internal::PodUnion_Data* data = nullptr;
  EXPECT_EQ(16U, SerializeUnion(pod1, &message, &data));

  PodUnionPtr pod2;
  mojo::internal::Deserialize<PodUnionDataView>(data, &pod2, &message);

  EXPECT_EQ(10, pod2->get_f_int8());
  EXPECT_TRUE(pod2->is_f_int8());
  EXPECT_EQ(pod2->which(), PodUnion::Tag::kFInt8);
}

TEST(UnionTest, EnumSerialization) {
  PodUnionPtr pod1(PodUnion::NewFEnum(AnEnum::SECOND));

  mojo::Message message;
  internal::PodUnion_Data* data = nullptr;
  EXPECT_EQ(16U, SerializeUnion(pod1, &message, &data));

  PodUnionPtr pod2;
  mojo::internal::Deserialize<PodUnionDataView>(data, &pod2, nullptr);

  EXPECT_EQ(AnEnum::SECOND, pod2->get_f_enum());
  EXPECT_TRUE(pod2->is_f_enum());
  EXPECT_EQ(pod2->which(), PodUnion::Tag::kFEnum);
}

TEST(UnionTest, PodValidation) {
  PodUnionPtr pod(PodUnion::NewFInt8(10));

  mojo::Message message;
  internal::PodUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(pod, &message, &data);
  EXPECT_EQ(16U, size);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::PodUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, SerializeNotNull) {
  PodUnionPtr pod(PodUnion::NewFInt8(0));

  mojo::Message message;
  internal::PodUnion_Data* data = nullptr;
  SerializeUnion(pod, &message, &data);
  EXPECT_FALSE(data->is_null());
}

TEST(UnionTest, SerializeIsNullInlined) {
  base::MetricsSubSampler::ScopedNeverSampleForTesting no_subsampling_;
  PodUnionPtr pod;

  Message message(0, 0, 0, 0, nullptr);
  mojo::internal::Buffer& buffer = *message.payload_buffer();
  EXPECT_EQ(sizeof(mojo::internal::MessageHeader), buffer.cursor());

  mojo::internal::MessageFragment<internal::PodUnion_Data> fragment(message);
  fragment.Allocate();
  mojo::internal::Serialize<PodUnionDataView>(pod, fragment, true);
  EXPECT_TRUE(fragment->is_null());
  EXPECT_EQ(16U + sizeof(mojo::internal::MessageHeader), buffer.cursor());

  PodUnionPtr pod2;
  mojo::internal::Deserialize<PodUnionDataView>(fragment.data(), &pod2,
                                                nullptr);
  EXPECT_TRUE(pod2.is_null());
}

TEST(UnionTest, SerializeIsNullInlinedV3) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kMojoMessageAlwaysUseLatestVersion);

  PodUnionPtr pod;

  Message message(0, 0, 0, 0, nullptr);
  mojo::internal::Buffer& buffer = *message.payload_buffer();
  EXPECT_EQ(sizeof(mojo::internal::MessageHeaderV3), buffer.cursor());

  mojo::internal::MessageFragment<internal::PodUnion_Data> fragment(message);
  fragment.Allocate();
  mojo::internal::Serialize<PodUnionDataView>(pod, fragment, true);
  EXPECT_TRUE(fragment->is_null());
  EXPECT_EQ(16U + sizeof(mojo::internal::MessageHeaderV3), buffer.cursor());

  PodUnionPtr pod2;
  mojo::internal::Deserialize<PodUnionDataView>(fragment.data(), &pod2,
                                                nullptr);
  EXPECT_TRUE(pod2.is_null());
}

TEST(UnionTest, SerializeIsNullNotInlined) {
  PodUnionPtr pod;
  mojo::Message message;
  internal::PodUnion_Data* data = nullptr;
  EXPECT_EQ(0u, SerializeUnion(pod, &message, &data));
  EXPECT_EQ(nullptr, data);
}

TEST(UnionTest, NullValidation) {
  void* buf = nullptr;
  mojo::internal::ValidationContext validation_context(buf, 0, 0, 0);
  EXPECT_TRUE(internal::PodUnion_Data::Validate(
      buf, &validation_context, false));
}

TEST(UnionTest, OOBValidation) {
  constexpr size_t size = sizeof(internal::PodUnion_Data) - 1;
  mojo::Message message(0, 0, size, 0, nullptr);
  mojo::internal::MessageFragment<internal::PodUnion_Data> fragment(message);
  fragment.Allocate();
  mojo::internal::ValidationContext validation_context(
      fragment.data(), static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::PodUnion_Data::Validate(fragment.data(),
                                                 &validation_context, false));
}

TEST(UnionTest, UnknownTagValidation) {
  constexpr size_t size = sizeof(internal::PodUnion_Data);
  mojo::Message message(0, 0, size, 0, nullptr);
  mojo::internal::MessageFragment<internal::PodUnion_Data> fragment(message);
  fragment.Allocate();
  fragment->tag = static_cast<internal::PodUnion_Data::PodUnion_Tag>(0xFFFFFF);
  mojo::internal::ValidationContext validation_context(
      fragment.data(), static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::PodUnion_Data::Validate(fragment.data(),
                                                 &validation_context, false));
}

TEST(UnionTest, UnknownEnumValueValidation) {
  PodUnionPtr pod(PodUnion::NewFEnum(static_cast<AnEnum>(0xFFFF)));

  mojo::Message message;
  internal::PodUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(pod, &message, &data);
  EXPECT_EQ(16U, size);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(
      internal::PodUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, UnknownExtensibleEnumValueValidation) {
  PodUnionPtr pod(
      PodUnion::NewFExtensibleEnum(static_cast<AnExtensibleEnum>(0xFFFF)));

  mojo::Message message;
  internal::PodUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(pod, &message, &data);
  EXPECT_EQ(16U, size);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::PodUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, StringGetterSetter) {
  std::string hello("hello world");
  ObjectUnionPtr pod(ObjectUnion::NewFString(hello));

  EXPECT_EQ(hello, pod->get_f_string());
  EXPECT_TRUE(pod->is_f_string());
  EXPECT_EQ(pod->which(), ObjectUnion::Tag::kFString);
}

TEST(UnionTest, StringFactoryFunction) {
  std::string hello("hello world");
  ObjectUnionPtr pod(ObjectUnion::NewFString(hello));

  EXPECT_EQ(hello, pod->get_f_string());
  EXPECT_TRUE(pod->is_f_string());
  EXPECT_EQ(pod->which(), ObjectUnion::Tag::kFString);
}

TEST(UnionTest, StringEquals) {
  ObjectUnionPtr pod1(ObjectUnion::NewFString("hello world"));
  ObjectUnionPtr pod2(ObjectUnion::NewFString("hello world"));

  EXPECT_TRUE(pod1.Equals(pod2));

  pod2->set_f_string("hello universe");
  EXPECT_FALSE(pod1.Equals(pod2));
}

TEST(UnionTest, StringClone) {
  std::string hello("hello world");
  ObjectUnionPtr pod(ObjectUnion::NewFString(hello));

  ObjectUnionPtr pod_clone = pod.Clone();
  EXPECT_EQ(hello, pod_clone->get_f_string());
  EXPECT_TRUE(pod_clone->is_f_string());
  EXPECT_EQ(pod_clone->which(), ObjectUnion::Tag::kFString);
}

TEST(UnionTest, StringSerialization) {
  std::string hello("hello world");
  ObjectUnionPtr pod1(ObjectUnion::NewFString(hello));

  mojo::Message message;
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion(pod1, &message, &data);

  ObjectUnionPtr pod2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &pod2, nullptr);
  EXPECT_EQ(hello, pod2->get_f_string());
  EXPECT_TRUE(pod2->is_f_string());
  EXPECT_EQ(pod2->which(), ObjectUnion::Tag::kFString);
}

TEST(UnionTest, NullStringValidation) {
  constexpr size_t size = sizeof(internal::ObjectUnion_Data);
  Message message(0, 0, 0, 0, nullptr);
  mojo::internal::Buffer& buffer = *message.payload_buffer();
  mojo::internal::MessageFragment<internal::ObjectUnion_Data> fragment(message);
  fragment.Allocate();
  fragment->tag = internal::ObjectUnion_Data::ObjectUnion_Tag::kFString;
  fragment->data.unknown = 0x0;
  mojo::internal::ValidationContext validation_context(
      fragment.data(), static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::ObjectUnion_Data::Validate(
      buffer.data(), &validation_context, false));
}

TEST(UnionTest, StringPointerOverflowValidation) {
  constexpr size_t size = sizeof(internal::ObjectUnion_Data);
  Message message(0, 0, 0, 0, nullptr);
  mojo::internal::Buffer& buffer = *message.payload_buffer();
  mojo::internal::MessageFragment<internal::ObjectUnion_Data> fragment(message);
  fragment.Allocate();
  fragment->tag = internal::ObjectUnion_Data::ObjectUnion_Tag::kFString;
  fragment->data.unknown = 0xFFFFFFFFFFFFFFFF;
  mojo::internal::ValidationContext validation_context(
      fragment.data(), static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::ObjectUnion_Data::Validate(
      buffer.data(), &validation_context, false));
}

TEST(UnionTest, StringValidateOOB) {
  Message message(0, 0, 0, 0, nullptr);
  mojo::internal::Buffer& buffer = *message.payload_buffer();
  mojo::internal::MessageFragment<internal::ObjectUnion_Data> fragment(message);
  fragment.Allocate();
  fragment->tag = internal::ObjectUnion_Data::ObjectUnion_Tag::kFString;

  fragment->data.f_f_string.offset = 8;
  char* ptr = reinterpret_cast<char*>(&fragment->data.f_f_string);
  mojo::internal::ArrayHeader* array_header =
      reinterpret_cast<mojo::internal::ArrayHeader*>(ptr + *ptr);
  array_header->num_bytes = 20;  // This should go out of bounds.
  array_header->num_elements = 20;
  mojo::internal::ValidationContext validation_context(fragment.data(), 32, 0,
                                                       0);
  EXPECT_FALSE(internal::ObjectUnion_Data::Validate(
      buffer.data(), &validation_context, false));
}

// TODO(azani): Move back in array_unittest.cc when possible.
// Array tests
TEST(UnionTest, PodUnionInArray) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union_array.emplace(2);
  small_struct->pod_union_array.value()[0] = PodUnion::NewFInt8(10);
  small_struct->pod_union_array.value()[1] = PodUnion::NewFInt16(12);

  EXPECT_EQ(10, small_struct->pod_union_array.value()[0]->get_f_int8());
  EXPECT_EQ(12, small_struct->pod_union_array.value()[1]->get_f_int16());
}

TEST(UnionTest, PodUnionInArraySerialization) {
  std::vector<PodUnionPtr> array(2);
  array[0] = PodUnion::NewFInt8(10);
  array[1] = PodUnion::NewFInt16(12);
  EXPECT_EQ(2U, array.size());

  mojo::Message message;
  mojo::internal::Array_Data<internal::PodUnion_Data>* data;
  EXPECT_EQ(40U, (SerializeArray<ArrayDataView<PodUnionDataView>, false>(
                     array, &message, &data)));

  std::vector<PodUnionPtr> array2;
  mojo::internal::Deserialize<ArrayDataView<PodUnionDataView>>(data, &array2,
                                                               nullptr);
  EXPECT_EQ(2U, array2.size());
  EXPECT_EQ(10, array2[0]->get_f_int8());
  EXPECT_EQ(12, array2[1]->get_f_int16());
}

TEST(UnionTest, PodUnionInArraySerializationWithNull) {
  std::vector<PodUnionPtr> array(2);
  array[0] = PodUnion::NewFInt8(10);
  EXPECT_EQ(2U, array.size());

  mojo::Message message;
  mojo::internal::Array_Data<internal::PodUnion_Data>* data;
  EXPECT_EQ(40U, (SerializeArray<ArrayDataView<PodUnionDataView>, true>(
                     array, &message, &data)));

  std::vector<PodUnionPtr> array2;
  mojo::internal::Deserialize<ArrayDataView<PodUnionDataView>>(data, &array2,
                                                               nullptr);
  EXPECT_EQ(2U, array2.size());
  EXPECT_EQ(10, array2[0]->get_f_int8());
  EXPECT_TRUE(array2[1].is_null());
}

TEST(UnionTest, ObjectUnionInArraySerialization) {
  std::vector<ObjectUnionPtr> array(2);
  array[0] = ObjectUnion::NewFString("hello");
  array[1] = ObjectUnion::NewFString("world");
  EXPECT_EQ(2U, array.size());

  mojo::Message message;
  mojo::internal::Array_Data<internal::ObjectUnion_Data>* data;
  const size_t size = SerializeArray<ArrayDataView<ObjectUnionDataView>, false>(
      array, &message, &data);
  EXPECT_EQ(72U, size);

  std::vector<char> new_buf;
  new_buf.resize(size);
  memcpy(new_buf.data(), data, size);

  data =
      reinterpret_cast<mojo::internal::Array_Data<internal::ObjectUnion_Data>*>(
          new_buf.data());
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  constexpr const mojo::internal::ContainerValidateParams& validate_params =
      mojo::internal::GetArrayValidator<0, false, nullptr>();
  ASSERT_TRUE(mojo::internal::Array_Data<internal::ObjectUnion_Data>::Validate(
      data, &validation_context, &validate_params));

  std::vector<ObjectUnionPtr> array2;
  mojo::internal::Deserialize<ArrayDataView<ObjectUnionDataView>>(data, &array2,
                                                                  nullptr);

  EXPECT_EQ(2U, array2.size());

  EXPECT_EQ("hello", array2[0]->get_f_string());
  EXPECT_EQ("world", array2[1]->get_f_string());
}

// TODO(azani): Move back in struct_unittest.cc when possible.
// Struct tests
TEST(UnionTest, Clone_Union) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::NewFInt8(10);

  SmallStructPtr clone = small_struct.Clone();
  EXPECT_EQ(10, clone->pod_union->get_f_int8());
}

// Serialization test of a struct with a union of plain old data.
TEST(UnionTest, Serialization_UnionOfPods) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::NewFInt32(10);

  mojo::Message message;
  internal::SmallStruct_Data* data = nullptr;
  SerializeStruct(small_struct, &message, &data);

  SmallStructPtr deserialized;
  mojo::internal::Deserialize<SmallStructDataView>(data, &deserialized,
                                                   &message);

  EXPECT_EQ(10, deserialized->pod_union->get_f_int32());
}

// Serialization test of a struct with a union of structs.
TEST(UnionTest, Serialization_UnionOfObjects) {
  SmallObjStructPtr obj_struct(SmallObjStruct::New());
  std::string hello("hello world");
  obj_struct->obj_union = ObjectUnion::NewFString(hello);

  mojo::Message message;
  internal::SmallObjStruct_Data* data = nullptr;
  SerializeStruct(obj_struct, &message, &data);

  SmallObjStructPtr deserialized;
  mojo::internal::Deserialize<SmallObjStructDataView>(data, &deserialized,
                                                      nullptr);

  EXPECT_EQ(hello, deserialized->obj_union->get_f_string());
}

// Validation test of a struct with a union.
TEST(UnionTest, Validation_UnionsInStruct) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::NewFInt32(10);

  mojo::Message message;
  internal::SmallStruct_Data* data = nullptr;
  const size_t size = SerializeStruct(small_struct, &message, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(internal::SmallStruct_Data::Validate(data, &validation_context));
}

// Validation test of a struct union fails due to unknown union tag.
TEST(UnionTest, Validation_PodUnionInStruct_Failure) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::NewFInt32(10);

  mojo::Message message;
  internal::SmallStruct_Data* data = nullptr;
  const size_t size = SerializeStruct(small_struct, &message, &data);
  data->pod_union.tag = static_cast<internal::PodUnion_Data::PodUnion_Tag>(100);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::SmallStruct_Data::Validate(data, &validation_context));
}

// Validation fails due to non-nullable null union in struct.
TEST(UnionTest, Validation_NullUnion_Failure) {
  SmallStructNonNullableUnionPtr small_struct(
      SmallStructNonNullableUnion::New());

  constexpr size_t size = sizeof(internal::SmallStructNonNullableUnion_Data);
  Message message(0, 0, 0, 0, nullptr);
  mojo::internal::MessageFragment<internal::SmallStructNonNullableUnion_Data>
      fragment(message);
  fragment.Allocate();
  mojo::internal::ValidationContext validation_context(
      fragment.data(), static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::SmallStructNonNullableUnion_Data::Validate(
      fragment.data(), &validation_context));
}

// Validation passes with nullable null union.
TEST(UnionTest, Validation_NullableUnion) {
  SmallStructPtr small_struct(SmallStruct::New());

  mojo::Message message;
  internal::SmallStruct_Data* data = nullptr;
  const size_t size = SerializeStruct(small_struct, &message, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(internal::SmallStruct_Data::Validate(data, &validation_context));
}

// TODO(azani): Move back in map_unittest.cc when possible.
// Map Tests
TEST(UnionTest, PodUnionInMap) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union_map.emplace();
  small_struct->pod_union_map.value()["one"] = PodUnion::NewFInt8(8);
  small_struct->pod_union_map.value()["two"] = PodUnion::NewFInt16(16);

  EXPECT_EQ(8, small_struct->pod_union_map.value()["one"]->get_f_int8());
  EXPECT_EQ(16, small_struct->pod_union_map.value()["two"]->get_f_int16());
}

TEST(UnionTest, PodUnionInMapSerialization) {
  using MojomType = MapDataView<StringDataView, PodUnionDataView>;

  base::flat_map<std::string, PodUnionPtr> map;
  map.emplace("one", PodUnion::NewFInt8(8));
  map.emplace("two", PodUnion::NewFInt16(16));

  mojo::Message message(0, 0, 0, 0, nullptr);
  const size_t payload_start = message.payload_buffer()->cursor();

  using DataType = typename mojo::internal::MojomTypeTraits<MojomType>::Data;
  mojo::internal::MessageFragment<DataType> fragment(message);
  constexpr const mojo::internal::ContainerValidateParams& validate_params =
      mojo::internal::GetMapValidator<
          mojo::internal::GetArrayValidator<0, false, nullptr>(),
          mojo::internal::GetArrayValidator<0, false, nullptr>()>();
  mojo::internal::Serialize<MojomType>(map, fragment, &validate_params);
  EXPECT_EQ(120U, message.payload_buffer()->cursor() - payload_start);

  base::flat_map<std::string, PodUnionPtr> map2;
  mojo::internal::Deserialize<MojomType>(fragment.data(), &map2, &message);

  EXPECT_EQ(8, map2["one"]->get_f_int8());
  EXPECT_EQ(16, map2["two"]->get_f_int16());
}

TEST(UnionTest, PodUnionInMapSerializationWithNull) {
  using MojomType = MapDataView<StringDataView, PodUnionDataView>;

  base::flat_map<std::string, PodUnionPtr> map;
  map.emplace("one", PodUnion::NewFInt8(8));
  map.emplace("two", nullptr);

  mojo::Message message(0, 0, 0, 0, nullptr);
  const size_t payload_start = message.payload_buffer()->cursor();

  using DataType = mojo::internal::MojomTypeTraits<MojomType>::Data;
  mojo::internal::MessageFragment<DataType> fragment(message);
  constexpr const mojo::internal::ContainerValidateParams& validate_params =
      mojo::internal::GetMapValidator<
          mojo::internal::GetArrayValidator<0, false, nullptr>(),
          mojo::internal::GetArrayValidator<0, true, nullptr>()>();
  mojo::internal::Serialize<MojomType>(map, fragment, &validate_params);
  EXPECT_EQ(120U, message.payload_buffer()->cursor() - payload_start);

  base::flat_map<std::string, PodUnionPtr> map2;
  mojo::internal::Deserialize<MojomType>(fragment.data(), &map2, &message);

  EXPECT_EQ(8, map2["one"]->get_f_int8());
  EXPECT_TRUE(map2["two"].is_null());
}

TEST(UnionTest, StructInUnionGetterSetterPasser) {
  DummyStructPtr dummy(DummyStruct::New());
  dummy->f_int8 = 8;

  ObjectUnionPtr obj(ObjectUnion::NewFDummy(std::move(dummy)));

  EXPECT_EQ(8, obj->get_f_dummy()->f_int8);
}

TEST(UnionTest, StructInUnionSerialization) {
  DummyStructPtr dummy(DummyStruct::New());
  dummy->f_int8 = 8;

  ObjectUnionPtr obj(ObjectUnion::NewFDummy(std::move(dummy)));

  mojo::Message message;
  internal::ObjectUnion_Data* data = nullptr;
  EXPECT_EQ(32U, SerializeUnion(obj, &message, &data));

  ObjectUnionPtr obj2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &obj2, nullptr);
  EXPECT_EQ(8, obj2->get_f_dummy()->f_int8);
}

TEST(UnionTest, StructInUnionValidation) {
  DummyStructPtr dummy(DummyStruct::New());
  dummy->f_int8 = 8;

  ObjectUnionPtr obj(ObjectUnion::NewFDummy(std::move(dummy)));

  mojo::Message message;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, StructInUnionValidationNonNullable) {
  mojo::internal::SerializationWarningObserverForTesting suppress_warning;

  DummyStructPtr dummy(nullptr);

  ObjectUnionPtr obj(ObjectUnion::NewFDummy(std::move(dummy)));

  mojo::Message message;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, StructInUnionValidationNullable) {
  DummyStructPtr dummy(nullptr);

  ObjectUnionPtr obj(ObjectUnion::NewFNullable(std::move(dummy)));

  mojo::Message message;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, ArrayInUnionGetterSetter) {
  std::vector<int8_t> array(2);
  array[0] = 8;
  array[1] = 9;

  ObjectUnionPtr obj(ObjectUnion::NewFArrayInt8(std::move(array)));

  EXPECT_EQ(8, obj->get_f_array_int8()[0]);
  EXPECT_EQ(9, obj->get_f_array_int8()[1]);
}

TEST(UnionTest, ArrayInUnionSerialization) {
  std::vector<int8_t> array(2);
  array[0] = 8;
  array[1] = 9;

  ObjectUnionPtr obj(ObjectUnion::NewFArrayInt8(std::move(array)));

  mojo::Message message;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &data);
  EXPECT_EQ(32U, size);

  ObjectUnionPtr obj2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &obj2, nullptr);

  EXPECT_EQ(8, obj2->get_f_array_int8()[0]);
  EXPECT_EQ(9, obj2->get_f_array_int8()[1]);
}

TEST(UnionTest, ArrayInUnionValidation) {
  std::vector<int8_t> array(2);
  array[0] = 8;
  array[1] = 9;

  ObjectUnionPtr obj(ObjectUnion::NewFArrayInt8(std::move(array)));

  mojo::Message message;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, MapInUnionGetterSetter) {
  base::flat_map<std::string, int8_t> map;
  map.insert({"one", 1});
  map.insert({"two", 2});

  ObjectUnionPtr obj(ObjectUnion::NewFMapInt8(std::move(map)));

  EXPECT_EQ(1, obj->get_f_map_int8()["one"]);
  EXPECT_EQ(2, obj->get_f_map_int8()["two"]);
}

TEST(UnionTest, MapInUnionSerialization) {
  base::flat_map<std::string, int8_t> map;
  map.insert({"one", 1});
  map.insert({"two", 2});

  ObjectUnionPtr obj(ObjectUnion::NewFMapInt8(std::move(map)));

  mojo::Message message;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &data);
  EXPECT_EQ(112U, size);

  ObjectUnionPtr obj2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &obj2, &message);

  EXPECT_EQ(1, obj2->get_f_map_int8()["one"]);
  EXPECT_EQ(2, obj2->get_f_map_int8()["two"]);
}

TEST(UnionTest, MapInUnionValidation) {
  base::flat_map<std::string, int8_t> map;
  map.insert({"one", 1});
  map.insert({"two", 2});

  ObjectUnionPtr obj(ObjectUnion::NewFMapInt8(std::move(map)));

  mojo::Message message;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &data);
  EXPECT_EQ(112U, size);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, UnionInUnionGetterSetter) {
  PodUnionPtr pod(PodUnion::NewFInt8(10));

  ObjectUnionPtr obj(ObjectUnion::NewFPodUnion(std::move(pod)));

  EXPECT_EQ(10, obj->get_f_pod_union()->get_f_int8());
}

TEST(UnionTest, UnionInUnionFactoryFunction) {
  PodUnionPtr pod(PodUnion::NewFInt8(10));

  ObjectUnionPtr obj(ObjectUnion::NewFPodUnion(std::move(pod)));

  EXPECT_EQ(10, obj->get_f_pod_union()->get_f_int8());
}

TEST(UnionTest, UnionInUnionSerialization) {
  PodUnionPtr pod(PodUnion::NewFInt8(10));

  ObjectUnionPtr obj(ObjectUnion::NewFPodUnion(std::move(pod)));

  mojo::Message message;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &data);
  EXPECT_EQ(32U, size);

  ObjectUnionPtr obj2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &obj2, nullptr);
  EXPECT_EQ(10, obj2->get_f_pod_union()->get_f_int8());
}

TEST(UnionTest, UnionInUnionValidation) {
  PodUnionPtr pod(PodUnion::NewFInt8(10));

  ObjectUnionPtr obj(ObjectUnion::NewFPodUnion(std::move(pod)));

  mojo::Message message;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &data);
  EXPECT_EQ(32U, size);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, UnionInUnionValidationNonNullable) {
  mojo::internal::SerializationWarningObserverForTesting suppress_warning;

  PodUnionPtr pod(nullptr);

  ObjectUnionPtr obj(ObjectUnion::NewFPodUnion(std::move(pod)));

  mojo::Message message;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, HandleInUnionGetterSetter) {
  ScopedMessagePipeHandle pipe0;
  ScopedMessagePipeHandle pipe1;

  CreateMessagePipe(nullptr, &pipe0, &pipe1);

  HandleUnionPtr handle(HandleUnion::NewFMessagePipe(std::move(pipe1)));

  std::string golden("hello world");
  WriteTextMessage(pipe0.get(), golden);

  std::string actual;
  ReadTextMessage(handle->get_f_message_pipe().get(), &actual);

  EXPECT_EQ(golden, actual);
}

TEST(UnionTest, HandleInUnionGetterFactoryFunction) {
  ScopedMessagePipeHandle pipe0;
  ScopedMessagePipeHandle pipe1;

  CreateMessagePipe(nullptr, &pipe0, &pipe1);

  HandleUnionPtr handle(HandleUnion::NewFMessagePipe(std::move(pipe1)));

  std::string golden("hello world");
  WriteTextMessage(pipe0.get(), golden);

  std::string actual;
  ReadTextMessage(handle->get_f_message_pipe().get(), &actual);

  EXPECT_EQ(golden, actual);
}

TEST(UnionTest, HandleInUnionSerialization) {
  ScopedMessagePipeHandle pipe0;
  ScopedMessagePipeHandle pipe1;

  CreateMessagePipe(nullptr, &pipe0, &pipe1);

  HandleUnionPtr handle(HandleUnion::NewFMessagePipe(std::move(pipe1)));

  mojo::Message message;
  internal::HandleUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(handle, &message, &data);
  EXPECT_EQ(16U, size);
  EXPECT_EQ(1U, message.handles()->size());

  HandleUnionPtr handle2;
  mojo::internal::Deserialize<HandleUnionDataView>(data, &handle2, &message);

  std::string golden("hello world");
  WriteTextMessage(pipe0.get(), golden);

  std::string actual;
  ReadTextMessage(handle2->get_f_message_pipe().get(), &actual);

  EXPECT_EQ(golden, actual);
}

TEST(UnionTest, HandleInUnionValidation) {
  ScopedMessagePipeHandle pipe0;
  ScopedMessagePipeHandle pipe1;

  CreateMessagePipe(nullptr, &pipe0, &pipe1);

  HandleUnionPtr handle(HandleUnion::NewFMessagePipe(std::move(pipe1)));

  mojo::Message message;
  internal::HandleUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(handle, &message, &data);
  EXPECT_EQ(16U, size);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 1, 0);
  EXPECT_TRUE(
      internal::HandleUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, HandleInUnionValidationNull) {
  mojo::internal::SerializationWarningObserverForTesting suppress_warning;

  ScopedMessagePipeHandle pipe;
  HandleUnionPtr handle(HandleUnion::NewFMessagePipe(std::move(pipe)));

  mojo::Message message;
  internal::HandleUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(handle, &message, &data);
  EXPECT_EQ(16U, size);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 1, 0);
  EXPECT_FALSE(
      internal::HandleUnion_Data::Validate(data, &validation_context, false));
}

class SmallCacheImpl : public SmallCache {
 public:
  explicit SmallCacheImpl(base::OnceClosure closure)
      : int_value_(0), closure_(std::move(closure)) {}
  ~SmallCacheImpl() override = default;

  int64_t int_value() const { return int_value_; }

 private:
  void SetIntValue(int64_t int_value) override {
    int_value_ = int_value;
    std::move(closure_).Run();
  }
  void GetIntValue(GetIntValueCallback callback) override {
    std::move(callback).Run(int_value_);
  }

  int64_t int_value_;
  base::OnceClosure closure_;
};

TEST(UnionTest, InterfaceInUnion) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  SmallCacheImpl impl(run_loop.QuitClosure());
  Remote<SmallCache> remote;
  Receiver<SmallCache> receiver(&impl, remote.BindNewPipeAndPassReceiver());

  HandleUnionPtr handle(HandleUnion::NewFSmallCache(remote.Unbind()));

  remote.Bind(std::move(handle->get_f_small_cache()));
  remote->SetIntValue(10);
  run_loop.Run();
  EXPECT_EQ(10, impl.int_value());
}

TEST(UnionTest, InterfaceInUnionFactoryFunction) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  SmallCacheImpl impl(run_loop.QuitClosure());
  Remote<SmallCache> remote;
  Receiver<SmallCache> receiver(&impl, remote.BindNewPipeAndPassReceiver());

  HandleUnionPtr handle = HandleUnion::NewFSmallCache(remote.Unbind());
  remote.Bind(std::move(handle->get_f_small_cache()));
  remote->SetIntValue(10);
  run_loop.Run();
  EXPECT_EQ(10, impl.int_value());
}

TEST(UnionTest, InterfaceInUnionSerialization) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  SmallCacheImpl impl(run_loop.QuitClosure());
  Remote<SmallCache> remote;
  Receiver<SmallCache> receiver(&impl, remote.BindNewPipeAndPassReceiver());

  HandleUnionPtr handle(HandleUnion::NewFSmallCache(remote.Unbind()));

  mojo::Message message;
  internal::HandleUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(handle, &message, &data);
  EXPECT_EQ(16U, size);
  EXPECT_EQ(1U, message.handles()->size());

  HandleUnionPtr handle2;
  mojo::internal::Deserialize<HandleUnionDataView>(data, &handle2, &message);

  remote.Bind(std::move(handle2->get_f_small_cache()));
  remote->SetIntValue(10);
  run_loop.Run();
  EXPECT_EQ(10, impl.int_value());
}

class UnionInterfaceImpl : public UnionInterface {
 public:
  UnionInterfaceImpl() = default;
  ~UnionInterfaceImpl() override = default;

 private:
  void Echo(PodUnionPtr in, EchoCallback callback) override {
    std::move(callback).Run(std::move(in));
  }
};

TEST(UnionTest, UnionInInterface) {
  base::test::SingleThreadTaskEnvironment task_environment;
  UnionInterfaceImpl impl;
  Remote<UnionInterface> remote;
  Receiver<UnionInterface> receiver(&impl, remote.BindNewPipeAndPassReceiver());

  PodUnionPtr pod(PodUnion::NewFInt16(16));

  remote->Echo(std::move(pod), base::BindOnce([](PodUnionPtr out) {
                 EXPECT_EQ(16, out->get_f_int16());
               }));
  base::RunLoop().RunUntilIdle();
}

TEST(UnionTest, InlineUnionAllocationWithNonPODFirstField) {
  // Regression test for https://crbug.com/1114366. Should not crash.
  UnionWithStringForFirstFieldPtr u;
  u = UnionWithStringForFirstField::NewS("hey");
}

class ExtensibleTestUnionExchange
    : public mojom::ExtensibleTestUnionExchangeV1 {
 public:
  explicit ExtensibleTestUnionExchange(
      PendingReceiver<mojom::ExtensibleTestUnionExchangeV2> receiver) {
    // Coerce the V2 interface receiver into a V1 receiver. This is OK per
    // comments on the ExtensibleTestUnionExchangeV2 mojom definition.
    receiver_.Bind(PendingReceiver<mojom::ExtensibleTestUnionExchangeV1>(
        receiver.PassPipe()));
  }

  // mojom::ExtensibleTestUnionExchangeV1:
  void ExchangeWithBoolDefault(
      mojom::ExtensibleTestUnionV1WithBoolDefaultPtr u,
      ExchangeWithBoolDefaultCallback callback) override {
    std::move(callback).Run(std::move(u));
  }
  void ExchangeWithIntDefault(
      mojom::ExtensibleTestUnionV1WithIntDefaultPtr u,
      ExchangeWithIntDefaultCallback callback) override {
    std::move(callback).Run(std::move(u));
  }
  void ExchangeWithNullableDefault(
      mojom::ExtensibleTestUnionV1WithNullableDefaultPtr u,
      ExchangeWithNullableDefaultCallback callback) override {
    std::move(callback).Run(std::move(u));
  }

 private:
  Receiver<mojom::ExtensibleTestUnionExchangeV1> receiver_{this};
};

TEST(UnionTest, ExtensibleUnion) {
  base::test::SingleThreadTaskEnvironment task_environment;
  Remote<mojom::ExtensibleTestUnionExchangeV2> remote;
  ExtensibleTestUnionExchange exchange(remote.BindNewPipeAndPassReceiver());

  {
    mojom::ExtensibleTestUnionV1WithBoolDefaultPtr result;
    remote->ExchangeWithBoolDefault(mojom::TestUnionV2::NewBlob({}), &result);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->is_flag());
    EXPECT_FALSE(result->get_flag());
  }

  {
    mojom::ExtensibleTestUnionV1WithIntDefaultPtr result;
    remote->ExchangeWithIntDefault(mojom::TestUnionV2::NewBlob({}), &result);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->is_integer());
    EXPECT_EQ(0u, result->get_integer());
  }

  {
    mojom::ExtensibleTestUnionV1WithNullableDefaultPtr result;
    remote->ExchangeWithNullableDefault(mojom::TestUnionV2::NewBlob({}),
                                        &result);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->is_name());
    EXPECT_FALSE(result->get_name().has_value());
  }
}

}  // namespace union_unittest
}  // namespace test
}  // namespace mojo
