// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <utility>

#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/interfaces/bindings/tests/test_export2.mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_structs.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

RectPtr MakeRect(int32_t factor = 1) {
  return Rect::New(1 * factor, 2 * factor, 10 * factor, 20 * factor);
}

void CheckRect(const Rect& rect, int32_t factor = 1) {
  EXPECT_EQ(1 * factor, rect.x);
  EXPECT_EQ(2 * factor, rect.y);
  EXPECT_EQ(10 * factor, rect.width);
  EXPECT_EQ(20 * factor, rect.height);
}

template <typename StructType>
struct SerializeStructHelperTraits {
  using DataView = typename StructType::DataView;
};

template <>
struct SerializeStructHelperTraits<native::NativeStruct> {
  using DataView = native::NativeStructDataView;
};

template <typename InputType, typename DataType>
size_t SerializeStruct(InputType& input,
                       mojo::Message* message,
                       DataType** out_data) {
  using StructType = typename InputType::Struct;
  using DataViewType =
      typename SerializeStructHelperTraits<StructType>::DataView;
  *message = mojo::Message(0, 0, 0, 0, nullptr);
  const size_t payload_start = message->payload_buffer()->cursor();
  mojo::internal::MessageFragment<DataType> fragment(*message);
  mojo::internal::Serialize<DataViewType>(input, fragment);
  *out_data = fragment.is_null() ? nullptr : fragment.data();
  return message->payload_buffer()->cursor() - payload_start;
}

MultiVersionStructPtr MakeMultiVersionStruct() {
  MessagePipe pipe;
  return MultiVersionStruct::New(123, MakeRect(5), std::string("hello"),
                                 std::vector<int8_t>{10, 9, 8},
                                 std::move(pipe.handle0), false, 42);
}

template <typename U, typename T>
U SerializeAndDeserialize(T input) {
  using InputMojomType = typename T::Struct::DataView;
  using OutputMojomType = typename U::Struct::DataView;

  using InputDataType =
      typename mojo::internal::MojomTypeTraits<InputMojomType>::Data*;
  using OutputDataType =
      typename mojo::internal::MojomTypeTraits<OutputMojomType>::Data*;

  mojo::Message message;
  InputDataType data;
  SerializeStruct(input, &message, &data);

  // Set the subsequent area to a special value, so that we can find out if we
  // mistakenly access the area.
  void* subsequent_area = message.payload_buffer()->AllocateAndGet(32);
  memset(subsequent_area, 0xAA, 32);

  OutputDataType output_data =
      reinterpret_cast<OutputDataType>(message.mutable_payload());

  U output;
  mojo::internal::Deserialize<OutputMojomType>(output_data, &output, &message);
  return std::move(output);
}

using StructTest = testing::Test;

}  // namespace

TEST_F(StructTest, Rect) {
  RectPtr rect;
  EXPECT_TRUE(rect.is_null());
  EXPECT_TRUE(!rect);
  EXPECT_FALSE(rect);

  rect = nullptr;
  EXPECT_TRUE(rect.is_null());
  EXPECT_TRUE(!rect);
  EXPECT_FALSE(rect);

  rect = MakeRect();
  EXPECT_FALSE(rect.is_null());
  EXPECT_FALSE(!rect);
  EXPECT_TRUE(rect);

  RectPtr null_rect = nullptr;
  EXPECT_TRUE(null_rect.is_null());
  EXPECT_TRUE(!null_rect);
  EXPECT_FALSE(null_rect);

  CheckRect(*rect);
}

TEST_F(StructTest, Clone) {
  NamedRegionPtr region;

  NamedRegionPtr clone_region = region.Clone();
  EXPECT_TRUE(clone_region.is_null());

  region = NamedRegion::New();
  clone_region = region.Clone();
  EXPECT_FALSE(clone_region->name);
  EXPECT_FALSE(clone_region->rects);

  region->name.emplace("hello world");
  clone_region = region.Clone();
  EXPECT_EQ(region->name, clone_region->name);

  region->rects.emplace(2);
  (*region->rects)[1] = MakeRect();
  clone_region = region.Clone();
  EXPECT_EQ(2u, clone_region->rects->size());
  EXPECT_TRUE((*clone_region->rects)[0].is_null());
  CheckRect(*(*clone_region->rects)[1]);

  // NoDefaultFieldValues contains handles, so Clone() is not available, but
  // NoDefaultFieldValuesPtr should still compile.
  NoDefaultFieldValuesPtr no_default_field_values(NoDefaultFieldValues::New());
  EXPECT_FALSE(no_default_field_values->f13.is_valid());
}

