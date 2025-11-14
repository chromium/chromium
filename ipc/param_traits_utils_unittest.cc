// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/param_traits_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/test/test_shared_memory_util.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace IPC {
namespace {

// Tests nesting of messages as parameters to other messages.
TEST(IPCMessageUtilsTest, NestedMessages) {
  int nested_content = 456789;
  Message nested_msg;
  ParamTraits<int>::Write(&nested_msg, nested_content);

  // Outer message contains the nested one as its parameter.
  Message outer_msg;
  ParamTraits<Message>::Write(&outer_msg, nested_msg);

  // Read back the nested message.
  base::PickleIterator iter(outer_msg);
  IPC::Message result_msg;
  ASSERT_TRUE(ParamTraits<Message>::Read(&outer_msg, &iter, &result_msg));

  // Verify nested message content
  base::PickleIterator nested_iter(nested_msg);
  int result_content = 0;
  ASSERT_TRUE(
      ParamTraits<int>::Read(&nested_msg, &nested_iter, &result_content));
  EXPECT_EQ(nested_content, result_content);

  // Try reading past the ends for both messages and make sure it fails.
  IPC::Message dummy;
  ASSERT_FALSE(ParamTraits<Message>::Read(&outer_msg, &iter, &dummy));
  ASSERT_FALSE(
      ParamTraits<int>::Read(&nested_msg, &nested_iter, &result_content));
}

// Tests that detection of various bad parameters is working correctly.
TEST(IPCMessageUtilsTest, ParameterValidation) {
  base::FilePath::StringType ok_string(FILE_PATH_LITERAL("hello"), 5);
  base::FilePath::StringType bad_string(FILE_PATH_LITERAL("hel\0o"), 5);

  // Change this if ParamTraits<FilePath>::Write() changes.
  IPC::Message message;
  ParamTraits<base::FilePath::StringType>::Write(&message, ok_string);
  ParamTraits<base::FilePath::StringType>::Write(&message, bad_string);

  base::PickleIterator iter(message);
  base::FilePath ok_path;
  base::FilePath bad_path;
  ASSERT_TRUE(ParamTraits<base::FilePath>::Read(&message, &iter, &ok_path));
  ASSERT_FALSE(ParamTraits<base::FilePath>::Read(&message, &iter, &bad_path));
}

TEST(IPCMessageUtilsTest, InlinedVector) {
  static constexpr size_t stack_capacity = 5;
  absl::InlinedVector<double, stack_capacity> inlined_vector;
  for (size_t i = 0; i < 2 * stack_capacity; i++) {
    inlined_vector.push_back(i * 2.0);
  }

  IPC::Message msg;
  IPC::WriteParam(&msg, inlined_vector);

  absl::InlinedVector<double, stack_capacity> output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));
  ASSERT_EQ(inlined_vector.size(), output.size());
  for (size_t i = 0; i < 2 * stack_capacity; i++) {
    EXPECT_EQ(inlined_vector[i], output[i]);
  }
}

TEST(IPCMessageUtilsTest, MojoChannelHandle) {
  mojo::MessagePipe message_pipe;

  IPC::Message message;
  IPC::WriteParam(&message, message_pipe.handle0.get());

  base::PickleIterator iter(message);
  mojo::MessagePipeHandle result_handle;
  EXPECT_TRUE(IPC::ReadParam(&message, &iter, &result_handle));
  EXPECT_EQ(message_pipe.handle0.get(), result_handle);
}

TEST(IPCMessageUtilsTest, OptionalUnset) {
  std::optional<int> opt;
  base::Pickle pickle;
  IPC::WriteParam(&pickle, opt);

  std::optional<int> unserialized_opt;
  base::PickleIterator iter(pickle);
  EXPECT_TRUE(IPC::ReadParam(&pickle, &iter, &unserialized_opt));
  EXPECT_FALSE(unserialized_opt);
}

TEST(IPCMessageUtilsTest, OptionalSet) {
  std::optional<int> opt(10);
  base::Pickle pickle;
  IPC::WriteParam(&pickle, opt);

  std::optional<int> unserialized_opt;
  base::PickleIterator iter(pickle);
  EXPECT_TRUE(IPC::ReadParam(&pickle, &iter, &unserialized_opt));
  EXPECT_TRUE(unserialized_opt);
  EXPECT_EQ(opt.value(), unserialized_opt.value());
}

template <typename SharedMemoryRegionType>
class SharedMemoryRegionTypedTest : public ::testing::Test {};

typedef ::testing::Types<base::WritableSharedMemoryRegion,
                         base::UnsafeSharedMemoryRegion,
                         base::ReadOnlySharedMemoryRegion>
    AllSharedMemoryRegionTypes;
TYPED_TEST_SUITE(SharedMemoryRegionTypedTest, AllSharedMemoryRegionTypes);

