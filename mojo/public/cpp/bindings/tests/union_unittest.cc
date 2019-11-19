// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/interfaces/bindings/tests/test_structs.mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_unions.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

template <typename InputType, typename DataType>
size_t SerializeStruct(InputType& input,
                       mojo::Message* message,
                       mojo::internal::SerializationContext* context,
                       DataType** out_data) {
  using StructType = typename InputType::Struct;
  using DataViewType = typename StructType::DataView;
  *message = mojo::Message(0, 0, 0, 0, nullptr);
  const size_t payload_start = message->payload_buffer()->cursor();
  typename DataType::BufferWriter writer;
  mojo::internal::Serialize<DataViewType>(input, message->payload_buffer(),
                                          &writer, context);
  *out_data = writer.is_null() ? nullptr : writer.data();
  return message->payload_buffer()->cursor() - payload_start;
}

template <typename InputType, typename DataType>
size_t SerializeUnion(InputType& input,
                      mojo::Message* message,
                      mojo::internal::SerializationContext* context,
                      DataType** out_data = nullptr) {
  using StructType = typename InputType::Struct;
  using DataViewType = typename StructType::DataView;
  *message = mojo::Message(0, 0, 0, 0, nullptr);
  const size_t payload_start = message->payload_buffer()->cursor();
  typename DataType::BufferWriter writer;
  mojo::internal::Serialize<DataViewType>(input, message->payload_buffer(),
                                          &writer, false, context);
  *out_data = writer.is_null() ? nullptr : writer.data();
  return message->payload_buffer()->cursor() - payload_start;
}

template <typename DataViewType, typename InputType>
size_t SerializeArray(InputType& input,
                      bool nullable_elements,
                      mojo::Message* message,
                      mojo::internal::SerializationContext* context,
                      typename DataViewType::Data_** out_data) {
  *message = mojo::Message(0, 0, 0, 0, nullptr);
  const size_t payload_start = message->payload_buffer()->cursor();
  typename DataViewType::Data_::BufferWriter writer;
  mojo::internal::ContainerValidateParams validate_params(0, nullable_elements,
                                                          nullptr);
  mojo::internal::Serialize<DataViewType>(input, message->payload_buffer(),
                                          &writer, &validate_params, context);
  *out_data = writer.is_null() ? nullptr : writer.data();
  return message->payload_buffer()->cursor() - payload_start;
}

TEST(UnionTest, PlainOldDataGetterSetter) {
  PodUnionPtr pod(PodUnion::New());

  pod->set_f_int8(10);
  EXPECT_EQ(10, pod->get_f_int8());
  EXPECT_TRUE(pod->is_f_int8());
  EXPECT_FALSE(pod->is_f_int8_other());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT8);

  pod->set_f_uint8(11);
  EXPECT_EQ(11, pod->get_f_uint8());
  EXPECT_TRUE(pod->is_f_uint8());
  EXPECT_FALSE(pod->is_f_int8());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT8);

  pod->set_f_int16(12);
  EXPECT_EQ(12, pod->get_f_int16());
  EXPECT_TRUE(pod->is_f_int16());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT16);

  pod->set_f_uint16(13);
  EXPECT_EQ(13, pod->get_f_uint16());
  EXPECT_TRUE(pod->is_f_uint16());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT16);

  pod->set_f_int32(14);
  EXPECT_EQ(14, pod->get_f_int32());
  EXPECT_TRUE(pod->is_f_int32());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT32);

  pod->set_f_uint32(uint32_t{15});
  EXPECT_EQ(uint32_t{15}, pod->get_f_uint32());
  EXPECT_TRUE(pod->is_f_uint32());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT32);

  pod->set_f_int64(16);
  EXPECT_EQ(16, pod->get_f_int64());
  EXPECT_TRUE(pod->is_f_int64());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT64);

  pod->set_f_uint64(uint64_t{17});
  EXPECT_EQ(uint64_t{17}, pod->get_f_uint64());
  EXPECT_TRUE(pod->is_f_uint64());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT64);

  pod->set_f_float(1.5);
  EXPECT_EQ(1.5, pod->get_f_float());
  EXPECT_TRUE(pod->is_f_float());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_FLOAT);

  pod->set_f_double(1.9);
  EXPECT_EQ(1.9, pod->get_f_double());
  EXPECT_TRUE(pod->is_f_double());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_DOUBLE);

  pod->set_f_bool(true);
  EXPECT_TRUE(pod->get_f_bool());
  pod->set_f_bool(false);
  EXPECT_FALSE(pod->get_f_bool());
  EXPECT_TRUE(pod->is_f_bool());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_BOOL);

  pod->set_f_enum(AnEnum::SECOND);
  EXPECT_EQ(AnEnum::SECOND, pod->get_f_enum());
  EXPECT_TRUE(pod->is_f_enum());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_ENUM);
}