// Serialization test of a struct with no pointer or handle members.
TEST_F(StructTest, Serialization_Basic) {
  RectPtr rect(MakeRect());

  mojo::Message message;
  internal::Rect_Data* data;
  EXPECT_EQ(8U + 16U, SerializeStruct(rect, &message, &data));

  RectPtr rect2;
  mojo::internal::Deserialize<RectDataView>(data, &rect2, &message);

  CheckRect(*rect2);
}

// Construction of a struct with struct pointers from null.
TEST_F(StructTest, Construction_StructPointers) {
  RectPairPtr pair;
  EXPECT_TRUE(pair.is_null());

  pair = RectPair::New();
  EXPECT_FALSE(pair.is_null());
  EXPECT_TRUE(pair->first.is_null());
  EXPECT_TRUE(pair->first.is_null());

  pair = nullptr;
  EXPECT_TRUE(pair.is_null());
}

// Serialization test of a struct with struct pointers.
TEST_F(StructTest, Serialization_StructPointers) {
  RectPairPtr pair(RectPair::New(MakeRect(), MakeRect()));

  mojo::Message message;
  internal::RectPair_Data* data;
  EXPECT_EQ(8U + 16U + 2 * (8U + 16U), SerializeStruct(pair, &message, &data));

  RectPairPtr pair2;
  mojo::internal::Deserialize<RectPairDataView>(data, &pair2, &message);

  CheckRect(*pair2->first);
  CheckRect(*pair2->second);
}

// Serialization test of a struct with an array member.
TEST_F(StructTest, Serialization_ArrayPointers) {
  std::vector<RectPtr> rects;
  for (size_t i = 0; i < 4; ++i)
    rects.push_back(MakeRect(static_cast<int32_t>(i) + 1));

  NamedRegionPtr region(
      NamedRegion::New(std::string("region"), std::move(rects)));

  mojo::Message message;
  internal::NamedRegion_Data* data;
  EXPECT_EQ(8U +            // header
                8U +        // name pointer
                8U +        // rects pointer
                8U +        // name header
                8U +        // name payload (rounded up)
                8U +        // rects header
                4 * 8U +    // rects payload (four pointers)
                4 * (8U +   // rect header
                     16U),  // rect payload (four ints)
            SerializeStruct(region, &message, &data));

  NamedRegionPtr region2;
  mojo::internal::Deserialize<NamedRegionDataView>(data, &region2, &message);

  EXPECT_EQ("region", *region2->name);

  EXPECT_EQ(4U, region2->rects->size());
  for (size_t i = 0; i < region2->rects->size(); ++i)
    CheckRect(*(*region2->rects)[i], static_cast<int32_t>(i) + 1);
}

// Serialization test of a struct with null array pointers.
TEST_F(StructTest, Serialization_NullArrayPointers) {
  NamedRegionPtr region(NamedRegion::New());
  EXPECT_FALSE(region->name);
  EXPECT_FALSE(region->rects);

  mojo::Message message;
  internal::NamedRegion_Data* data;
  EXPECT_EQ(8U +      // header
                8U +  // name pointer
                8U,   // rects pointer
            SerializeStruct(region, &message, &data));

  NamedRegionPtr region2;
  mojo::internal::Deserialize<NamedRegionDataView>(data, &region2, &message);

  EXPECT_FALSE(region2->name);
  EXPECT_FALSE(region2->rects);
}

