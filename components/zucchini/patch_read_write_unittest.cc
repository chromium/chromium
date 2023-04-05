// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/patch_reader.h"
#include "components/zucchini/patch_writer.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "components/zucchini/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

// Used for initialization of raw test data.
using ByteVector = std::vector<uint8_t>;

// Helper function that creates an object of type |T| and intializes it from
// data in |buffer|. Ensures initialization is successful. |buffer| is passed as
// pointer to avoid passing a temporay, which can causes dangling references.
template <class T>
T TestInitialize(const ByteVector* buffer) {
  T value;
  BufferSource buffer_source(buffer->data(), buffer->size());
  EXPECT_TRUE(value.Initialize(&buffer_source));
  EXPECT_TRUE(buffer_source.empty());  // Make sure all data has been consumed
  return value;
}

// Helper function that creates an object of type |T| and tries to intialize it
// from invalid data in |buffer|, expecting the operation to fail. |buffer| is
// passed as pointer to avoid passing a temporary, which can causes dangling
// references.
template <class T>
void TestInvalidInitialize(const ByteVector* buffer) {
  T value;
  BufferSource buffer_source(buffer->data(), buffer->size());
  EXPECT_FALSE(value.Initialize(&buffer_source));
}

// Helper function that serializes |value| into a buffer. Ensures that
// serialization is successful and that the result matches |expected|.
template <class T>
void TestSerialize(const ByteVector& expected, const T& value) {
  size_t size = value.SerializedSize();
  EXPECT_EQ(expected.size(), size);
  ByteVector buffer(size);
  BufferSink buffer_sink(buffer.data(), buffer.size());
  EXPECT_TRUE(value.SerializeInto(&buffer_sink));
  EXPECT_EQ(expected, buffer);
}

ByteVector CreatePatchElement() {
  return {
      // PatchElementHeader
      0x01, 0, 0, 0,       // old_offset
      0x51, 0, 0, 0,       // old_length
      0x03, 0, 0, 0,       // new_offset
      0x13, 0, 0, 0,       // new_length
      'P', 'x', '8', '6',  // exe_type = EXE_TYPE_WIN32_X86
      0x01, 0x00,          // element version
      // EquivalenceSource
      1, 0, 0, 0,  // src_skip size
      0x10,        // src_skip content
      1, 0, 0, 0,  // dst_skip size
      0x00,        // dst_skip content
      1, 0, 0, 0,  // copy_count size
      0x12,        // copy_count content
      // ExtraDataSource
      1, 0, 0, 0,  // extra_data size
      0x13,        // extra_data content
      // RawDeltaSource
      1, 0, 0, 0,  // raw_delta_skip size
      0x14,        // raw_delta_skip content
      1, 0, 0, 0,  // raw_delta_diff size
      0x15,        // raw_delta_diff content
      // ReferenceDeltaSource
      1, 0, 0, 0,  // reference_delta size
      0x16,        // reference_delta content
      // PatchElementReader
      2, 0, 0, 0,  // pool count
      0,           // pool_tag
      1, 0, 0, 0,  // extra_targets size
      0x17,        // extra_targets content
      2,           // pool_tag
      1, 0, 0, 0,  // extra_targets size
      0x18,        // extra_targets content
  };
}

ByteVector CreateElementMatch() {
  return {
      // PatchElementHeader
      0x01, 0,    0,   0,    // old_offset
      0x02, 0,    0,   0,    // old_length
      0x03, 0,    0,   0,    // new_offset
      0x04, 0,    0,   0,    // new_length
      'D',  'E',  'X', ' ',  // exe_type = kExeTypeDex
      0x01, 0x00,            // element version
  };
}

// Helper to mutate test |data| (e.g., from CreatePatchElement()) at |idx| from
// |from_val| (as sanity check) to |to_val|.
void ModifyByte(size_t idx,
                uint8_t from_val,
                uint8_t to_val,
                std::vector<uint8_t>* data) {
  ASSERT_EQ(from_val, (*data)[idx]);
  (*data)[idx] = to_val;
}

}  // namespace

bool operator==(const ByteVector& a, ConstBufferView b) {
  return a == ByteVector(b.begin(), b.end());
}