TEST(UnionTest, PlainOldDataFactoryFunction) {
  PodUnionPtr pod = PodUnion::NewFInt8(11);
  EXPECT_EQ(11, pod->get_f_int8());
  EXPECT_TRUE(pod->is_f_int8());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT8);

  pod = PodUnion::NewFInt16(12);
  EXPECT_EQ(12, pod->get_f_int16());
  EXPECT_TRUE(pod->is_f_int16());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT16);

  pod = PodUnion::NewFUint16(13);
  EXPECT_EQ(13, pod->get_f_uint16());
  EXPECT_TRUE(pod->is_f_uint16());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT16);

  pod = PodUnion::NewFInt32(14);
  EXPECT_EQ(14, pod->get_f_int32());
  EXPECT_TRUE(pod->is_f_int32());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT32);

  pod = PodUnion::NewFUint32(15);
  EXPECT_EQ(uint32_t{15}, pod->get_f_uint32());
  EXPECT_TRUE(pod->is_f_uint32());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT32);

  pod = PodUnion::NewFInt64(16);
  EXPECT_EQ(16, pod->get_f_int64());
  EXPECT_TRUE(pod->is_f_int64());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT64);

  pod = PodUnion::NewFUint64(17);
  EXPECT_EQ(uint64_t{17}, pod->get_f_uint64());
  EXPECT_TRUE(pod->is_f_uint64());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT64);

  pod = PodUnion::NewFFloat(1.5);
  EXPECT_EQ(1.5, pod->get_f_float());
  EXPECT_TRUE(pod->is_f_float());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_FLOAT);

  pod = PodUnion::NewFDouble(1.9);
  EXPECT_EQ(1.9, pod->get_f_double());
  EXPECT_TRUE(pod->is_f_double());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_DOUBLE);

  pod = PodUnion::NewFBool(true);
  EXPECT_TRUE(pod->get_f_bool());
  pod = PodUnion::NewFBool(false);
  EXPECT_FALSE(pod->get_f_bool());
  EXPECT_TRUE(pod->is_f_bool());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_BOOL);

  pod = PodUnion::NewFEnum(AnEnum::SECOND);
  EXPECT_EQ(AnEnum::SECOND, pod->get_f_enum());
  EXPECT_TRUE(pod->is_f_enum());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_ENUM);

  pod = PodUnion::NewFEnum(AnEnum::FIRST);
  EXPECT_EQ(AnEnum::FIRST, pod->get_f_enum());
  EXPECT_TRUE(pod->is_f_enum());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_ENUM);
}

TEST(UnionTest, PodEquals) {
  PodUnionPtr pod1(PodUnion::New());
  PodUnionPtr pod2(PodUnion::New());

  pod1->set_f_int8(10);
  pod2->set_f_int8(10);
  EXPECT_TRUE(pod1.Equals(pod2));

  pod2->set_f_int8(11);
  EXPECT_FALSE(pod1.Equals(pod2));

  pod2->set_f_int8_other(10);
  EXPECT_FALSE(pod1.Equals(pod2));
}