// Tests deserializing structs as a newer version.
TEST_F(StructTest, Versioning_OldToNew) {
  {
    MultiVersionStructV0Ptr input(MultiVersionStructV0::New(123));
    MultiVersionStructPtr expected_output(MultiVersionStruct::New(123));

    MultiVersionStructPtr output =
        SerializeAndDeserialize<MultiVersionStructPtr>(std::move(input));
    EXPECT_TRUE(output);
    EXPECT_TRUE(output->Equals(*expected_output));
  }

  {
    MultiVersionStructV1Ptr input(MultiVersionStructV1::New(123, MakeRect(5)));
    MultiVersionStructPtr expected_output(
        MultiVersionStruct::New(123, MakeRect(5)));

    MultiVersionStructPtr output =
        SerializeAndDeserialize<MultiVersionStructPtr>(std::move(input));
    EXPECT_TRUE(output);
    EXPECT_TRUE(output->Equals(*expected_output));
  }

  {
    MultiVersionStructV3Ptr input(
        MultiVersionStructV3::New(123, MakeRect(5), std::string("hello")));
    MultiVersionStructPtr expected_output(
        MultiVersionStruct::New(123, MakeRect(5), std::string("hello")));

    MultiVersionStructPtr output =
        SerializeAndDeserialize<MultiVersionStructPtr>(std::move(input));
    EXPECT_TRUE(output);
    EXPECT_TRUE(output->Equals(*expected_output));
  }

  {
    MultiVersionStructV5Ptr input(MultiVersionStructV5::New(
        123, MakeRect(5), std::string("hello"), std::vector<int8_t>{10, 9, 8}));
    MultiVersionStructPtr expected_output(MultiVersionStruct::New(
        123, MakeRect(5), std::string("hello"), std::vector<int8_t>{10, 9, 8}));

    MultiVersionStructPtr output =
        SerializeAndDeserialize<MultiVersionStructPtr>(std::move(input));
    EXPECT_TRUE(output);
    EXPECT_TRUE(output->Equals(*expected_output));
  }

  {
    MessagePipe pipe;
    MultiVersionStructV7Ptr input(MultiVersionStructV7::New(
        123, MakeRect(5), std::string("hello"), std::vector<int8_t>{10, 9, 8},
        std::move(pipe.handle0), false));

    MultiVersionStructPtr expected_output(MultiVersionStruct::New(
        123, MakeRect(5), std::string("hello"), std::vector<int8_t>{10, 9, 8}));
    // Save the raw handle value separately so that we can compare later.
    MojoHandle expected_handle = input->f_message_pipe.get().value();

    MultiVersionStructPtr output =
        SerializeAndDeserialize<MultiVersionStructPtr>(std::move(input));
    EXPECT_TRUE(output);
    EXPECT_EQ(expected_handle, output->f_message_pipe.get().value());
    output->f_message_pipe.reset();
    EXPECT_TRUE(output->Equals(*expected_output));
  }
}

// Tests deserializing structs as an older version.
TEST_F(StructTest, Versioning_NewToOld) {
  {
    MultiVersionStructPtr input = MakeMultiVersionStruct();
    MultiVersionStructV7Ptr expected_output(MultiVersionStructV7::New(
        123, MakeRect(5), std::string("hello"), std::vector<int8_t>{10, 9, 8}));
    // Save the raw handle value separately so that we can compare later.
    MojoHandle expected_handle = input->f_message_pipe.get().value();

    MultiVersionStructV7Ptr output =
        SerializeAndDeserialize<MultiVersionStructV7Ptr>(std::move(input));
    EXPECT_TRUE(output);
    EXPECT_EQ(expected_handle, output->f_message_pipe.get().value());
    output->f_message_pipe.reset();
    EXPECT_TRUE(output->Equals(*expected_output));
  }

  {
    MultiVersionStructPtr input = MakeMultiVersionStruct();
    MultiVersionStructV5Ptr expected_output(MultiVersionStructV5::New(
        123, MakeRect(5), std::string("hello"), std::vector<int8_t>{10, 9, 8}));

    MultiVersionStructV5Ptr output =
        SerializeAndDeserialize<MultiVersionStructV5Ptr>(std::move(input));
    EXPECT_TRUE(output);
    EXPECT_TRUE(output->Equals(*expected_output));
  }

  {
    MultiVersionStructPtr input = MakeMultiVersionStruct();
    MultiVersionStructV3Ptr expected_output(
        MultiVersionStructV3::New(123, MakeRect(5), std::string("hello")));

    MultiVersionStructV3Ptr output =
        SerializeAndDeserialize<MultiVersionStructV3Ptr>(std::move(input));
    EXPECT_TRUE(output);
    EXPECT_TRUE(output->Equals(*expected_output));
  }

  {
    MultiVersionStructPtr input = MakeMultiVersionStruct();
    MultiVersionStructV1Ptr expected_output(
        MultiVersionStructV1::New(123, MakeRect(5)));

    MultiVersionStructV1Ptr output =
        SerializeAndDeserialize<MultiVersionStructV1Ptr>(std::move(input));
    EXPECT_TRUE(output);
    EXPECT_TRUE(output->Equals(*expected_output));
  }

  {
    MultiVersionStructPtr input = MakeMultiVersionStruct();
    MultiVersionStructV0Ptr expected_output(MultiVersionStructV0::New(123));

    MultiVersionStructV0Ptr output =
        SerializeAndDeserialize<MultiVersionStructV0Ptr>(std::move(input));
    EXPECT_TRUE(output);
    EXPECT_TRUE(output->Equals(*expected_output));
  }
}

