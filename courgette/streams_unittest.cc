// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/streams.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

TEST(StreamsTest, SimpleWriteRead) {
  const unsigned int kValue1 = 12345;
  courgette::SinkStream sink;

  EXPECT_TRUE(sink.WriteVarint32(kValue1));

  const uint8_t* sink_buffer = sink.Buffer();
  size_t length = sink.Length();

  courgette::SourceStream source;
  source.Init(sink_buffer, length);

  unsigned int value;
  bool can_read = source.ReadVarint32(&value);
  EXPECT_TRUE(can_read);
  EXPECT_EQ(kValue1, value);
  EXPECT_EQ(0U, source.Remaining());
}

TEST(StreamsTest, SimpleWriteRead2) {
  courgette::SinkStream sink;

  EXPECT_TRUE(sink.Write("Hello", 5));

  const uint8_t* sink_buffer = sink.Buffer();
  size_t sink_length = sink.Length();

  courgette::SourceStream source;
  source.Init(sink_buffer, sink_length);

  char text[10] = {0};
  bool can_read = source.Read(text, 5);
  EXPECT_TRUE(can_read);
  EXPECT_EQ(0, memcmp("Hello", text, 5));
  EXPECT_EQ(0U, source.Remaining());
}

TEST(StreamsTest, StreamSetWriteRead) {
  courgette::SinkStreamSet out;
  out.Init(4);

  const unsigned int kValue1 = 12345;

  EXPECT_TRUE(out.stream(3)->WriteVarint32(kValue1));

  courgette::SinkStream collected;

  EXPECT_TRUE(out.CopyTo(&collected));

  const uint8_t* collected_buffer = collected.Buffer();
  size_t collected_length = collected.Length();

  courgette::SourceStreamSet in;
  bool can_init = in.Init(collected_buffer, collected_length);
  EXPECT_TRUE(can_init);

  uint32_t value;
  bool can_read = in.stream(3)->ReadVarint32(&value);
  EXPECT_TRUE(can_read);
  EXPECT_EQ(kValue1, value);
  EXPECT_EQ(0U, in.stream(3)->Remaining());
  EXPECT_EQ(0U, in.stream(2)->Remaining());
}

TEST(StreamsTest, StreamSetWriteRead2) {
  const size_t kNumberOfStreams = 4;
  const unsigned int kEnd = ~0U;

  courgette::SinkStreamSet out;
  out.Init(kNumberOfStreams);

  static const unsigned int data[] = {
    3, 123,  3, 1000,  0, 100, 2, 100,  0, 999999,
    0, 0,  0, 0,  1, 2,  1, 3,  1, 5,  0, 66,
    // varint32 edge case values:
    1, 127,  1, 128,  1, 129,  1, 16383,  1, 16384,
    kEnd
  };

  for (size_t i = 0;  data[i] != kEnd;  i += 2) {
    size_t id = data[i];
    size_t datum = data[i + 1];
    EXPECT_TRUE(out.stream(id)->WriteVarint32(datum));
  }

  courgette::SinkStream collected;

  EXPECT_TRUE(out.CopyTo(&collected));

  courgette::SourceStreamSet in;
  bool can_init = in.Init(collected.Buffer(), collected.Length());
  EXPECT_TRUE(can_init);

  for (size_t i = 0;  data[i] != kEnd;  i += 2) {
    size_t id = data[i];
    size_t datum = data[i + 1];
    uint32_t value = 77;
    bool can_read = in.stream(id)->ReadVarint32(&value);
    EXPECT_TRUE(can_read);
    EXPECT_EQ(datum, value);
  }

  for (size_t i = 0;  i < kNumberOfStreams;  ++i) {
    EXPECT_EQ(0U, in.stream(i)->Remaining());
  }
}

TEST(StreamsTest, SignedVarint32) {
  courgette::SinkStream out;

  static const int32_t data[] = {0,       64,      128,        8192,
                                 16384,   1 << 20, 1 << 21,    1 << 22,
                                 1 << 27, 1 << 28, 0x7fffffff, -0x7fffffff};

  std::vector<int32_t> values;
  for (size_t i = 0;  i < sizeof(data)/sizeof(data[0]);  ++i) {
    int32_t basis = data[i];
    for (int delta = -4; delta <= 4; ++delta) {
      EXPECT_TRUE(out.WriteVarint32Signed(basis + delta));
      values.push_back(basis + delta);
      EXPECT_TRUE(out.WriteVarint32Signed(-basis + delta));
      values.push_back(-basis + delta);
    }
  }

  courgette::SourceStream in;
  in.Init(out);

  for (size_t i = 0;  i < values.size();  ++i) {
    int written_value = values[i];
    int32_t datum;
    bool can_read = in.ReadVarint32Signed(&datum);
    EXPECT_TRUE(can_read);
    EXPECT_EQ(written_value, datum);
  }

  EXPECT_TRUE(in.Empty());
}

TEST(StreamsTest, StreamSetReadWrite) {
  courgette::SinkStreamSet out;

  { // Local scope for temporary stream sets.
    courgette::SinkStreamSet subset1;
    EXPECT_TRUE(subset1.stream(3)->WriteVarint32(30000));
    EXPECT_TRUE(subset1.stream(5)->WriteVarint32(50000));
    EXPECT_TRUE(out.WriteSet(&subset1));

    courgette::SinkStreamSet subset2;
    EXPECT_TRUE(subset2.stream(2)->WriteVarint32(20000));
    EXPECT_TRUE(subset2.stream(6)->WriteVarint32(60000));
    EXPECT_TRUE(out.WriteSet(&subset2));
  }

  courgette::SinkStream collected;
  EXPECT_TRUE(out.CopyTo(&collected));
  courgette::SourceStreamSet in;
  bool can_init_in = in.Init(collected.Buffer(), collected.Length());
  EXPECT_TRUE(can_init_in);

  courgette::SourceStreamSet subset1;
  bool can_read_1 = in.ReadSet(&subset1);
  EXPECT_TRUE(can_read_1);
  EXPECT_FALSE(in.Empty());

  courgette::SourceStreamSet subset2;
  bool can_read_2 = in.ReadSet(&subset2);
  EXPECT_TRUE(can_read_2);
  EXPECT_TRUE(in.Empty());

  courgette::SourceStreamSet subset3;
  bool can_read_3 = in.ReadSet(&subset3);
  EXPECT_FALSE(can_read_3);

  EXPECT_FALSE(subset1.Empty());
  EXPECT_FALSE(subset1.Empty());

  uint32_t datum;
  EXPECT_TRUE(subset1.stream(3)->ReadVarint32(&datum));
  EXPECT_EQ(30000U, datum);
  EXPECT_TRUE(subset1.stream(5)->ReadVarint32(&datum));
  EXPECT_EQ(50000U, datum);
  EXPECT_TRUE(subset1.Empty());

  EXPECT_TRUE(subset2.stream(2)->ReadVarint32(&datum));
  EXPECT_EQ(20000U, datum);
  EXPECT_TRUE(subset2.stream(6)->ReadVarint32(&datum));
  EXPECT_EQ(60000U, datum);
  EXPECT_TRUE(subset2.Empty());
}