TEST(UnionTest, PodClone) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  PodUnionPtr pod_clone = pod.Clone();
  EXPECT_EQ(10, pod_clone->get_f_int8());
  EXPECT_TRUE(pod_clone->is_f_int8());
  EXPECT_EQ(pod_clone->which(), PodUnion::Tag::F_INT8);
}

TEST(UnionTest, PodSerialization) {
  PodUnionPtr pod1(PodUnion::New());
  pod1->set_f_int8(10);

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::PodUnion_Data* data = nullptr;
  EXPECT_EQ(16U, SerializeUnion(pod1, &message, &context, &data));

  PodUnionPtr pod2;
  mojo::internal::Deserialize<PodUnionDataView>(data, &pod2, &context);

  EXPECT_EQ(10, pod2->get_f_int8());
  EXPECT_TRUE(pod2->is_f_int8());
  EXPECT_EQ(pod2->which(), PodUnion::Tag::F_INT8);
}

TEST(UnionTest, EnumSerialization) {
  PodUnionPtr pod1(PodUnion::NewFEnum(AnEnum::SECOND));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::PodUnion_Data* data = nullptr;
  EXPECT_EQ(16U, SerializeUnion(pod1, &message, &context, &data));

  PodUnionPtr pod2;
  mojo::internal::Deserialize<PodUnionDataView>(data, &pod2, nullptr);

  EXPECT_EQ(AnEnum::SECOND, pod2->get_f_enum());
  EXPECT_TRUE(pod2->is_f_enum());
  EXPECT_EQ(pod2->which(), PodUnion::Tag::F_ENUM);
}

TEST(UnionTest, PodValidation) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::PodUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(pod, &message, &context, &data);
  EXPECT_EQ(16U, size);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::PodUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, SerializeNotNull) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(0);

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::PodUnion_Data* data = nullptr;
  SerializeUnion(pod, &message, &context, &data);
  EXPECT_FALSE(data->is_null());
}

TEST(UnionTest, SerializeIsNullInlined) {
  PodUnionPtr pod;

  mojo::internal::FixedBufferForTesting buffer(16);
  internal::PodUnion_Data::BufferWriter writer;
  writer.Allocate(&buffer);
  mojo::internal::SerializationContext context;
  mojo::internal::Serialize<PodUnionDataView>(pod, &buffer, &writer, true,
                                              &context);
  EXPECT_TRUE(writer.data()->is_null());
  EXPECT_EQ(16U, buffer.cursor());

  PodUnionPtr pod2;
  mojo::internal::Deserialize<PodUnionDataView>(writer.data(), &pod2, nullptr);
  EXPECT_TRUE(pod2.is_null());
}

TEST(UnionTest, SerializeIsNullNotInlined) {
  PodUnionPtr pod;
  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::PodUnion_Data* data = nullptr;
  EXPECT_EQ(0u, SerializeUnion(pod, &message, &context, &data));
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
  internal::PodUnion_Data::BufferWriter writer;
  writer.Allocate(message.payload_buffer());
  mojo::internal::ValidationContext validation_context(
      writer.data(), static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::PodUnion_Data::Validate(writer.data(),
                                                 &validation_context, false));
}

TEST(UnionTest, UnknownTagValidation) {
  constexpr size_t size = sizeof(internal::PodUnion_Data);
  mojo::Message message(0, 0, size, 0, nullptr);
  internal::PodUnion_Data::BufferWriter writer;
  writer.Allocate(message.payload_buffer());
  writer->tag = static_cast<internal::PodUnion_Data::PodUnion_Tag>(0xFFFFFF);
  mojo::internal::ValidationContext validation_context(
      writer.data(), static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::PodUnion_Data::Validate(writer.data(),
                                                 &validation_context, false));
}