TEST(PatchTest, ParseSerializeElementMatch) {
  ByteVector data = CreateElementMatch();
  BufferSource buffer_source(data.data(), data.size());
  ElementMatch element_match = {};
  EXPECT_TRUE(patch::ParseElementMatch(&buffer_source, &element_match));
  EXPECT_EQ(kExeTypeDex, element_match.exe_type());
  EXPECT_EQ(kExeTypeDex, element_match.old_element.exe_type);
  EXPECT_EQ(kExeTypeDex, element_match.new_element.exe_type);
  EXPECT_EQ(0x1U, element_match.old_element.offset);
  EXPECT_EQ(0x2U, element_match.old_element.size);
  EXPECT_EQ(0x3U, element_match.new_element.offset);
  EXPECT_EQ(0x4U, element_match.new_element.size);

  size_t size = patch::SerializedElementMatchSize(element_match);
  EXPECT_EQ(data.size(), size);
  ByteVector buffer(size);
  BufferSink buffer_sink(buffer.data(), buffer.size());
  EXPECT_TRUE(patch::SerializeElementMatch(element_match, &buffer_sink));
  EXPECT_EQ(data, buffer);
}

TEST(PatchTest, ParseElementMatchTooSmall) {
  ByteVector data = {4};
  BufferSource buffer_source(data.data(), data.size());
  ElementMatch element_match = {};
  EXPECT_FALSE(patch::ParseElementMatch(&buffer_source, &element_match));
}

TEST(PatchTest, ParseElementMatchNoLength) {
  // Set old_length to 0 to trigger an error.
  {
    ByteVector data = CreateElementMatch();
    // old_length := 0.
    ModifyByte(offsetof(PatchElementHeader, old_length), 0x02, 0x00, &data);
    BufferSource buffer_source(data.data(), data.size());
    ElementMatch element_match = {};
    EXPECT_FALSE(patch::ParseElementMatch(&buffer_source, &element_match));
  }
  // Set new_length to 0 to trigger an error.
  {
    ByteVector data = CreateElementMatch();
    // new_length := 0.
    ModifyByte(offsetof(PatchElementHeader, new_length), 0x04, 0x00, &data);
    BufferSource buffer_source(data.data(), data.size());
    ElementMatch element_match = {};
    EXPECT_FALSE(patch::ParseElementMatch(&buffer_source, &element_match));
  }
  // Set both new_length and old_length to 0 to trigger an error.
  {
    ByteVector data = CreateElementMatch();
    // old_length := 0.
    ModifyByte(offsetof(PatchElementHeader, old_length), 0x02, 0x00, &data);
    // new_length := 0.
    ModifyByte(offsetof(PatchElementHeader, new_length), 0x04, 0x00, &data);
    BufferSource buffer_source(data.data(), data.size());
    ElementMatch element_match = {};
    EXPECT_FALSE(patch::ParseElementMatch(&buffer_source, &element_match));
  }
}

TEST(PatchTest, ParseSerializeElementMatchExeMismatch) {
  ByteVector buffer(28);
  BufferSink buffer_sink(buffer.data(), buffer.size());
  EXPECT_FALSE(patch::SerializeElementMatch(
      ElementMatch{{{1, 2}, kExeTypeNoOp}, {{3, 4}, kExeTypeWin32X86}},
      &buffer_sink));
}

TEST(PatchTest, SerializeElementMatchTooSmall) {
  ByteVector buffer(4);
  BufferSink buffer_sink(buffer.data(), buffer.size());
  EXPECT_FALSE(patch::SerializeElementMatch(
      ElementMatch{{{1, 2}, kExeTypeDex}, {{3, 4}, kExeTypeDex}},
      &buffer_sink));
}

