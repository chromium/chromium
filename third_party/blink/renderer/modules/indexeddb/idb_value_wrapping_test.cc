// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialization_tag.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

TEST(IDBValueWrapperTest, WriteVarIntOneByte) {
  Vector<char> output;

  IDBValueWrapper::WriteVarInt(0, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x00', output[0]);
  output.clear();

  IDBValueWrapper::WriteVarInt(1, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x01', output[0]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x34, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x34', output[0]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x7f, output);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ('\x7f', output[0]);
}

TEST(IDBValueWrapperTest, WriteVarIntMultiByte) {
  Vector<char> output;

  IDBValueWrapper::WriteVarInt(0xff, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\xff', output[0]);
  EXPECT_EQ('\x01', output[1]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x100, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\x80', output[0]);
  EXPECT_EQ('\x02', output[1]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x1234, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\xb4', output[0]);
  EXPECT_EQ('\x24', output[1]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0xabcd, output);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ('\xcd', output[0]);
  EXPECT_EQ('\xd7', output[1]);
  EXPECT_EQ('\x2', output[2]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x123456, output);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ('\xd6', output[0]);
  EXPECT_EQ('\xe8', output[1]);
  EXPECT_EQ('\x48', output[2]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0xabcdef, output);
  ASSERT_EQ(4U, output.size());
  EXPECT_EQ('\xef', output[0]);
  EXPECT_EQ('\x9b', output[1]);
  EXPECT_EQ('\xaf', output[2]);
  EXPECT_EQ('\x05', output[3]);
  output.clear();
}

TEST(IDBValueWrapperTest, WriteVarIntMultiByteEdgeCases) {
  Vector<char> output;

  IDBValueWrapper::WriteVarInt(0x80, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\x80', output[0]);
  EXPECT_EQ('\x01', output[1]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x3fff, output);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ('\xff', output[0]);
  EXPECT_EQ('\x7f', output[1]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x4000, output);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ('\x80', output[0]);
  EXPECT_EQ('\x80', output[1]);
  EXPECT_EQ('\x01', output[2]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x1fffff, output);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ('\xff', output[0]);
  EXPECT_EQ('\xff', output[1]);
  EXPECT_EQ('\x7f', output[2]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x200000, output);
  ASSERT_EQ(4U, output.size());
  EXPECT_EQ('\x80', output[0]);
  EXPECT_EQ('\x80', output[1]);
  EXPECT_EQ('\x80', output[2]);
  EXPECT_EQ('\x01', output[3]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0xfffffff, output);
  ASSERT_EQ(4U, output.size());
  EXPECT_EQ('\xff', output[0]);
  EXPECT_EQ('\xff', output[1]);
  EXPECT_EQ('\xff', output[2]);
  EXPECT_EQ('\x7f', output[3]);
  output.clear();

  IDBValueWrapper::WriteVarInt(0x10000000, output);
  ASSERT_EQ(5U, output.size());
  EXPECT_EQ('\x80', output[0]);
  EXPECT_EQ('\x80', output[1]);
  EXPECT_EQ('\x80', output[2]);
  EXPECT_EQ('\x80', output[3]);
  EXPECT_EQ('\x01', output[4]);
  output.clear();

  // Maximum value of unsigned on 32-bit platforms.
  IDBValueWrapper::WriteVarInt(0xffffffff, output);
  ASSERT_EQ(5U, output.size());
  EXPECT_EQ('\xff', output[0]);
  EXPECT_EQ('\xff', output[1]);
  EXPECT_EQ('\xff', output[2]);
  EXPECT_EQ('\xff', output[3]);
  EXPECT_EQ('\x0f', output[4]);
  output.clear();
}

// Friend class of IDBValueUnwrapper with access to its internals.
class IDBValueUnwrapperReadTestHelper {
  STACK_ALLOCATED();

 public:
  void ReadVarInt(base::span<const uint8_t> parse_span) {
    IDBValueUnwrapper unwrapper;

    unwrapper.parse_span_ = parse_span;
    success_ = unwrapper.ReadVarInt(read_varint_);

    if (!unwrapper.parse_span_.empty()) {
      ASSERT_EQ(&unwrapper.parse_span_.back(), &parse_span.back())
          << "ReadVarInt should not change end of buffer";
    }
    consumed_bytes_ = parse_span.size() - unwrapper.parse_span_.size();
  }

  void ReadBytes(base::span<const uint8_t> parse_span) {
    IDBValueUnwrapper unwrapper;

    unwrapper.parse_span_ = parse_span;
    success_ = unwrapper.ReadBytes(read_bytes_);

    if (!unwrapper.parse_span_.empty()) {
      ASSERT_EQ(&unwrapper.parse_span_.back(), &parse_span.back())
          << "ReadBytes should not change end of buffer";
    }
    consumed_bytes_ = parse_span.size() - unwrapper.parse_span_.size();
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

  helper.ReadVarInt({0x00, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_varint());
  EXPECT_EQ(1U, helper.consumed_bytes());

  helper.ReadVarInt({0x01, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(1U, helper.read_varint());
  EXPECT_EQ(1U, helper.consumed_bytes());

  helper.ReadVarInt({0x7f, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x7fU, helper.read_varint());
  EXPECT_EQ(1U, helper.consumed_bytes());

  helper.ReadVarInt({0x7f});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x7fU, helper.read_varint());
  EXPECT_EQ(1U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadVarIntMultiBytes) {
  IDBValueUnwrapperReadTestHelper helper;

  helper.ReadVarInt({0xff, 0x01, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xffU, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt({0x80, 0x02, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x100U, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt({0xb4, 0x24, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x1234U, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt({0xcd, 0xd7, 0x02, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xabcdU, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt({0xd6, 0xe8, 0x48, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x123456U, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt({0xd6, 0xe8, 0x48});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x123456U, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt({0xef, 0x9b, 0xaf, 0x05, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xabcdefU, helper.read_varint());
  EXPECT_EQ(4U, helper.consumed_bytes());

  helper.ReadVarInt({0xef, 0x9b, 0xaf, 0x05});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xabcdefU, helper.read_varint());
  EXPECT_EQ(4U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadVarIntMultiByteEdgeCases) {
  IDBValueUnwrapperReadTestHelper helper;

  helper.ReadVarInt({0x80, 0x01, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x80U, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt({0xff, 0x7f, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x3fffU, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt({0x80, 0x80, 0x01, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x4000U, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt({0xff, 0xff, 0x7f, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x1fffffU, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt({0x80, 0x80, 0x80, 0x01, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x200000U, helper.read_varint());
  EXPECT_EQ(4U, helper.consumed_bytes());

  helper.ReadVarInt({0xff, 0xff, 0xff, 0x7f, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xfffffffU, helper.read_varint());
  EXPECT_EQ(4U, helper.consumed_bytes());

  helper.ReadVarInt({0x80, 0x80, 0x80, 0x80, 0x01, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x10000000U, helper.read_varint());
  EXPECT_EQ(5U, helper.consumed_bytes());

  helper.ReadVarInt({0xff, 0xff, 0xff, 0xff, 0x0f, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0xffffffffU, helper.read_varint());
  EXPECT_EQ(5U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadVarIntTruncatedInput) {
  IDBValueUnwrapperReadTestHelper helper;

  helper.ReadVarInt({});
  EXPECT_FALSE(helper.success());

  helper.ReadVarInt({0x80});
  EXPECT_FALSE(helper.success());

  helper.ReadVarInt({0xff});
  EXPECT_FALSE(helper.success());

  helper.ReadVarInt({0x80, 0x80});
  EXPECT_FALSE(helper.success());

  helper.ReadVarInt({0xff, 0xff});
  EXPECT_FALSE(helper.success());

  helper.ReadVarInt({0x80, 0x80, 0x80, 0x80});
  EXPECT_FALSE(helper.success());

  helper.ReadVarInt({0xff, 0xff, 0xff, 0xff});
  EXPECT_FALSE(helper.success());
}

TEST(IDBValueUnwrapperTest, ReadVarIntDenormalizedInput) {
  IDBValueUnwrapperReadTestHelper helper;

  helper.ReadVarInt({0x80, 0x00, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt({0xff, 0x00, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x7fU, helper.read_varint());
  EXPECT_EQ(2U, helper.consumed_bytes());

  helper.ReadVarInt({0x80, 0x80, 0x00, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt({0x80, 0xff, 0x00, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x3f80U, helper.read_varint());
  EXPECT_EQ(3U, helper.consumed_bytes());

  helper.ReadVarInt({0x80, 0xff, 0x80, 0xff, 0x00, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0x0fe03f80U, helper.read_varint());
  EXPECT_EQ(5U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, WriteVarIntMaxUnsignedRoundtrip) {
  unsigned max_value = std::numeric_limits<unsigned>::max();
  Vector<char> output;
  IDBValueWrapper::WriteVarInt(max_value, output);

  IDBValueUnwrapperReadTestHelper helper;
  helper.ReadVarInt(base::as_bytes(base::span(output)));
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(max_value, helper.read_varint());
  EXPECT_EQ(output.size(), helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadBytes) {
  IDBValueUnwrapperReadTestHelper helper;

  // Most test cases have an extra byte at the end of the input to verify that
  // the parser doesn't consume too much data.

  helper.ReadBytes({0x00, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_bytes().size());
  EXPECT_EQ(1U, helper.consumed_bytes());

  helper.ReadBytes({0x01, 0x42, 0x01});
  EXPECT_TRUE(helper.success());
  ASSERT_EQ(1U, helper.read_bytes().size());
  EXPECT_EQ('\x42', helper.read_bytes()[0]);
  EXPECT_EQ(2U, helper.consumed_bytes());

  Vector<uint8_t> long_output;
  long_output.push_back(0x80);
  long_output.push_back(0x02);
  for (int i = 0; i < 256; ++i)
    long_output.push_back(static_cast<unsigned char>(i));
  long_output.push_back(0x01);
  helper.ReadBytes(long_output);
  EXPECT_TRUE(helper.success());
  ASSERT_EQ(256U, helper.read_bytes().size());
  ASSERT_EQ(long_output.size() - 1, helper.consumed_bytes());
  EXPECT_EQ(base::span(helper.read_bytes()),
            (base::span(long_output).subspan<2, 256>()));

  helper.ReadBytes({0x01, 0x42});
  EXPECT_TRUE(helper.success());
  ASSERT_EQ(1U, helper.read_bytes().size());
  EXPECT_EQ('\x42', helper.read_bytes()[0]);
  EXPECT_EQ(2U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, ReadBytesTruncatedInput) {
  IDBValueUnwrapperReadTestHelper helper;

  helper.ReadBytes({});
  EXPECT_FALSE(helper.success());

  helper.ReadBytes({0x01});
  EXPECT_FALSE(helper.success());

  helper.ReadBytes({0x03, 0x42, 0x42});
  EXPECT_FALSE(helper.success());
}

TEST(IDBValueUnwrapperTest, ReadBytesDenormalizedInput) {
  IDBValueUnwrapperReadTestHelper helper;

  helper.ReadBytes({0x80, 0x00, 0x01});
  EXPECT_TRUE(helper.success());
  EXPECT_EQ(0U, helper.read_bytes().size());
  EXPECT_EQ(2U, helper.consumed_bytes());
}

TEST(IDBValueUnwrapperTest, IsWrapped) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  NonThrowableExceptionState non_throwable_exception_state;
  v8::Local<v8::Value> v8_true = v8::True(scope.GetIsolate());
  IDBValueWrapper wrapper(scope.GetIsolate(), v8_true,
                          SerializedScriptValue::SerializeOptions::kSerialize,
                          non_throwable_exception_state,
                          /*backend_uses_sqlite=*/false);
  wrapper.set_wrapping_threshold_for_test(0);
  wrapper.DoneCloning();

  std::unique_ptr<IDBValue> wrapped_value = std::move(wrapper).Build();

  const Vector<char> wrapped_marker_bytes(wrapped_value->Data());

  wrapped_value->SetIsolate(scope.GetIsolate());
  EXPECT_TRUE(IDBValueUnwrapper::IsWrapped(wrapped_value.get()));

  // IsWrapped() looks at the first 3 bytes in the value's byte array.
  // Truncating the array to fewer than 3 bytes should cause IsWrapped() to
  // return false.
  ASSERT_LT(3U, wrapped_marker_bytes.size());
  for (wtf_size_t i = 0; i < 3; ++i) {
    IDBValue mutant_value;
    mutant_value.SetData(
        Vector<char>(base::span(wrapped_marker_bytes).first(i)));
    mutant_value.SetIsolate(scope.GetIsolate());

    EXPECT_FALSE(IDBValueUnwrapper::IsWrapped(&mutant_value));
  }

  // IsWrapped() looks at the first 3 bytes in the value. Flipping any bit in
  // these 3 bytes should cause IsWrapped() to return false.
  ASSERT_LT(3U, wrapped_marker_bytes.size());
  for (wtf_size_t i = 0; i < 3; ++i) {
    for (int j = 0; j < 8; ++j) {
      char mask = 1 << j;
      Vector<char> copy = wrapped_marker_bytes;
      copy[i] ^= mask;
      IDBValue mutant_value;
      mutant_value.SetData(std::move(copy));
      mutant_value.SetIsolate(scope.GetIsolate());
      EXPECT_FALSE(IDBValueUnwrapper::IsWrapped(&mutant_value));
    }
  }
}

TEST(IDBValueUnwrapperTest, SqliteDoesntWrapOrCompress) {
  for (const bool use_sqlite : {true, false}) {
    test::TaskEnvironment task_environment;
    V8TestingScope scope;
    NonThrowableExceptionState non_throwable_exception_state;
    v8::Local<v8::Value> v8_value =
        v8::String::NewFromUtf8(
            scope.GetIsolate(),
            base::StrCat(std::vector<std::string>(500, "abcd")).c_str(),
            v8::NewStringType::kNormal)
            .ToLocalChecked();
    IDBValueWrapper wrapper(scope.GetIsolate(), v8_value,
                            SerializedScriptValue::SerializeOptions::kSerialize,
                            non_throwable_exception_state, use_sqlite);
    wrapper.set_wrapping_threshold_for_test(0);
    wrapper.set_compression_threshold_for_test(0);
    wrapper.DoneCloning();

    std::unique_ptr<IDBValue> wrapped_value = std::move(wrapper).Build();

    wrapped_value->SetIsolate(scope.GetIsolate());
    EXPECT_NE(use_sqlite, IDBValueUnwrapper::IsWrapped(wrapped_value.get()));

    auto is_compressed = [](base::span<const uint8_t> data) {
      return data.size() >= 3 && data[0] == kVersionTag && data[1] == 0x11 &&
             data[2] == 2;
    };
    // Even with compression enabled, the data won't look compressed if it's
    // wrapped.
    EXPECT_FALSE(is_compressed(wrapped_value->Data()));
  }
}

TEST(IDBValueUnwrapperTest, Compression) {
  test::TaskEnvironment task_environment;

  struct {
    bool should_compress;
    std::string bytes;
    int32_t compression_threshold;
    // Wrapping threshold is tested here to ensure it does not interfere
    // with the compression threshold.
    int32_t wrapping_threshold;
  } test_cases[] = {
      {false,
       "abcdefghijcklmnopqrstuvwxyz123456789?/"
       ".,'[]!@#$%^&*(&)asjdflkajnwefkajwneflkacoiw93lkm",
       /* compression_threshold = */ 0, /*wrapping_threshold = */ 500},
      {false, base::StrCat(std::vector<std::string>(100u, "abcd")),
       /* compression_threshold = */ 500, /*wrapping_threshold = */ 500},
      {true, base::StrCat(std::vector<std::string>(500, "abcd")),
       /* compression_threshold = */ 500, /*wrapping_threshold = */ 500},
      {true, base::StrCat(std::vector<std::string>(500, "abcd")),
       /* compression_threshold = */ 500, /*wrapping_threshold = */ 400},
      {true, base::StrCat(std::vector<std::string>(500, "abcd")),
       /* compression_threshold = */ 500, /*wrapping_threshold = */ 600}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << "Testing string " << test_case.bytes);

    base::test::ScopedFeatureList enable_feature_list;
    enable_feature_list.InitAndEnableFeatureWithParameters(
        features::kIndexedDBCompressValuesWithSnappy,
        {{"compression-threshold",
          base::StringPrintf("%i", test_case.compression_threshold)}});

    V8TestingScope scope;
    NonThrowableExceptionState non_throwable_exception_state;
    v8::Local<v8::Value> v8_value =
        v8::String::NewFromUtf8(scope.GetIsolate(), test_case.bytes.c_str(),
                                v8::NewStringType::kNormal)
            .ToLocalChecked();
    IDBValueWrapper wrapper(scope.GetIsolate(), v8_value,
                            SerializedScriptValue::SerializeOptions::kSerialize,
                            non_throwable_exception_state,
                            /*backend_uses_sqlite=*/false);
    wrapper.set_wrapping_threshold_for_test(test_case.wrapping_threshold);
    wrapper.set_compression_threshold_for_test(test_case.compression_threshold);
    wrapper.DoneCloning();

    std::unique_ptr<IDBValue> value = std::move(wrapper).Build();

    // Verify whether the serialized bytes show the compression marker.
    base::span<const uint8_t> serialized_bytes = value->Data();
    ASSERT_GT(serialized_bytes.size(), 3u);
    if (test_case.should_compress) {
      EXPECT_EQ(serialized_bytes[0], kVersionTag);
      EXPECT_EQ(serialized_bytes[1], 0x11);
      EXPECT_EQ(serialized_bytes[2], 2);
    }

    {
      // Verify whether the decompressed bytes show the standard serialization
      // marker.
      SerializedScriptValue::DataBufferPtr decompressed;
      ASSERT_EQ(
          test_case.should_compress,
          IDBValueUnwrapper::Decompress(value->Data(), nullptr, &decompressed));
    }

    // Round trip to v8 value.
    value->SetIsolate(scope.GetIsolate());
    auto serialized_string = value->CreateSerializedValue();
    EXPECT_TRUE(serialized_string->Deserialize(scope.GetIsolate())
                    ->StrictEquals(v8_value));

    {
      // The data in `value` is still compressed after
      // `CreateSerializedValue()`.
      SerializedScriptValue::DataBufferPtr decompressed;
      ASSERT_EQ(
          test_case.should_compress,
          IDBValueUnwrapper::Decompress(value->Data(), nullptr, &decompressed));
    }
  }
}

// Verifies that the decompression code should still run and succeed on
// compressed data even if the flag is disabled. This is required to be able to
// decompress existing data that has been persisted to disk if/when compression
// is later disabled.
TEST(IDBValueUnwrapperTest, Decompression) {
  test::TaskEnvironment task_environment;
  Vector<WebBlobInfo> blob_infos;
  Vector<char> buffer;
  std::unique_ptr<IDBValue> value;
  V8TestingScope scope;
  v8::Local<v8::Value> v8_value;
  {
    base::test::ScopedFeatureList enable_feature_list{
        features::kIndexedDBCompressValuesWithSnappy};
    NonThrowableExceptionState non_throwable_exception_state;
    std::string bytes = base::StrCat(std::vector<std::string>(100u, "abcd"));
    v8_value = v8::String::NewFromUtf8(scope.GetIsolate(), bytes.c_str(),
                                       v8::NewStringType::kNormal)
                   .ToLocalChecked();
    IDBValueWrapper wrapper(scope.GetIsolate(), v8_value,
                            SerializedScriptValue::SerializeOptions::kSerialize,
                            non_throwable_exception_state,
                            /*backend_uses_sqlite=*/false);
    wrapper.DoneCloning();
    value = std::move(wrapper).Build();
  }

  {
    base::test::ScopedFeatureList disable_feature_list;
    disable_feature_list.InitAndDisableFeature(
        features::kIndexedDBCompressValuesWithSnappy);
    EXPECT_FALSE(base::FeatureList::IsEnabled(
        features::kIndexedDBCompressValuesWithSnappy));

    // Complete round trip to v8 value with compression disabled.
    value->SetIsolate(scope.GetIsolate());
    auto serialized_string = value->CreateSerializedValue();
    EXPECT_TRUE(serialized_string->Deserialize(scope.GetIsolate())
                    ->StrictEquals(v8_value));
  }
}

}  // namespace blink