TEST(UnionTest, UnknownEnumValueValidation) {
  PodUnionPtr pod(PodUnion::NewFEnum(static_cast<AnEnum>(0xFFFF)));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::PodUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(pod, &message, &context, &data);
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
  mojo::internal::SerializationContext context;
  internal::PodUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(pod, &message, &context, &data);
  EXPECT_EQ(16U, size);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::PodUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, StringGetterSetter) {
  ObjectUnionPtr pod(ObjectUnion::New());

  std::string hello("hello world");
  pod->set_f_string(hello);
  EXPECT_EQ(hello, pod->get_f_string());
  EXPECT_TRUE(pod->is_f_string());
  EXPECT_EQ(pod->which(), ObjectUnion::Tag::F_STRING);
}

TEST(UnionTest, StringFactoryFunction) {
  std::string hello("hello world");
  ObjectUnionPtr pod(ObjectUnion::NewFString(hello));

  EXPECT_EQ(hello, pod->get_f_string());
  EXPECT_TRUE(pod->is_f_string());
  EXPECT_EQ(pod->which(), ObjectUnion::Tag::F_STRING);
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
  EXPECT_EQ(pod_clone->which(), ObjectUnion::Tag::F_STRING);
}

TEST(UnionTest, StringSerialization) {
  std::string hello("hello world");
  ObjectUnionPtr pod1(ObjectUnion::NewFString(hello));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion(pod1, &message, &context, &data);

  ObjectUnionPtr pod2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &pod2, nullptr);
  EXPECT_EQ(hello, pod2->get_f_string());
  EXPECT_TRUE(pod2->is_f_string());
  EXPECT_EQ(pod2->which(), ObjectUnion::Tag::F_STRING);
}

TEST(UnionTest, NullStringValidation) {
  constexpr size_t size = sizeof(internal::ObjectUnion_Data);
  mojo::internal::FixedBufferForTesting buffer(size);
  internal::ObjectUnion_Data::BufferWriter writer;
  writer.Allocate(&buffer);
  writer->tag = internal::ObjectUnion_Data::ObjectUnion_Tag::F_STRING;
  writer->data.unknown = 0x0;
  mojo::internal::ValidationContext validation_context(
      writer.data(), static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::ObjectUnion_Data::Validate(
      buffer.data(), &validation_context, false));
}

TEST(UnionTest, StringPointerOverflowValidation) {
  constexpr size_t size = sizeof(internal::ObjectUnion_Data);
  mojo::internal::FixedBufferForTesting buffer(size);
  internal::ObjectUnion_Data::BufferWriter writer;
  writer.Allocate(&buffer);
  writer->tag = internal::ObjectUnion_Data::ObjectUnion_Tag::F_STRING;
  writer->data.unknown = 0xFFFFFFFFFFFFFFFF;
  mojo::internal::ValidationContext validation_context(
      writer.data(), static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::ObjectUnion_Data::Validate(
      buffer.data(), &validation_context, false));
}

TEST(UnionTest, StringValidateOOB) {
  constexpr size_t size = 32;
  mojo::internal::FixedBufferForTesting buffer(size);
  internal::ObjectUnion_Data::BufferWriter writer;
  writer.Allocate(&buffer);
  writer->tag = internal::ObjectUnion_Data::ObjectUnion_Tag::F_STRING;

  writer->data.f_f_string.offset = 8;
  char* ptr = reinterpret_cast<char*>(&writer->data.f_f_string);
  mojo::internal::ArrayHeader* array_header =
      reinterpret_cast<mojo::internal::ArrayHeader*>(ptr + *ptr);
  array_header->num_bytes = 20;  // This should go out of bounds.
  array_header->num_elements = 20;
  mojo::internal::ValidationContext validation_context(writer.data(), 32, 0, 0);
  EXPECT_FALSE(internal::ObjectUnion_Data::Validate(
      buffer.data(), &validation_context, false));
}