TEST(PatchTest, ParseSerializeBuffer) {
  auto TestSerialize = [](const ByteVector& expected, const ByteVector& value) {
    size_t size = patch::SerializedBufferSize(value);
    EXPECT_EQ(expected.size(), size);
    ByteVector buffer(size);
    BufferSink buffer_sink(buffer.data(), buffer.size());
    EXPECT_TRUE(patch::SerializeBuffer(value, &buffer_sink));
    EXPECT_EQ(expected, buffer);
  };

  // |data| is passed as pointer to avoid passing a temporay, which can causes
  // dangling references.
  auto TestParse = [](const ByteVector* data) {
    BufferSource value;
    BufferSource buffer_source(data->data(), data->size());
    EXPECT_TRUE(patch::ParseBuffer(&buffer_source, &value));
    // Make sure all data has been consumed.
    EXPECT_TRUE(buffer_source.empty());
    return value;
  };

  ByteVector data = {
      0, 0, 0, 0,  // size
  };
  BufferSource buffer = TestParse(&data);
  EXPECT_TRUE(buffer.empty());
  TestSerialize(data, ByteVector({}));

  data = {
      3, 0, 0, 0,  // size
      1, 2, 3      // content
  };
  buffer = TestParse(&data);
  EXPECT_EQ(3U, buffer.size());
  EXPECT_EQ(ByteVector({1, 2, 3}), ByteVector(buffer.begin(), buffer.end()));
  TestSerialize(data, ByteVector({1, 2, 3}));

  // Ill-formed input.
  data = {
      3, 0, 0, 0,  // size
      1, 2         // insufficient content
  };
  BufferSource value;
  BufferSource buffer_source(data.data(), data.size());
  EXPECT_FALSE(patch::ParseBuffer(&buffer_source, &value));
  EXPECT_TRUE(value.empty());
}

TEST(PatchTest, SerializeBufferTooSmall) {
  ByteVector buffer(3);
  BufferSink buffer_sink(buffer.data(), buffer.size());
  EXPECT_FALSE(patch::SerializeBuffer(ByteVector(), &buffer_sink));
}

TEST(EquivalenceSinkSourceTest, Empty) {
  ByteVector data = {
      // EquivalenceSource
      0, 0, 0, 0,  // src_skip size
      0, 0, 0, 0,  // dst_skip size
      0, 0, 0, 0,  // copy_count size
  };
  EquivalenceSource equivalence_source =
      TestInitialize<EquivalenceSource>(&data);

  EXPECT_FALSE(equivalence_source.GetNext());
  EXPECT_TRUE(equivalence_source.Done());

  TestSerialize(data, EquivalenceSink());
}

TEST(EquivalenceSourceSinkTest, Normal) {
  ByteVector data = {
      // EquivalenceSource
      2, 0, 0, 0,  // src_skip size
      6, 7,        // src_skip content
      2, 0, 0, 0,  // dst_skip size
      7, 1,        // dst_skip content
      2, 0, 0, 0,  // copy_count size
      2, 1         // copy_count content
  };
  EquivalenceSource equivalence_source =
      TestInitialize<EquivalenceSource>(&data);
  auto equivalence = equivalence_source.GetNext();
  EXPECT_FALSE(equivalence_source.Done());
  EXPECT_TRUE(equivalence.has_value());
  EXPECT_EQ(offset_t(3), equivalence->src_offset);
  EXPECT_EQ(offset_t(7), equivalence->dst_offset);
  EXPECT_EQ(offset_t(2), equivalence->length);

  equivalence = equivalence_source.GetNext();
  EXPECT_TRUE(equivalence_source.Done());
  EXPECT_TRUE(equivalence.has_value());
  EXPECT_EQ(offset_t(1), equivalence->src_offset);
  EXPECT_EQ(offset_t(10), equivalence->dst_offset);
  EXPECT_EQ(offset_t(1), equivalence->length);

  equivalence = equivalence_source.GetNext();
  EXPECT_FALSE(equivalence.has_value());

  EquivalenceSink equivalence_sink;
  equivalence_sink.PutNext(Equivalence{3, 7, 2});
  equivalence_sink.PutNext(Equivalence{1, 10, 1});
  TestSerialize(data, equivalence_sink);
}

TEST(ExtraDataSourceSinkTest, Empty) {
  ByteVector data = {
      // ExtraDataSource
      0, 0, 0, 0,  // extra_data size
  };
  ExtraDataSource extra_data_source = TestInitialize<ExtraDataSource>(&data);

  EXPECT_FALSE(extra_data_source.GetNext(2));
  EXPECT_TRUE(extra_data_source.Done());

  TestSerialize(data, ExtraDataSink());
}