TYPED_TEST(SharedMemoryRegionTypedTest, WriteAndRead) {
  const size_t size = 2314;
  auto [pre_pickle, pre_mapping] = base::CreateMappedRegion<TypeParam>(size);
  const size_t pre_size = pre_pickle.GetSize();

  const std::string content = "Hello, world!";
  UNSAFE_TODO(memcpy(pre_mapping.memory(), content.data(), content.size()));

  IPC::Message message;
  IPC::WriteParam(&message, pre_pickle);
  EXPECT_FALSE(pre_pickle.IsValid());

  TypeParam post_pickle;
  base::PickleIterator iter(message);
  EXPECT_TRUE(IPC::ReadParam(&message, &iter, &post_pickle));
  EXPECT_EQ(pre_size, post_pickle.GetSize());
  typename TypeParam::MappingType post_mapping = post_pickle.Map();
  EXPECT_EQ(pre_mapping.guid(), post_mapping.guid());
  UNSAFE_TODO(EXPECT_EQ(0, memcmp(pre_mapping.memory(), post_mapping.memory(),
                                  post_pickle.GetSize())));
}

TYPED_TEST(SharedMemoryRegionTypedTest, InvalidRegion) {
  TypeParam pre_pickle;
  EXPECT_FALSE(pre_pickle.IsValid());

  IPC::Message message;
  IPC::WriteParam(&message, pre_pickle);

  TypeParam post_pickle;
  base::PickleIterator iter(message);
  EXPECT_TRUE(IPC::ReadParam(&message, &iter, &post_pickle));
  EXPECT_FALSE(post_pickle.IsValid());
}

TEST(IPCMessageUtilsTest, UnguessableTokenTest) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  base::Pickle pickle;
  IPC::WriteParam(&pickle, token);

  base::UnguessableToken deserialized_token;
  base::PickleIterator iter(pickle);
  EXPECT_TRUE(IPC::ReadParam(&pickle, &iter, &deserialized_token));
  EXPECT_EQ(token, deserialized_token);
}

TEST(IPCMessageUtilsTest, FlatMap) {
  base::flat_map<std::string, int> input;
  input["foo"] = 42;
  input["bar"] = 96;

  base::Pickle pickle;
  IPC::WriteParam(&pickle, input);

  base::PickleIterator iter(pickle);
  base::flat_map<std::string, int> output;
  EXPECT_TRUE(IPC::ReadParam(&pickle, &iter, &output));

  EXPECT_EQ(input, output);
}

TEST(IPCMessageUtilsTest, StrongAlias) {
  using TestType = base::StrongAlias<class Tag, int>;
  TestType input(42);

  base::Pickle pickle;
  IPC::WriteParam(&pickle, input);

  base::PickleIterator iter(pickle);
  TestType output;
  EXPECT_TRUE(IPC::ReadParam(&pickle, &iter, &output));

  EXPECT_EQ(input, output);
}

TEST(IPCMessageUtilsTest, DictValueConversion) {
  base::Value::Dict dict_value;
  dict_value.Set("path1", 42);
  dict_value.Set("path2", 84);
  base::Value::List subvalue;
  subvalue.Append(1234);
  subvalue.Append(5678);
  dict_value.Set("path3", std::move(subvalue));

  IPC::Message message;
  ParamTraits<base::Value::Dict>::Write(&message, dict_value);

  base::PickleIterator iter(message);
  base::Value::Dict read_value;
  ASSERT_TRUE(
      ParamTraits<base::Value::Dict>::Read(&message, &iter, &read_value));
  EXPECT_EQ(dict_value, read_value);
}

TEST(IPCMessageUtilsTest, ListValueConversion) {
  base::Value::List list_value;
  list_value.Append(42);
  list_value.Append(84);

  IPC::Message message;
  ParamTraits<base::Value::List>::Write(&message, list_value);

  base::PickleIterator iter(message);
  base::Value::List read_value;
  ASSERT_TRUE(
      ParamTraits<base::Value::List>::Read(&message, &iter, &read_value));
  EXPECT_EQ(list_value, read_value);
}

#if BUILDFLAG(IS_WIN)
TEST(IPCMessageUtilsTest, ScopedHandle) {
  HANDLE raw_dupe_handle;
  ASSERT_TRUE(::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentProcess(),
                                ::GetCurrentProcess(), &raw_dupe_handle, 0,
                                FALSE, DUPLICATE_SAME_ACCESS));
  base::win::ScopedHandle dupe_handle(raw_dupe_handle);

  Message message;
  WriteParam(&message, dupe_handle);

  base::PickleIterator iter(message);
  base::win::ScopedHandle read_handle;
  EXPECT_TRUE(ReadParam(&message, &iter, &read_handle));
  EXPECT_TRUE(read_handle.is_valid());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace
}  // namespace IPC