// TODO(azani): Move back in array_unittest.cc when possible.
// Array tests
TEST(UnionTest, PodUnionInArray) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union_array.emplace(2);
  small_struct->pod_union_array.value()[0] = PodUnion::New();
  small_struct->pod_union_array.value()[1] = PodUnion::New();

  small_struct->pod_union_array.value()[0]->set_f_int8(10);
  small_struct->pod_union_array.value()[1]->set_f_int16(12);

  EXPECT_EQ(10, small_struct->pod_union_array.value()[0]->get_f_int8());
  EXPECT_EQ(12, small_struct->pod_union_array.value()[1]->get_f_int16());
}

TEST(UnionTest, PodUnionInArraySerialization) {
  std::vector<PodUnionPtr> array(2);
  array[0] = PodUnion::New();
  array[1] = PodUnion::New();

  array[0]->set_f_int8(10);
  array[1]->set_f_int16(12);
  EXPECT_EQ(2U, array.size());

  mojo::Message message;
  mojo::internal::SerializationContext context;
  mojo::internal::Array_Data<internal::PodUnion_Data>* data;
  EXPECT_EQ(40U, SerializeArray<ArrayDataView<PodUnionDataView>>(
                     array, false, &message, &context, &data));

  std::vector<PodUnionPtr> array2;
  mojo::internal::Deserialize<ArrayDataView<PodUnionDataView>>(data, &array2,
                                                               nullptr);
  EXPECT_EQ(2U, array2.size());
  EXPECT_EQ(10, array2[0]->get_f_int8());
  EXPECT_EQ(12, array2[1]->get_f_int16());
}

TEST(UnionTest, PodUnionInArraySerializationWithNull) {
  std::vector<PodUnionPtr> array(2);
  array[0] = PodUnion::New();

  array[0]->set_f_int8(10);
  EXPECT_EQ(2U, array.size());

  mojo::Message message;
  mojo::internal::SerializationContext context;
  mojo::internal::Array_Data<internal::PodUnion_Data>* data;
  EXPECT_EQ(40U, SerializeArray<ArrayDataView<PodUnionDataView>>(
                     array, true, &message, &context, &data));

  std::vector<PodUnionPtr> array2;
  mojo::internal::Deserialize<ArrayDataView<PodUnionDataView>>(data, &array2,
                                                               nullptr);
  EXPECT_EQ(2U, array2.size());
  EXPECT_EQ(10, array2[0]->get_f_int8());
  EXPECT_TRUE(array2[1].is_null());
}

TEST(UnionTest, ObjectUnionInArraySerialization) {
  std::vector<ObjectUnionPtr> array(2);
  array[0] = ObjectUnion::New();
  array[1] = ObjectUnion::New();

  array[0]->set_f_string("hello");
  array[1]->set_f_string("world");
  EXPECT_EQ(2U, array.size());

  mojo::Message message;
  mojo::internal::SerializationContext context;
  mojo::internal::Array_Data<internal::ObjectUnion_Data>* data;
  const size_t size = SerializeArray<ArrayDataView<ObjectUnionDataView>>(
      array, false, &message, &context, &data);
  EXPECT_EQ(72U, size);

  std::vector<char> new_buf;
  new_buf.resize(size);
  memcpy(new_buf.data(), data, size);

  data =
      reinterpret_cast<mojo::internal::Array_Data<internal::ObjectUnion_Data>*>(
          new_buf.data());
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  mojo::internal::ContainerValidateParams validate_params(0, false, nullptr);
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
  small_struct->pod_union = PodUnion::New();
  small_struct->pod_union->set_f_int8(10);

  SmallStructPtr clone = small_struct.Clone();
  EXPECT_EQ(10, clone->pod_union->get_f_int8());
}

// Serialization test of a struct with a union of plain old data.
TEST(UnionTest, Serialization_UnionOfPods) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::New();
  small_struct->pod_union->set_f_int32(10);

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::SmallStruct_Data* data = nullptr;
  SerializeStruct(small_struct, &message, &context, &data);

  SmallStructPtr deserialized;
  mojo::internal::Deserialize<SmallStructDataView>(data, &deserialized,
                                                   &context);

  EXPECT_EQ(10, deserialized->pod_union->get_f_int32());
}