TEST(ExtraDataSourceSinkTest, Normal) {
  ByteVector data = {
      // ExtraDataSource
      5, 0, 0, 0,     // extra_data size
      1, 2, 3, 4, 5,  // extra_data content
  };
  ExtraDataSource extra_data_source = TestInitialize<ExtraDataSource>(&data);
  EXPECT_FALSE(extra_data_source.Done());

  auto extra_data = extra_data_source.GetNext(3);
  EXPECT_FALSE(extra_data_source.Done());
  EXPECT_TRUE(extra_data.has_value());
  EXPECT_EQ(size_t(3), extra_data->size());
  EXPECT_EQ(ByteVector({1, 2, 3}),
            ByteVector(extra_data->begin(), extra_data->end()));

  extra_data = extra_data_source.GetNext(2);
  EXPECT_TRUE(extra_data_source.Done());
  EXPECT_TRUE(extra_data.has_value());
  EXPECT_EQ(ByteVector({4, 5}),
            ByteVector(extra_data->begin(), extra_data->end()));

  extra_data = extra_data_source.GetNext(2);
  EXPECT_FALSE(extra_data.has_value());

  ExtraDataSink extra_data_sink;

  ByteVector content = {1, 2, 3};
  extra_data_sink.PutNext({content.data(), content.size()});
  content = {4, 5};
  extra_data_sink.PutNext({content.data(), content.size()});
  TestSerialize(data, extra_data_sink);
}

TEST(RawDeltaSourceSinkTest, Empty) {
  ByteVector data = {
      // RawDeltaSource
      0, 0, 0, 0,  // raw_delta_skip size
      0, 0, 0, 0,  // raw_delta_diff size
  };
  RawDeltaSource raw_delta_source = TestInitialize<RawDeltaSource>(&data);

  EXPECT_FALSE(raw_delta_source.GetNext());
  EXPECT_TRUE(raw_delta_source.Done());

  TestSerialize(data, RawDeltaSink());
}

TEST(RawDeltaSinkSourceSinkTest, Normal) {
  ByteVector data = {
      // RawDeltaSource
      3,  0,  0,   0,  // raw_delta_skip size
      1,  3,  0,       // raw_delta_skip content
      3,  0,  0,   0,  // raw_delta_diff size
      42, 24, 235,     // raw_delta_diff content
  };
  RawDeltaSource raw_delta_source = TestInitialize<RawDeltaSource>(&data);
  EXPECT_FALSE(raw_delta_source.Done());

  auto raw_delta = raw_delta_source.GetNext();
  EXPECT_FALSE(raw_delta_source.Done());
  EXPECT_TRUE(raw_delta.has_value());
  EXPECT_EQ(1U, raw_delta->copy_offset);
  EXPECT_EQ(42, raw_delta->diff);

  raw_delta = raw_delta_source.GetNext();
  EXPECT_FALSE(raw_delta_source.Done());
  EXPECT_TRUE(raw_delta.has_value());
  EXPECT_EQ(5U, raw_delta->copy_offset);
  EXPECT_EQ(24, raw_delta->diff);

  raw_delta = raw_delta_source.GetNext();
  EXPECT_TRUE(raw_delta_source.Done());
  EXPECT_TRUE(raw_delta.has_value());
  EXPECT_EQ(6U, raw_delta->copy_offset);
  EXPECT_EQ(-21, raw_delta->diff);

  EXPECT_FALSE(raw_delta_source.GetNext());
  EXPECT_TRUE(raw_delta_source.Done());

  RawDeltaSink raw_delta_sink;
  raw_delta_sink.PutNext({1, 42});
  raw_delta_sink.PutNext({5, 24});
  raw_delta_sink.PutNext({6, -21});
  TestSerialize(data, raw_delta_sink);
}

