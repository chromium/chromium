// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/interfaces/bindings/tests/test_data_view.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace data_view {
namespace {

class DataViewTest : public testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

struct DataViewHolder {
  std::unique_ptr<TestStructDataView> data_view;
  mojo::Message message;
};

std::unique_ptr<DataViewHolder> SerializeTestStruct(TestStructPtr input) {
  auto result = std::make_unique<DataViewHolder>();
  result->message = Message(0, 0, 0, 0, nullptr);
  mojo::internal::MessageFragment<internal::TestStruct_Data> fragment(
      result->message);
  mojo::internal::Serialize<TestStructDataView>(input, fragment);
  result->data_view =
      std::make_unique<TestStructDataView>(fragment.data(), &result->message);
  return result;
}

class TestInterfaceImpl : public TestInterface {
 public:
  explicit TestInterfaceImpl(PendingReceiver<TestInterface> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~TestInterfaceImpl() override {}

  // TestInterface implementation:
  void Echo(int32_t value, EchoCallback callback) override {
    std::move(callback).Run(value);
  }

 private:
  Receiver<TestInterface> receiver_;
};

}  // namespace

TEST_F(DataViewTest, String) {
  TestStructPtr obj(TestStruct::New());
  obj->f_string = "hello";

  auto data_view_holder = SerializeTestStruct(std::move(obj));
  auto& data_view = *data_view_holder->data_view;

  StringDataView string_data_view;
  data_view.GetFStringDataView(&string_data_view);

  ASSERT_FALSE(string_data_view.is_null());
  EXPECT_EQ(std::string("hello"),
            std::string(string_data_view.storage(), string_data_view.size()));
}

TEST_F(DataViewTest, NestedStruct) {
  TestStructPtr obj(TestStruct::New());
  obj->f_struct = NestedStruct::New();
  obj->f_struct->f_int32 = 42;

  auto data_view_holder = SerializeTestStruct(std::move(obj));
  auto& data_view = *data_view_holder->data_view;

  NestedStructDataView struct_data_view;
  data_view.GetFStructDataView(&struct_data_view);

  ASSERT_FALSE(struct_data_view.is_null());
  EXPECT_EQ(42, struct_data_view.f_int32());
}

TEST_F(DataViewTest, NativeStruct) {
  TestStructPtr obj(TestStruct::New());
  obj->f_native_struct = native::NativeStruct::New();
  obj->f_native_struct->data = std::vector<uint8_t>({3, 2, 1});

  auto data_view_holder = SerializeTestStruct(std::move(obj));
  auto& data_view = *data_view_holder->data_view;

  native::NativeStructDataView struct_data_view;
  data_view.GetFNativeStructDataView(&struct_data_view);

  ArrayDataView<uint8_t> data_data_view;
  struct_data_view.GetDataDataView(&data_data_view);

  ASSERT_FALSE(data_data_view.is_null());
  ASSERT_EQ(3u, data_data_view.size());
  EXPECT_EQ(3, data_data_view[0]);
  EXPECT_EQ(2, data_data_view[1]);
  EXPECT_EQ(1, data_data_view[2]);
  EXPECT_EQ(3, *data_data_view.data());
}

TEST_F(DataViewTest, BoolArray) {
  TestStructPtr obj(TestStruct::New());
  obj->f_bool_array = {true, false};

  auto data_view_holder = SerializeTestStruct(std::move(obj));
  auto& data_view = *data_view_holder->data_view;

  ArrayDataView<bool> array_data_view;
  data_view.GetFBoolArrayDataView(&array_data_view);

  ASSERT_FALSE(array_data_view.is_null());
  ASSERT_EQ(2u, array_data_view.size());
  EXPECT_TRUE(array_data_view[0]);
  EXPECT_FALSE(array_data_view[1]);
}

TEST_F(DataViewTest, IntegerArray) {
  TestStructPtr obj(TestStruct::New());
  obj->f_int32_array = {1024, 128};

  auto data_view_holder = SerializeTestStruct(std::move(obj));
  auto& data_view = *data_view_holder->data_view;

  ArrayDataView<int32_t> array_data_view;
  data_view.GetFInt32ArrayDataView(&array_data_view);

  ASSERT_FALSE(array_data_view.is_null());
  ASSERT_EQ(2u, array_data_view.size());
  EXPECT_EQ(1024, array_data_view[0]);
  EXPECT_EQ(128, array_data_view[1]);
  EXPECT_EQ(1024, *array_data_view.data());
}

TEST_F(DataViewTest, EnumArray) {
  TestStructPtr obj(TestStruct::New());
  obj->f_enum_array = {TestEnum::VALUE_1, TestEnum::VALUE_0};

  auto data_view_holder = SerializeTestStruct(std::move(obj));
  auto& data_view = *data_view_holder->data_view;

  ArrayDataView<TestEnum> array_data_view;
  data_view.GetFEnumArrayDataView(&array_data_view);

  ASSERT_FALSE(array_data_view.is_null());
  ASSERT_EQ(2u, array_data_view.size());
  EXPECT_EQ(TestEnum::VALUE_1, array_data_view[0]);
  EXPECT_EQ(TestEnum::VALUE_0, array_data_view[1]);

  TestEnum output;
  ASSERT_TRUE(array_data_view.Read(0, &output));
  EXPECT_EQ(TestEnum::VALUE_1, output);
}

TEST_F(DataViewTest, InterfaceArray) {
  PendingRemote<TestInterface> pending_remote;
  TestInterfaceImpl impl(pending_remote.InitWithNewPipeAndPassReceiver());

  TestStructPtr obj(TestStruct::New());
  obj->f_interface_array.push_back(std::move(pending_remote));

  auto data_view_holder = SerializeTestStruct(std::move(obj));
  auto& data_view = *data_view_holder->data_view;

  ArrayDataView<TestInterfacePtrDataView> array_data_view;
  data_view.GetFInterfaceArrayDataView(&array_data_view);

  ASSERT_FALSE(array_data_view.is_null());
  ASSERT_EQ(1u, array_data_view.size());

  pending_remote = array_data_view.Take<PendingRemote<TestInterface>>(0);
  ASSERT_TRUE(pending_remote);
  int32_t result = 0;

  Remote<TestInterface> remote(std::move(pending_remote));
  ASSERT_TRUE(remote->Echo(42, &result));
  EXPECT_EQ(42, result);
}

TEST_F(DataViewTest, NestedArray) {
  TestStructPtr obj(TestStruct::New());
  obj->f_nested_array = {{3, 4}, {2}};

  auto data_view_holder = SerializeTestStruct(std::move(obj));
  auto& data_view = *data_view_holder->data_view;

  ArrayDataView<ArrayDataView<int32_t>> array_data_view;
  data_view.GetFNestedArrayDataView(&array_data_view);

  ASSERT_FALSE(array_data_view.is_null());
  ASSERT_EQ(2u, array_data_view.size());

  ArrayDataView<int32_t> nested_array_data_view;
  array_data_view.GetDataView(0, &nested_array_data_view);
  ASSERT_FALSE(nested_array_data_view.is_null());
  ASSERT_EQ(2u, nested_array_data_view.size());
  EXPECT_EQ(4, nested_array_data_view[1]);

  std::vector<int32_t> vec;
  ASSERT_TRUE(array_data_view.Read(1, &vec));
  ASSERT_EQ(1u, vec.size());
  EXPECT_EQ(2, vec[0]);
}

TEST_F(DataViewTest, StructArray) {
  NestedStructPtr nested_struct(NestedStruct::New());
  nested_struct->f_int32 = 42;

  TestStructPtr obj(TestStruct::New());
  obj->f_struct_array.push_back(std::move(nested_struct));

  auto data_view_holder = SerializeTestStruct(std::move(obj));
  auto& data_view = *data_view_holder->data_view;

  ArrayDataView<NestedStructDataView> array_data_view;
  data_view.GetFStructArrayDataView(&array_data_view);

  ASSERT_FALSE(array_data_view.is_null());
  ASSERT_EQ(1u, array_data_view.size());

  NestedStructDataView struct_data_view;
  array_data_view.GetDataView(0, &struct_data_view);
  ASSERT_FALSE(struct_data_view.is_null());
  EXPECT_EQ(42, struct_data_view.f_int32());

  NestedStructPtr nested_struct2;
  ASSERT_TRUE(array_data_view.Read(0, &nested_struct2));
  ASSERT_TRUE(nested_struct2);
  EXPECT_EQ(42, nested_struct2->f_int32);
}

TEST_F(DataViewTest, Map) {
  TestStructPtr obj(TestStruct::New());
  obj->f_map["1"] = 1;
  obj->f_map["2"] = 2;

  auto data_view_holder = SerializeTestStruct(std::move(obj));
  auto& data_view = *data_view_holder->data_view;

  MapDataView<StringDataView, int32_t> map_data_view;
  data_view.GetFMapDataView(&map_data_view);

  ASSERT_FALSE(map_data_view.is_null());
  ASSERT_EQ(2u, map_data_view.size());

  ASSERT_FALSE(map_data_view.keys().is_null());
  ASSERT_EQ(2u, map_data_view.keys().size());

  ASSERT_FALSE(map_data_view.values().is_null());
  ASSERT_EQ(2u, map_data_view.values().size());

  std::vector<std::string> keys;
  ASSERT_TRUE(map_data_view.ReadKeys(&keys));
  std::vector<int32_t> values;
  ASSERT_TRUE(map_data_view.ReadValues(&values));

  std::unordered_map<std::string, int32_t> map;
  for (size_t i = 0; i < 2; ++i)
    map[keys[i]] = values[i];

  EXPECT_EQ(1, map["1"]);
  EXPECT_EQ(2, map["2"]);
}

TEST_F(DataViewTest, UnionArray) {
  TestUnionPtr union_ptr = TestUnion::NewFInt32(1024);

  TestStructPtr obj(TestStruct::New());
  obj->f_union_array.push_back(std::move(union_ptr));

  auto data_view_holder = SerializeTestStruct(std::move(obj));
  auto& data_view = *data_view_holder->data_view;

  ArrayDataView<TestUnionDataView> array_data_view;
  data_view.GetFUnionArrayDataView(&array_data_view);
  ASSERT_FALSE(array_data_view.is_null());
  ASSERT_EQ(1u, array_data_view.size());

  TestUnionDataView union_data_view;
  array_data_view.GetDataView(0, &union_data_view);
  ASSERT_FALSE(union_data_view.is_null());

  TestUnionPtr union_ptr2;
  ASSERT_TRUE(array_data_view.Read(0, &union_ptr2));
  ASSERT_TRUE(union_ptr2->is_f_int32());
  EXPECT_EQ(1024, union_ptr2->get_f_int32());
}

}  // namespace data_view
}  // namespace test
}  // namespace mojo