// Serialization test of a struct with a union of structs.
TEST(UnionTest, Serialization_UnionOfObjects) {
  SmallObjStructPtr obj_struct(SmallObjStruct::New());
  obj_struct->obj_union = ObjectUnion::New();
  std::string hello("hello world");
  obj_struct->obj_union->set_f_string(hello);

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::SmallObjStruct_Data* data = nullptr;
  SerializeStruct(obj_struct, &message, &context, &data);

  SmallObjStructPtr deserialized;
  mojo::internal::Deserialize<SmallObjStructDataView>(data, &deserialized,
                                                      nullptr);

  EXPECT_EQ(hello, deserialized->obj_union->get_f_string());
}

// Validation test of a struct with a union.
TEST(UnionTest, Validation_UnionsInStruct) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::New();
  small_struct->pod_union->set_f_int32(10);

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::SmallStruct_Data* data = nullptr;
  const size_t size = SerializeStruct(small_struct, &message, &context, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(internal::SmallStruct_Data::Validate(data, &validation_context));
}

// Validation test of a struct union fails due to unknown union tag.
TEST(UnionTest, Validation_PodUnionInStruct_Failure) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::New();
  small_struct->pod_union->set_f_int32(10);

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::SmallStruct_Data* data = nullptr;
  const size_t size = SerializeStruct(small_struct, &message, &context, &data);
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
  mojo::internal::FixedBufferForTesting buffer(size);
  mojo::Message message;
  internal::SmallStructNonNullableUnion_Data::BufferWriter writer;
  writer.Allocate(&buffer);
  mojo::internal::ValidationContext validation_context(
      writer.data(), static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::SmallStructNonNullableUnion_Data::Validate(
      writer.data(), &validation_context));
}

// Validation passes with nullable null union.
TEST(UnionTest, Validation_NullableUnion) {
  SmallStructPtr small_struct(SmallStruct::New());

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::SmallStruct_Data* data = nullptr;
  const size_t size = SerializeStruct(small_struct, &message, &context, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(internal::SmallStruct_Data::Validate(data, &validation_context));
}

// TODO(azani): Move back in map_unittest.cc when possible.
// Map Tests
TEST(UnionTest, PodUnionInMap) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union_map.emplace();
  small_struct->pod_union_map.value()["one"] = PodUnion::New();
  small_struct->pod_union_map.value()["two"] = PodUnion::New();

  small_struct->pod_union_map.value()["one"]->set_f_int8(8);
  small_struct->pod_union_map.value()["two"]->set_f_int16(16);

  EXPECT_EQ(8, small_struct->pod_union_map.value()["one"]->get_f_int8());
  EXPECT_EQ(16, small_struct->pod_union_map.value()["two"]->get_f_int16());
}

TEST(UnionTest, PodUnionInMapSerialization) {
  using MojomType = MapDataView<StringDataView, PodUnionDataView>;

  base::flat_map<std::string, PodUnionPtr> map;
  map.insert(std::make_pair("one", PodUnion::New()));
  map.insert(std::make_pair("two", PodUnion::New()));

  map["one"]->set_f_int8(8);
  map["two"]->set_f_int16(16);

  mojo::Message message(0, 0, 0, 0, nullptr);
  mojo::internal::SerializationContext context;
  const size_t payload_start = message.payload_buffer()->cursor();

  typename mojo::internal::MojomTypeTraits<MojomType>::Data::BufferWriter
      writer;
  mojo::internal::ContainerValidateParams validate_params(
      new mojo::internal::ContainerValidateParams(0, false, nullptr),
      new mojo::internal::ContainerValidateParams(0, false, nullptr));
  mojo::internal::Serialize<MojomType>(map, message.payload_buffer(), &writer,
                                       &validate_params, &context);
  EXPECT_EQ(120U, message.payload_buffer()->cursor() - payload_start);

  base::flat_map<std::string, PodUnionPtr> map2;
  mojo::internal::Deserialize<MojomType>(writer.data(), &map2, &context);

  EXPECT_EQ(8, map2["one"]->get_f_int8());
  EXPECT_EQ(16, map2["two"]->get_f_int16());
}