TEST(RawDeltaSourceSinkTest, InvalidContent) {
  ByteVector data = {
      // RawDeltaSource
      2, 0, 0, 0,  // raw_delta_skip size
      1, 3,        // raw_delta_skip content
      2, 0, 0, 0,  // raw_delta_diff size
      0, 4,        // raw_delta_diff content
  };
  RawDeltaSource raw_delta_source = TestInitialize<RawDeltaSource>(&data);
  EXPECT_FALSE(raw_delta_source.GetNext());
  EXPECT_FALSE(raw_delta_source.Done());
}

TEST(ReferenceDeltaSourceSinkTest, Empty) {
  ByteVector data = {
      // ReferenceDeltaSource
      0, 0, 0, 0,  // reference_delta size
  };
  ReferenceDeltaSource reference_delta_source =
      TestInitialize<ReferenceDeltaSource>(&data);

  EXPECT_FALSE(reference_delta_source.GetNext());
  EXPECT_TRUE(reference_delta_source.Done());

  TestSerialize(data, ReferenceDeltaSink());
}

TEST(ReferenceDeltaSourceSinkTest, Normal) {
  ByteVector data = {
      // ReferenceDeltaSource
      2,  0,  0, 0,  // reference_delta size
      84, 47,        // reference_delta content
  };
  ReferenceDeltaSource reference_delta_source =
      TestInitialize<ReferenceDeltaSource>(&data);
  EXPECT_FALSE(reference_delta_source.Done());

  auto delta = reference_delta_source.GetNext();
  EXPECT_FALSE(reference_delta_source.Done());
  EXPECT_TRUE(delta.has_value());
  EXPECT_EQ(42, *delta);

  delta = reference_delta_source.GetNext();
  EXPECT_TRUE(reference_delta_source.Done());
  EXPECT_TRUE(delta.has_value());
  EXPECT_EQ(-24, *delta);

  EXPECT_FALSE(reference_delta_source.GetNext());
  EXPECT_TRUE(reference_delta_source.Done());

  ReferenceDeltaSink reference_delta;
  reference_delta.PutNext(42);
  reference_delta.PutNext(-24);
  TestSerialize(data, reference_delta);
}

TEST(TargetSourceSinkTest, Empty) {
  ByteVector data = {
      // TargetSource
      0, 0, 0, 0,  // extra_targets size
  };
  TargetSource target_source = TestInitialize<TargetSource>(&data);

  EXPECT_FALSE(target_source.GetNext());
  EXPECT_TRUE(target_source.Done());

  TestSerialize(data, TargetSink());
}

TEST(TargetSourceSinkTest, Normal) {
  ByteVector data = {
      // TargetSource
      2, 0, 0, 0,  // extra_targets size
      3, 1,        // extra_targets content
  };
  TargetSource target_source = TestInitialize<TargetSource>(&data);
  EXPECT_FALSE(target_source.Done());

  auto target = target_source.GetNext();
  EXPECT_FALSE(target_source.Done());
  EXPECT_TRUE(target.has_value());
  EXPECT_EQ(3U, *target);

  target = target_source.GetNext();
  EXPECT_TRUE(target_source.Done());
  EXPECT_TRUE(target.has_value());
  EXPECT_EQ(5U, *target);

  EXPECT_FALSE(target_source.GetNext());
  EXPECT_TRUE(target_source.Done());

  TargetSink target_sink;
  target_sink.PutNext(3);
  target_sink.PutNext(5);
  TestSerialize(data, target_sink);
}