// Serialization test for native struct.
TEST_F(StructTest, Serialization_NativeStruct) {
  using Data = native::internal::NativeStruct_Data;
  {
    // Serialization of a null native struct.
    native::NativeStructPtr native;

    mojo::Message message;
    Data* data = nullptr;
    EXPECT_EQ(0u, SerializeStruct(native, &message, &data));
    EXPECT_EQ(nullptr, data);

    native::NativeStructPtr output_native;
    mojo::internal::Deserialize<native::NativeStructDataView>(
        data, &output_native, &message);
    EXPECT_TRUE(output_native.is_null());
  }

  {
    // Serialization of a native struct with null data.
    native::NativeStructPtr native(native::NativeStruct::New());

    mojo::Message message;
    Data* data = nullptr;
    EXPECT_EQ(32u, SerializeStruct(native, &message, &data));
    EXPECT_EQ(0u, data->data.Get()->size());

    native::NativeStructPtr output_native;
    mojo::internal::Deserialize<native::NativeStructDataView>(
        data, &output_native, &message);
    EXPECT_TRUE(output_native->data.empty());
  }

  {
    native::NativeStructPtr native(native::NativeStruct::New());
    native->data = std::vector<uint8_t>{'X', 'Y'};

    mojo::Message message;
    Data* data = nullptr;
    EXPECT_EQ(40u, SerializeStruct(native, &message, &data));
    EXPECT_EQ(2u, data->data.Get()->size());

    native::NativeStructPtr output_native;
    mojo::internal::Deserialize<native::NativeStructDataView>(
        data, &output_native, &message);
    ASSERT_TRUE(output_native);
    ASSERT_FALSE(output_native->data.empty());
    EXPECT_EQ(2u, output_native->data.size());
    EXPECT_EQ('X', output_native->data[0]);
    EXPECT_EQ('Y', output_native->data[1]);
  }
}