TEST(UnionTest, PodUnionInMapSerializationWithNull) {
  using MojomType = MapDataView<StringDataView, PodUnionDataView>;

  base::flat_map<std::string, PodUnionPtr> map;
  map.insert(std::make_pair("one", PodUnion::New()));
  map.insert(std::make_pair("two", nullptr));

  map["one"]->set_f_int8(8);

  mojo::Message message(0, 0, 0, 0, nullptr);
  mojo::internal::SerializationContext context;
  const size_t payload_start = message.payload_buffer()->cursor();

  typename mojo::internal::MojomTypeTraits<MojomType>::Data::BufferWriter
      writer;
  mojo::internal::ContainerValidateParams validate_params(
      new mojo::internal::ContainerValidateParams(0, false, nullptr),
      new mojo::internal::ContainerValidateParams(0, true, nullptr));
  mojo::internal::Serialize<MojomType>(map, message.payload_buffer(), &writer,
                                       &validate_params, &context);
  EXPECT_EQ(120U, message.payload_buffer()->cursor() - payload_start);

  base::flat_map<std::string, PodUnionPtr> map2;
  mojo::internal::Deserialize<MojomType>(writer.data(), &map2, &context);

  EXPECT_EQ(8, map2["one"]->get_f_int8());
  EXPECT_TRUE(map2["two"].is_null());
}

TEST(UnionTest, StructInUnionGetterSetterPasser) {
  DummyStructPtr dummy(DummyStruct::New());
  dummy->f_int8 = 8;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_dummy(std::move(dummy));

  EXPECT_EQ(8, obj->get_f_dummy()->f_int8);
}

TEST(UnionTest, StructInUnionSerialization) {
  DummyStructPtr dummy(DummyStruct::New());
  dummy->f_int8 = 8;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_dummy(std::move(dummy));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::ObjectUnion_Data* data = nullptr;
  EXPECT_EQ(32U, SerializeUnion(obj, &message, &context, &data));

  ObjectUnionPtr obj2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &obj2, nullptr);
  EXPECT_EQ(8, obj2->get_f_dummy()->f_int8);
}

TEST(UnionTest, StructInUnionValidation) {
  DummyStructPtr dummy(DummyStruct::New());
  dummy->f_int8 = 8;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_dummy(std::move(dummy));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &context, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, StructInUnionValidationNonNullable) {
  mojo::internal::SerializationWarningObserverForTesting suppress_warning;

  DummyStructPtr dummy(nullptr);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_dummy(std::move(dummy));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &context, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, StructInUnionValidationNullable) {
  DummyStructPtr dummy(nullptr);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_nullable(std::move(dummy));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &context, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, ArrayInUnionGetterSetter) {
  std::vector<int8_t> array(2);
  array[0] = 8;
  array[1] = 9;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_array_int8(std::move(array));

  EXPECT_EQ(8, obj->get_f_array_int8()[0]);
  EXPECT_EQ(9, obj->get_f_array_int8()[1]);
}

TEST(UnionTest, ArrayInUnionSerialization) {
  std::vector<int8_t> array(2);
  array[0] = 8;
  array[1] = 9;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_array_int8(std::move(array));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &context, &data);
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

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_array_int8(std::move(array));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &context, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, MapInUnionGetterSetter) {
  base::flat_map<std::string, int8_t> map;
  map.insert({"one", 1});
  map.insert({"two", 2});

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_map_int8(std::move(map));

  EXPECT_EQ(1, obj->get_f_map_int8()["one"]);
  EXPECT_EQ(2, obj->get_f_map_int8()["two"]);
}

