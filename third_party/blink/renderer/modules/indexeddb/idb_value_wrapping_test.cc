// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

TEST(IDBValueWrapperTest, WriteVarIntOneByte) {
  Vector<char> output;

  IDBValueWrapper::WriteVarInt(0, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x00', output.data()[0]);
  output.clear();

  IDBValueWrapper::WriteVarInt(1, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x01', output.data()[0]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x34, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x34', output.data()[0]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x7f, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x7f', output.data()[0]);
}

TEST(IDBValueWrapperTest, WriteVarIntMultiByte) {
  Vector<char> output;

  IDBValueWrapper::WriteVarInt(0xff, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\xff', output.data()[0]);
  EXPECT_EQ('\x01', output.data()[1]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x100, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\x80', output.data()[0]);
  EXPECT_EQ('\x02', output.data()[1]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x1234, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\xb4', output.data()[0]);
  EXPECT_EQ('\x24', output.data()[1]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0xabcd, output);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ('\xcd', output.data()[0]);
  EXPECT_EQ('\xd7', output.data()[1]);
  EXPECT_EQ('\x2', output.data()[2]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x123456, output);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ('\xd6', output.data()[0]);
  EXPECT_EQ('\xe8', output.data()[1]);
  EXPECT_EQ('\x48', output.data()[2]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0xabcdef, output);
  ASSERT_EQ(4U, output.size());
  EXPECT_EQ('\xef', output.data()[0]);
  EXPECT_EQ('\x9b', output.data()[1]);
  EXPECT_EQ('\xaf', output.data()[2]);
  EXPECT_EQ('\x05', output.data()[3]);
  output.clear();
}

TEST(IDBValueWrapperTest, WriteVarIntMultiByteEdgeCases) {
  Vector<char> output;

  IDBValueWrapper::WriteVarInt(0x80, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\x80', output.data()[0]);
  EXPECT_EQ('\x01', output.data()[1]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x3fff, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\xff', output.data()[0]);
  EXPECT_EQ('\x7f', output.data()[1]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x4000, output);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ('\x80', output.data()[0]);
  EXPECT_EQ('\x80', output.data()[1]);
  EXPECT_EQ('\x01', output.data()[2]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x1fffff, output);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ('\xff', output.data()[0]);
  EXPECT_EQ('\xff', output.data()[1]);
  EXPECT_EQ('\x7f', output.data()[2]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x200000, output);
  ASSERT_EQ(4U, output.size());
  EXPECT_EQ('\x80', output.data()[0]);
  EXPECT_EQ('\x80', output.data()[1]);
  EXPECT_EQ('\x80', output.data()[2]);
  EXPECT_EQ('\x01', output.data()[3]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0xfffffff, output);
  ASSERT_EQ(4U, output.size());
  EXPECT_EQ('\xff', output.data()[0]);
  EXPECT_EQ('\xff', output.data()[1]);
  EXPECT_EQ('\xff', output.data()[2]);
  EXPECT_EQ('\x7f', output.data()[3]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x10000000, output);
  ASSERT_EQ(5U, output.size());
  EXPECT_EQ('\x80', output.data()[0]);
  EXPECT_EQ('\x80', output.data()[1]);
  EXPECT_EQ('\x80', output.data()[2]);
  EXPECT_EQ('\x80', output.data()[3]);
  EXPECT_EQ('\x01', output.data()[4]);
  output.clear();

  // Maximum value of unsigned on 32-bit platforms.
  IDBValueWrapper::WriteVarInt(0xffffffff, output);
  ASSERT_EQ(5U, output.size());
  EXPECT_EQ('\xff', output.data()[0]);
  EXPECT_EQ('\xff', output.data()[1]);
  EXPECT_EQ('\xff', output.data()[2]);
  EXPECT_EQ('\xff', output.data()[3]);
  EXPECT_EQ('\x0f', output.data()[4]);
  output.clear();
}

TEST(IDBValueWrapperTest, WriteBytes) {
  Vector<char> output;

  Vector<uint8_t> empty;
  IDBValueWrapper::WriteBytes(empty, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x00', output.data()[0]);
  output.clear();

  Vector<uint8_t> one_char;
  one_char.Append("\x42", 1);
  IDBValueWrapper::WriteBytes(one_char, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\x01', output.data()[0]);
  EXPECT_EQ('\x42', output.data()[1]);
  output.clear();

  Vector<uint8_t> long_vector;
  for (int i = 0; i < 256; ++i)
    long_vector.push_back(static_cast<uint8_t>(i));
  IDBValueWrapper::WriteBytes(long_vector, output);
  ASSERT_EQ(258U, output.size());
  EXPECT_EQ('\x80', output.data()[0]);
  EXPECT_EQ('\x02', output.data()[1]);
  EXPECT_TRUE(std::equal(long_vector.begin(), long_vector.end(),
                         reinterpret_cast<const uint8_t*>(output.data() + 2)));
  output.clear();
}

// Friend class of IDBValueUnwrapper with access to its internals.
class IDBValueUnwrapperReadTestHelper {
  STACK_ALLOCATED();

 public:
  void ReadVarInt(const char* start, uint32_t buffer_size) {
    IDBValueUnwrapper unwrapper;

    const uint8_t* buffer_start = reinterpret_cast<const uint8_t*>(start);
    const uint8_t* buffer_end = buffer_start + buffer_size;
    unwrapper.current_ = buffer_start;
    unwrapper.end_ = buffer_end;
    success_ = unwrapper.ReadVarInt(read_varint_);

    ASSERT_EQ(unwrapper.end_, buffer_end)
        << "ReadVarInt should not change end_";
    ASSERT_LE(unwrapper.current_, unwrapper.end_)
        << "ReadVarInt should not move current_ past end_";
    consumed_bytes_ = static_cast<uint32_t>(unwrapper.current_ - buffer_start);
  }

  void ReadBytes(const char* start, uint32_t buffer_size) {
    IDBValueUnwrapper unwrapper;

    const uint8_t* buffer_start = reinterpret_cast<const uint8_t*>(start);
    const uint8_t* buffer_end = buffer_start + buffer_size;
    unwrapper.current_ = buffer_start;
    unwrapper.end_ = buffer_end;
    success_ = unwrapper.ReadBytes(read_bytes_);

    ASSERT_EQ(unwrapper.end_, buffer_end) << "ReadBytes should not change end_";
    ASSERT_LE(unwrapper.current_, unwrapper.end_)
        << "ReadBytes should not move current_ past end_";
    consumed_bytes_ = static_cast<uint32_t>(unwrapper.current_ - buffer_start);
  }

  bool success() { return success_; }
  unsigned consumed_bytes() { return consumed_bytes_; }
  unsigned read_varint() { return read_varint_; }
  const Vector<uint8_t>& read_bytes() { return read_bytes_; }

 private:
  bool success_;
  unsigned consumed_bytes_;
  unsigned read_varint_;
  Vector<uint8_t> read_bytes_;
};

TEST(IDBValueUnwrapperTest, ReadVarIntOneByte) {
  IDBValueUnwrapperReadTestHelper helper;

  // Most test cases have an extra byte at the end of the input to verify that
  // the parser doesn't consume too much data.

  helper.ReadVarInt("\x00\x01", 2);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_varint());
  EXPECT_EQ(1U, helper.consumed_bytes());

  helper.ReadVarInt("\x01\x01", 2);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(1U, helper.read_varint());
  EXPECT_EQ(1U, helper.consumed_bytes());

  helper.ReadVarInt("\x7f\x01", 2);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x7fU, helper.read_varint());
  EXPECT_EQ(1U, helper.consumed_bytes());

  helper.ReadVarInt("\x7f\x01", 1);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x7fU, helper.read_varint());
  EXPECT_EQ(1U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadVarIntMultiBytes) {
  IDBValueUnwrapperReadTestHelper helper;

  helper.ReadVarInt("\xff\x01\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xffU, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt("\x80\x02\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x100U, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt("\xb4\x24\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x1234U, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt("\xcd\xd7\x02\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xabcdU, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt("\xd6\xe8\x48\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x123456U, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt("\xd6\xe8\x48\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x123456U, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt("\xef\x9b\xaf\x05\x01", 5);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xabcdefU, helper.read_varint());
  EXPECT_EQ(4U, helper.consumed_bytes());

  helper.ReadVarInt("\xef\x9b\xaf\x05\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xabcdefU, helper.read_varint());
  EXPECT_EQ(4U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadVarIntMultiByteEdgeCases) {
  IDBValueUnwrapperReadTestHelper helper;

  helper.ReadVarInt("\x80\x01\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x80U, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt("\xff\x7f\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x3fffU, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt("\x80\x80\x01\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x4000U, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt("\xff\xff\x7f\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x1fffffU, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt("\x80\x80\x80\x01\x01", 5);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x200000U, helper.read_varint());
  EXPECT_EQ(4U, helper.consumed_bytes());

  helper.ReadVarInt("\xff\xff\xff\x7f\x01", 5);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xfffffffU, helper.read_varint());
  EXPECT_EQ(4U, helper.consumed_bytes());

  helper.ReadVarInt("\x80\x80\x80\x80\x01\x01", 6);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x10000000U, helper.read_varint());
  EXPECT_EQ(5U, helper.consumed_bytes());

  helper.ReadVarInt("\xff\xff\xff\xff\x0f\x01", 6);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xffffffffU, helper.read_varint());
  EXPECT_EQ(5U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadVarIntTruncatedInput) {
  IDBValueUnwrapperReadTestHelper helper;

  helper.ReadVarInt("\x01", 0);
  EXPECT_FALSE(helper.success());

  helper.ReadVarInt("\x80\x01", 1);
  EXPECT_FALSE(helper.success());

  helper.ReadVarInt("\xff\x01", 1);
  EXPECT_FALSE(helper.success());

  helper.ReadVarInt("\x80\x80\x01", 2);
  EXPECT_FALSE(helper.success());

  helper.ReadVarInt("\xff\xff\x01", 2);
  EXPECT_FALSE(helper.success());

  helper.ReadVarInt("\x80\x80\x80\x80\x01", 4);
  EXPECT_FALSE(helper.success());

  helper.ReadVarInt("\xff\xff\xff\xff\x01", 4);
  EXPECT_FALSE(helper.success());
}

TEST(IDBValueUnwrapperTest, ReadVarIntDenormalizedInput) {
  IDBValueUnwrapperReadTestHelper helper;

  helper.ReadVarInt("\x80\x00\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt("\xff\x00\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x7fU, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt("\x80\x80\x00\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt("\x80\xff\x00\x01", 4);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x3f80U, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt("\x80\xff\x80\xff\x00\x01", 6);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x0fe03f80U, helper.read_varint());
  EXPECT_EQ(5U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, WriteVarIntMaxUnsignedRoundtrip) {
  unsigned max_value = std::numeric_limits<unsigned>::max();
  Vector<char> output;
  IDBValueWrapper::WriteVarInt(max_value, output);

  IDBValueUnwrapperReadTestHelper helper;
  helper.ReadVarInt(output.data(), output.size());
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(max_value, helper.read_varint());
  EXPECT_EQ(output.size(), helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadBytes) {
  IDBValueUnwrapperReadTestHelper helper;

  // Most test cases have an extra byte at the end of the input to verify that
  // the parser doesn't consume too much data.

  helper.ReadBytes("\x00\x01", 2);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_bytes().size());
  EXPECT_EQ(1U, helper.consumed_bytes());

  helper.ReadBytes("\x01\x42\x01", 3);
  EXPECT_TRUE(helper.success());
  ASSERT_EQ(1U, helper.read_bytes().size());
  EXPECT_EQ('\x42', helper.read_bytes().data()[0]);
  EXPECT_EQ(2U, helper.consumed_bytes());

  Vector<uint8_t> long_output;
  long_output.push_back(0x80);
  long_output.push_back(0x02);
  for (int i = 0; i < 256; ++i)
    long_output.push_back(static_cast<unsigned char>(i));
  long_output.push_back(0x01);
  helper.ReadBytes(reinterpret_cast<char*>(long_output.data()),
                   long_output.size());
  EXPECT_TRUE(helper.success());
  ASSERT_EQ(256U, helper.read_bytes().size());
  ASSERT_EQ(long_output.size() - 1, helper.consumed_bytes());
  EXPECT_TRUE(std::equal(helper.read_bytes().begin(), helper.read_bytes().end(),
                         long_output.data() + 2));

  helper.ReadBytes("\x01\x42\x01", 2);
  EXPECT_TRUE(helper.success());
  ASSERT_EQ(1U, helper.read_bytes().size());
  EXPECT_EQ('\x42', helper.read_bytes().data()[0]);
  EXPECT_EQ(2U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadBytesTruncatedInput) {
  IDBValueUnwrapperReadTestHelper helper;

  helper.ReadBytes("\x01\x42", 0);
  EXPECT_FALSE(helper.success());

  helper.ReadBytes("\x01\x42", 1);
  EXPECT_FALSE(helper.success());

  helper.ReadBytes("\x03\x42\x42\x42", 3);
  EXPECT_FALSE(helper.success());
}

TEST(IDBValueUnwrapperTest, ReadBytesDenormalizedInput) {
  IDBValueUnwrapperReadTestHelper helper;

  helper.ReadBytes("\x80\x00\x01", 3);
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_bytes().size());
  EXPECT_EQ(2U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, IsWrapped) {
  V8TestingScope scope;
  NonThrowableExceptionState non_throwable_exception_state;
  v8::Local<v8::Value> v8_true = v8::True(scope.GetIsolate());
  IDBValueWrapper wrapper(scope.GetIsolate(), v8_true,
                          SerializedScriptValue::SerializeOptions::kSerialize,
                          non_throwable_exception_state);
  wrapper.DoneCloning();
  wrapper.WrapIfBiggerThan(0);
  Vector<scoped_refptr<BlobDataHandle>> blob_data_handles =
      wrapper.TakeBlobDataHandles();
  Vector<WebBlobInfo> blob_infos = wrapper.TakeBlobInfo();
  scoped_refptr<SharedBuffer> wrapped_marker_buffer = wrapper.TakeWireBytes();
  IDBKeyPath key_path(String("primaryKey"));

  Vector<char> wrapped_marker_bytes(
      static_cast<wtf_size_t>(wrapped_marker_buffer->size()));
  ASSERT_TRUE(wrapped_marker_buffer->GetBytes(wrapped_marker_bytes.data(),
                                              wrapped_marker_bytes.size()));

  auto wrapped_value = std::make_unique<IDBValue>(
      std::move(wrapped_marker_buffer), std::move(blob_infos));
  wrapped_value->SetIsolate(scope.GetIsolate());
  EXPECT_TRUE(IDBValueUnwrapper::IsWrapped(wrapped_value.get()));

  // IsWrapped() looks at the first 3 bytes in the value's byte array.
  // Truncating the array to fewer than 3 bytes should cause IsWrapped() to
  // return false.
  ASSERT_LT(3U, wrapped_marker_bytes.size());
  for (wtf_size_t i = 0; i < 3; ++i) {
    auto mutant_value = std::make_unique<IDBValue>(
        SharedBuffer::Create(wrapped_marker_bytes.data(), i),
        std::move(blob_infos));
    mutant_value->SetIsolate(scope.GetIsolate());

    EXPECT_FALSE(IDBValueUnwrapper::IsWrapped(mutant_value.get()));
  }

  // IsWrapped() looks at the first 3 bytes in the value. Flipping any bit in
  // these 3 bytes should cause IsWrapped() to return false.
  ASSERT_LT(3U, wrapped_marker_bytes.size());
  for (wtf_size_t i = 0; i < 3; ++i) {
    for (int j = 0; j < 8; ++j) {
      char mask = 1 << j;
      wrapped_marker_bytes[i] ^= mask;
      auto mutant_value = std::make_unique<IDBValue>(
          SharedBuffer::Create(wrapped_marker_bytes.data(),
                               wrapped_marker_bytes.size()),
          std::move(blob_infos));
      mutant_value->SetIsolate(scope.GetIsolate());
      EXPECT_FALSE(IDBValueUnwrapper::IsWrapped(mutant_value.get()));

      wrapped_marker_bytes[i] ^= mask;
    }
  }
}

}  // namespace blink