TEST_F(StructTest, Serialization_PublicAPI) {
  {
    // A null struct pointer.
    RectPtr null_struct;
    auto data = Rect::Serialize(&null_struct);
    EXPECT_TRUE(data.empty());

    // Initialize it to non-null.
    RectPtr output(Rect::New());
    ASSERT_TRUE(Rect::Deserialize(data, &output));
    EXPECT_TRUE(output.is_null());
  }

  {
    // A struct with no fields.
    EmptyStructPtr empty_struct(EmptyStruct::New());
    auto data = EmptyStruct::Serialize(&empty_struct);
    EXPECT_FALSE(data.empty());

    EmptyStructPtr output;
    ASSERT_TRUE(EmptyStruct::Deserialize(data, &output));
    EXPECT_FALSE(output.is_null());
  }

  {
    // A simple struct.
    RectPtr rect = MakeRect();
    RectPtr cloned_rect = rect.Clone();
    auto data = Rect::Serialize(&rect);

    RectPtr output;
    ASSERT_TRUE(Rect::Deserialize(data, &output));
    EXPECT_TRUE(output.Equals(cloned_rect));
  }

  {
    // A struct containing other objects.
    std::vector<RectPtr> rects;
    for (size_t i = 0; i < 3; ++i)
      rects.push_back(MakeRect(static_cast<int32_t>(i) + 1));
    NamedRegionPtr region(
        NamedRegion::New(std::string("region"), std::move(rects)));

    NamedRegionPtr cloned_region = region.Clone();
    auto data = NamedRegion::Serialize(&region);

    // Make sure that the serialized result gets pointers encoded properly.
    auto cloned_data = data;
    NamedRegionPtr output;
    ASSERT_TRUE(NamedRegion::Deserialize(cloned_data, &output));
    EXPECT_TRUE(output.Equals(cloned_region));
  }

  {
    // Deserialization failure.
    RectPtr rect = MakeRect();
    auto data = Rect::Serialize(&rect);

    NamedRegionPtr output;
    EXPECT_FALSE(NamedRegion::Deserialize(data, &output));
  }

  {
    // A struct from another component.
    auto pair = test_export2::StringPair::New("hello", "world");
    auto data = test_export2::StringPair::Serialize(&pair);

    test_export2::StringPairPtr output;
    ASSERT_TRUE(test_export2::StringPair::Deserialize(data, &output));
    EXPECT_TRUE(output.Equals(pair));
  }
}

TEST_F(StructTest, VersionedStructConstructor) {
  auto reordered = ReorderedStruct::New(123, 456, 789);
  EXPECT_EQ(123, reordered->a);
  EXPECT_EQ(456, reordered->b);
  EXPECT_EQ(789, reordered->c);

  reordered = ReorderedStruct::New(123, 456);
  EXPECT_EQ(123, reordered->a);
  EXPECT_EQ(6, reordered->b);
  EXPECT_EQ(456, reordered->c);

  reordered = ReorderedStruct::New(123);
  EXPECT_EQ(3, reordered->a);
  EXPECT_EQ(6, reordered->b);
  EXPECT_EQ(123, reordered->c);

  reordered = ReorderedStruct::New();
  EXPECT_EQ(3, reordered->a);
  EXPECT_EQ(6, reordered->b);
  EXPECT_EQ(1, reordered->c);
}

TEST_F(StructTest, EnumNestedStructTest) {
  EnumNestedStructPtr enum_nested_struct =
      EnumNestedStruct::New(EnumNestedStruct::StructEnum::FIRST);
  EXPECT_EQ(EnumNestedStruct::StructEnum::FIRST,
            enum_nested_struct->local_enum_state_);

  enum_nested_struct->local_enum_state_ = EnumNestedStruct::StructEnum::SECOND;

  EnumNestedStructPtr expected_output(
      EnumNestedStruct::New(EnumNestedStruct::StructEnum::SECOND));

  EnumNestedStructPtr output = SerializeAndDeserialize<EnumNestedStructPtr>(
      std::move(enum_nested_struct));
  EXPECT_TRUE(output.Equals(expected_output));
  EXPECT_EQ(EnumNestedStruct::StructEnum::SECOND, output->local_enum_state_);
}

TEST_F(StructTest, Matcher) {
  // Pointee will dereference StructPtr<S> or InlinedStructPtr<S> to get S.
  // This test ensures that the matcher compiles and works.
  using ::testing::Pointee;

  using ::testing::AllOf;
  using ::testing::Field;

  mojo::InlinedStructPtr<Rect> rect = MakeRect();
  EXPECT_THAT(
      rect, Pointee(AllOf(Field(&Rect::x, 1), Field(&Rect::y, 2),
                          Field(&Rect::width, 10), Field(&Rect::height, 20))));

  mojo::StructPtr<MultiVersionStruct> multi = MakeMultiVersionStruct();
  EXPECT_THAT(multi, Pointee(AllOf(Field(&MultiVersionStruct::f_int32, 123),
                                   Field(&MultiVersionStruct::f_rect,
                                         Pointee(Field(&Rect::x, 5))))));
}

}  // namespace test
}  // namespace mojo