TEST(UnionTest, MapInUnionSerialization) {
  base::flat_map<std::string, int8_t> map;
  map.insert({"one", 1});
  map.insert({"two", 2});

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_map_int8(std::move(map));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &context, &data);
  EXPECT_EQ(112U, size);

  ObjectUnionPtr obj2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &obj2, &context);

  EXPECT_EQ(1, obj2->get_f_map_int8()["one"]);
  EXPECT_EQ(2, obj2->get_f_map_int8()["two"]);
}

TEST(UnionTest, MapInUnionValidation) {
  base::flat_map<std::string, int8_t> map;
  map.insert({"one", 1});
  map.insert({"two", 2});

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_map_int8(std::move(map));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &context, &data);
  EXPECT_EQ(112U, size);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, UnionInUnionGetterSetter) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_pod_union(std::move(pod));

  EXPECT_EQ(10, obj->get_f_pod_union()->get_f_int8());
}

TEST(UnionTest, UnionInUnionFactoryFunction) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  ObjectUnionPtr obj(ObjectUnion::NewFPodUnion(std::move(pod)));

  EXPECT_EQ(10, obj->get_f_pod_union()->get_f_int8());
}

TEST(UnionTest, UnionInUnionSerialization) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_pod_union(std::move(pod));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &context, &data);
  EXPECT_EQ(32U, size);

  ObjectUnionPtr obj2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &obj2, nullptr);
  EXPECT_EQ(10, obj2->get_f_pod_union()->get_f_int8());
}

TEST(UnionTest, UnionInUnionValidation) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_pod_union(std::move(pod));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &context, &data);
  EXPECT_EQ(32U, size);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, UnionInUnionValidationNonNullable) {
  mojo::internal::SerializationWarningObserverForTesting suppress_warning;

  PodUnionPtr pod(nullptr);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_pod_union(std::move(pod));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::ObjectUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(obj, &message, &context, &data);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(
      internal::ObjectUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, HandleInUnionGetterSetter) {
  ScopedMessagePipeHandle pipe0;
  ScopedMessagePipeHandle pipe1;

  CreateMessagePipe(nullptr, &pipe0, &pipe1);

  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_message_pipe(std::move(pipe1));

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

  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_message_pipe(std::move(pipe1));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::HandleUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(handle, &message, &context, &data);
  EXPECT_EQ(16U, size);
  EXPECT_EQ(1U, context.handles()->size());

  HandleUnionPtr handle2(HandleUnion::New());
  mojo::internal::Deserialize<HandleUnionDataView>(data, &handle2, &context);

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

  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_message_pipe(std::move(pipe1));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::HandleUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(handle, &message, &context, &data);
  EXPECT_EQ(16U, size);

  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 1, 0);
  EXPECT_TRUE(
      internal::HandleUnion_Data::Validate(data, &validation_context, false));
}

TEST(UnionTest, HandleInUnionValidationNull) {
  mojo::internal::SerializationWarningObserverForTesting suppress_warning;

  ScopedMessagePipeHandle pipe;
  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_message_pipe(std::move(pipe));

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::HandleUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(handle, &message, &context, &data);
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

  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_small_cache(remote.Unbind());

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

  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_small_cache(remote.Unbind());

  mojo::Message message;
  mojo::internal::SerializationContext context;
  internal::HandleUnion_Data* data = nullptr;
  const size_t size = SerializeUnion(handle, &message, &context, &data);
  EXPECT_EQ(16U, size);
  EXPECT_EQ(1U, context.handles()->size());

  HandleUnionPtr handle2(HandleUnion::New());
  mojo::internal::Deserialize<HandleUnionDataView>(data, &handle2, &context);

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

  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int16(16);

  remote->Echo(std::move(pod), base::BindOnce([](PodUnionPtr out) {
                 EXPECT_EQ(16, out->get_f_int16());
               }));
  base::RunLoop().RunUntilIdle();
}

}  // namespace test
}  // namespace mojo