TEST(PatchElementTest, Normal) {
  ByteVector data = CreatePatchElement();

  PatchElementReader patch_element_reader =
      TestInitialize<PatchElementReader>(&data);

  ElementMatch element_match = patch_element_reader.element_match();
  EXPECT_EQ(kExeTypeWin32X86, element_match.exe_type());
  EXPECT_EQ(kExeTypeWin32X86, element_match.old_element.exe_type);
  EXPECT_EQ(kExeTypeWin32X86, element_match.new_element.exe_type);
  EXPECT_EQ(0x1U, element_match.old_element.offset);
  EXPECT_EQ(0x51U, element_match.old_element.size);
  EXPECT_EQ(0x3U, element_match.new_element.offset);
  EXPECT_EQ(0x13U, element_match.new_element.size);

  EquivalenceSource equivalence_source =
      patch_element_reader.GetEquivalenceSource();
  EXPECT_EQ(ByteVector({0x10}), equivalence_source.src_skip());
  EXPECT_EQ(ByteVector({0x00}), equivalence_source.dst_skip());
  EXPECT_EQ(ByteVector({0x12}), equivalence_source.copy_count());

  ExtraDataSource extra_data_source = patch_element_reader.GetExtraDataSource();
  EXPECT_EQ(ByteVector({0x13}), extra_data_source.extra_data());

  RawDeltaSource raw_delta_source = patch_element_reader.GetRawDeltaSource();
  EXPECT_EQ(ByteVector({0x14}), raw_delta_source.raw_delta_skip());
  EXPECT_EQ(ByteVector({0x15}), raw_delta_source.raw_delta_diff());

  ReferenceDeltaSource reference_delta_source =
      patch_element_reader.GetReferenceDeltaSource();
  EXPECT_EQ(ByteVector({0x16}), reference_delta_source.reference_delta());

  TargetSource target_source1 =
      patch_element_reader.GetExtraTargetSource(PoolTag(0));
  EXPECT_EQ(ByteVector({0x17}), target_source1.extra_targets());
  TargetSource target_source2 =
      patch_element_reader.GetExtraTargetSource(PoolTag(1));
  EXPECT_EQ(ByteVector({}), target_source2.extra_targets());
  TargetSource target_source3 =
      patch_element_reader.GetExtraTargetSource(PoolTag(2));
  EXPECT_EQ(ByteVector({0x18}), target_source3.extra_targets());

  PatchElementWriter patch_element_writer(element_match);

  patch_element_writer.SetEquivalenceSink(
      EquivalenceSink({0x10}, {0x00}, {0x12}));
  patch_element_writer.SetExtraDataSink(ExtraDataSink({0x13}));
  patch_element_writer.SetRawDeltaSink(RawDeltaSink({0x14}, {0x15}));
  patch_element_writer.SetReferenceDeltaSink(ReferenceDeltaSink({0x16}));
  patch_element_writer.SetTargetSink(PoolTag(0), TargetSink({0x17}));
  patch_element_writer.SetTargetSink(PoolTag(2), TargetSink({0x18}));
  TestSerialize(data, patch_element_writer);
}

TEST(PatchElementTest, BadEquivalence) {
  // If the "old" element is too small then the test should fail.
  {
    ByteVector data = CreatePatchElement();
    // old_length := 0x4 (too small).
    ModifyByte(offsetof(PatchElementHeader, old_length), 0x51, 0x04, &data);
    TestInvalidInitialize<PatchElementReader>(&data);
  }

  // If the "new" element is too small then the test should fail.
  {
    ByteVector data = CreatePatchElement();
    // new_length := 0x5 (too small).
    ModifyByte(offsetof(PatchElementHeader, new_length), 0x13, 0x05, &data);
    TestInvalidInitialize<PatchElementReader>(&data);
  }
}

TEST(PatchElementTest, WrongExtraData) {
  // Make "new" too large so insufficient extra data exists to cover the image.
  {
    ByteVector data = CreatePatchElement();
    // new_length := 0x14 (too large).
    ModifyByte(offsetof(PatchElementHeader, new_length), 0x13, 0x14, &data);
    TestInvalidInitialize<PatchElementReader>(&data);
  }
  // Make "new" too small so there is too much extra data.
  {
    ByteVector data = CreatePatchElement();
    // new_length := 0x12 (too small).
    ModifyByte(offsetof(PatchElementHeader, new_length), 0x13, 0x12, &data);
    TestInvalidInitialize<PatchElementReader>(&data);
  }
}

TEST(PatchElementTest, WrongVersion) {
  // Bump element version to 2.
  {
    ByteVector data = CreatePatchElement();
    ModifyByte(offsetof(PatchElementHeader, version), 0x01, 0x02, &data);
    TestInvalidInitialize<PatchElementReader>(&data);
  }
  // Bump element version to 0.
  {
    ByteVector data = CreatePatchElement();
    ModifyByte(offsetof(PatchElementHeader, version), 0x01, 0x00, &data);
    TestInvalidInitialize<PatchElementReader>(&data);
  }
}

TEST(EnsemblePatchTest, RawPatch) {
  ByteVector data = {
      // PatchHeader
      0x5A, 0x75, 0x63, 0x63,  // magic
      0x02, 0x00, 0x00, 0x00,  // major/minor version
      0x10, 0x32, 0x54, 0x76,  // old_size
      0x00, 0x11, 0x22, 0x33,  // old_crc
      0x01, 0, 0, 0,           // new_size
      0x44, 0x55, 0x66, 0x77,  // new_crc

      1, 0, 0, 0,  // number of element

      // PatchElementHeader
      0x01, 0, 0, 0,       // old_offset
      0x02, 0, 0, 0,       // old_length
      0x00, 0, 0, 0,       // new_offset
      0x01, 0, 0, 0,       // new_length
      'P', 'x', '8', '6',  // exe_type = EXE_TYPE_WIN32_X8
      0x01, 0x00,          // element version
      // EquivalenceSource
      0, 0, 0, 0,  // src_skip size
      0, 0, 0, 0,  // dst_skip size
      0, 0, 0, 0,  // copy_count size
      // ExtraDataSource
      0x01, 0, 0, 0,  // extra_data size
      0x04,           // extra_data content
      // RawDeltaSource
      0, 0, 0, 0,  // raw_delta_skip size
      0, 0, 0, 0,  // raw_delta_diff size
      // ReferenceDeltaSource
      0, 0, 0, 0,  // reference_delta size
      // PatchElementReader
      0, 0, 0, 0,  // pool count
  };

  EnsemblePatchReader ensemble_patch_reader =
      TestInitialize<EnsemblePatchReader>(&data);

  PatchHeader header = ensemble_patch_reader.header();
  EXPECT_EQ(PatchHeader::kMagic, header.magic);
  EXPECT_EQ(kMajorVersion, header.major_version);
  EXPECT_EQ(kMinorVersion, header.minor_version);
  EXPECT_EQ(0x76543210U, header.old_size);
  EXPECT_EQ(0x33221100U, header.old_crc);
  EXPECT_EQ(0x01U, header.new_size);
  EXPECT_EQ(0x77665544U, header.new_crc);

  const std::vector<PatchElementReader>& elements =
      ensemble_patch_reader.elements();
  EXPECT_EQ(size_t(1), elements.size());

  EnsemblePatchWriter ensemble_patch_writer(header);
  PatchElementWriter patch_element_writer(elements[0].element_match());
  patch_element_writer.SetEquivalenceSink({});
  patch_element_writer.SetExtraDataSink(ExtraDataSink({0x04}));
  patch_element_writer.SetRawDeltaSink({});
  patch_element_writer.SetReferenceDeltaSink({});
  ensemble_patch_writer.AddElement(std::move(patch_element_writer));

  TestSerialize(data, ensemble_patch_writer);
}

TEST(EnsemblePatchTest, CheckFile) {
  ByteVector data = {
      // PatchHeader
      0x5A, 0x75, 0x63, 0x63,  // magic
      0x02, 0x00, 0x00, 0x00,  // major/minor version
      0x05, 0x00, 0x00, 0x00,  // old_size
      0xDF, 0x13, 0xE4, 0x10,  // old_crc
      0x03, 0x00, 0x00, 0x00,  // new_size
      0xDC, 0xF7, 0x00, 0x40,  // new_crc

      1, 0, 0, 0,  // number of element

      // PatchElementHeader
      0x01, 0, 0, 0,       // old_offset
      0x02, 0, 0, 0,       // old_length
      0x00, 0, 0, 0,       // new_offset
      0x03, 0, 0, 0,       // new_length
      'P', 'x', '8', '6',  // exe_type = EXE_TYPE_WIN32_X86
      0x01, 0x00,          // element version
      // EquivalenceSource
      0, 0, 0, 0,  // src_skip size
      0, 0, 0, 0,  // dst_skip size
      0, 0, 0, 0,  // copy_count size
      // ExtraDataSource
      0x03, 0, 0, 0,  // extra_data size
      'A', 'B', 'C',  // extra_data content
      // RawDeltaSource
      0, 0, 0, 0,  // raw_delta_skip size
      0, 0, 0, 0,  // raw_delta_diff size
      // ReferenceDeltaSource
      0, 0, 0, 0,  // reference_delta size
      // PatchElementReader
      0, 0, 0, 0,  // pool count
  };

  EnsemblePatchReader ensemble_patch_reader =
      TestInitialize<EnsemblePatchReader>(&data);

  ByteVector old_file = {0x10, 0x32, 0x54, 0x76, 0x98};
  ByteVector new_file = {0xBA, 0xDC, 0xFE};

  ConstBufferView old_image(old_file.data(), old_file.size());
  ConstBufferView new_image(new_file.data(), new_file.size());

  EXPECT_TRUE(ensemble_patch_reader.CheckOldFile(old_image));
  EXPECT_TRUE(ensemble_patch_reader.CheckNewFile(new_image));
  EXPECT_FALSE(ensemble_patch_reader.CheckOldFile(new_image));
  EXPECT_FALSE(ensemble_patch_reader.CheckNewFile(old_image));
}

TEST(EnsemblePatchTest, InvalidMagic) {
  ByteVector data = {
      // PatchHeader
      0x42, 0x42, 0x42, 0x42,  // magic
      0x02, 0x00, 0x00, 0x00,  // major/minor version
      0x10, 0x32, 0x54, 0x76,  // old_size
      0x00, 0x11, 0x22, 0x33,  // old_crc
      0x03, 0x00, 0x00, 0x00,  // new_size
      0x44, 0x55, 0x66, 0x77,  // new_crc

      1, 0, 0, 0,  // number of element

      // PatchElementHeader
      0x01, 0, 0, 0,       // old_offset
      0x02, 0, 0, 0,       // old_length
      0x00, 0, 0, 0,       // new_offset
      0x03, 0, 0, 0,       // new_length
      'P', 'x', '8', '6',  // exe_type = EXE_TYPE_WIN32_X86
      0x01, 0x00,          // element version
      // EquivalenceSource
      0, 0, 0, 0,  // src_skip size
      0, 0, 0, 0,  // dst_skip size
      0, 0, 0, 0,  // copy_count size
      // ExtraDataSource
      0, 0, 0, 0,  // extra_data size
      // RawDeltaSource
      0, 0, 0, 0,  // raw_delta_skip size
      0, 0, 0, 0,  // raw_delta_diff size
      // ReferenceDeltaSource
      0, 0, 0, 0,  // reference_delta size
      // PatchElementReader
      0, 0, 0, 0,  // pool count
  };

  TestInvalidInitialize<EnsemblePatchReader>(&data);
}

TEST(EnsemblePatchTest, InvalidVersion) {
  ByteVector data = {
      // PatchHeader
      0x5A, 0x75, 0x63, 0x63,  // magic
      0x01, 0x00, 0x00, 0x00,  // major/minor version
      0x10, 0x32, 0x54, 0x76,  // old_size
      0x00, 0x11, 0x22, 0x33,  // old_crc
      0x03, 0x00, 0x00, 0x00,  // new_size
      0x44, 0x55, 0x66, 0x77,  // new_crc

      1, 0, 0, 0,  // number of element

      // PatchElementHeader
      0x01, 0, 0, 0,       // old_offset
      0x02, 0, 0, 0,       // old_length
      0x00, 0, 0, 0,       // new_offset
      0x03, 0, 0, 0,       // new_length
      'P', 'x', '8', '6',  // exe_type = EXE_TYPE_WIN32_X86
      0x01, 0x00,          // element version
      // EquivalenceSource
      0, 0, 0, 0,  // src_skip size
      0, 0, 0, 0,  // dst_skip size
      0, 0, 0, 0,  // copy_count size
      // ExtraDataSource
      0, 0, 0, 0,  // extra_data size
      // RawDeltaSource
      0, 0, 0, 0,  // raw_delta_skip size
      0, 0, 0, 0,  // raw_delta_diff size
      // ReferenceDeltaSource
      0, 0, 0, 0,  // reference_delta size
      // PatchElementReader
      0, 0, 0, 0,  // pool count
  };

  TestInvalidInitialize<EnsemblePatchReader>(&data);
}

}  // namespace zucchini
