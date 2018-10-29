// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/spdy/core/spdy_frame_reader.h"

#include <algorithm>
#include <iostream>
#include <memory>

#include "base/sys_byteorder.h"
#include "net/third_party/spdy/platform/api/spdy_arraysize.h"
#include "testing/platform_test.h"

namespace spdy {

TEST(SpdyFrameReaderTest, ReadUInt16) {
  // Frame data in network byte order.
  const uint16_t kFrameData[] = {
      base::HostToNet16(1), base::HostToNet16(1 << 15),
  };

  SpdyFrameReader frame_reader(reinterpret_cast<const char*>(kFrameData),
                               sizeof(kFrameData));
  EXPECT_FALSE(frame_reader.IsDoneReading());

  uint16_t uint16_val;
  EXPECT_TRUE(frame_reader.ReadUInt16(&uint16_val));
  EXPECT_FALSE(frame_reader.IsDoneReading());
  EXPECT_EQ(1, uint16_val);

  EXPECT_TRUE(frame_reader.ReadUInt16(&uint16_val));
  EXPECT_TRUE(frame_reader.IsDoneReading());
  EXPECT_EQ(1 << 15, uint16_val);
}

TEST(SpdyFrameReaderTest, ReadUInt32) {
  // Frame data in network byte order.
  const uint32_t kFrameData[] = {
      base::HostToNet32(1), base::HostToNet32(0x80000000),
  };

  SpdyFrameReader frame_reader(reinterpret_cast<const char*>(kFrameData),
                               SPDY_ARRAYSIZE(kFrameData) * sizeof(uint32_t));
  EXPECT_FALSE(frame_reader.IsDoneReading());

  uint32_t uint32_val;
  EXPECT_TRUE(frame_reader.ReadUInt32(&uint32_val));
  EXPECT_FALSE(frame_reader.IsDoneReading());
  EXPECT_EQ(1u, uint32_val);

  EXPECT_TRUE(frame_reader.ReadUInt32(&uint32_val));
  EXPECT_TRUE(frame_reader.IsDoneReading());
  EXPECT_EQ(1u << 31, uint32_val);
}

TEST(SpdyFrameReaderTest, ReadStringPiece16) {
  // Frame data in network byte order.
  const char kFrameData[] = {
      0x00, 0x02,  // uint16_t(2)
      0x48, 0x69,  // "Hi"
      0x00, 0x10,  // uint16_t(16)
      0x54, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x67, 0x2c,
      0x20, 0x31, 0x2c, 0x20, 0x32, 0x2c, 0x20, 0x33,  // "Testing, 1, 2, 3"
  };

  SpdyFrameReader frame_reader(kFrameData, SPDY_ARRAYSIZE(kFrameData));
  EXPECT_FALSE(frame_reader.IsDoneReading());

  SpdyStringPiece stringpiece_val;
  EXPECT_TRUE(frame_reader.ReadStringPiece16(&stringpiece_val));
  EXPECT_FALSE(frame_reader.IsDoneReading());
  EXPECT_EQ(0, stringpiece_val.compare("Hi"));

  EXPECT_TRUE(frame_reader.ReadStringPiece16(&stringpiece_val));
  EXPECT_TRUE(frame_reader.IsDoneReading());
  EXPECT_EQ(0, stringpiece_val.compare("Testing, 1, 2, 3"));
}

TEST(SpdyFrameReaderTest, ReadStringPiece32) {
  // Frame data in network byte order.
  const char kFrameData[] = {
      0x00, 0x00, 0x00, 0x03,  // uint32_t(3)
      0x66, 0x6f, 0x6f,        // "foo"
      0x00, 0x00, 0x00, 0x10,  // uint32_t(16)
      0x54, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x67, 0x2c,
      0x20, 0x34, 0x2c, 0x20, 0x35, 0x2c, 0x20, 0x36,  // "Testing, 4, 5, 6"
  };

  SpdyFrameReader frame_reader(kFrameData, SPDY_ARRAYSIZE(kFrameData));
  EXPECT_FALSE(frame_reader.IsDoneReading());

  SpdyStringPiece stringpiece_val;
  EXPECT_TRUE(frame_reader.ReadStringPiece32(&stringpiece_val));
  EXPECT_FALSE(frame_reader.IsDoneReading());
  EXPECT_EQ(0, stringpiece_val.compare("foo"));

  EXPECT_TRUE(frame_reader.ReadStringPiece32(&stringpiece_val));
  EXPECT_TRUE(frame_reader.IsDoneReading());
  EXPECT_EQ(0, stringpiece_val.compare("Testing, 4, 5, 6"));
}

TEST(SpdyFrameReaderTest, ReadUInt16WithBufferTooSmall) {
  // Frame data in network byte order.
  const char kFrameData[] = {
      0x00,  // part of a uint16_t
  };

  SpdyFrameReader frame_reader(kFrameData, SPDY_ARRAYSIZE(kFrameData));
  EXPECT_FALSE(frame_reader.IsDoneReading());

  uint16_t uint16_val;
  EXPECT_FALSE(frame_reader.ReadUInt16(&uint16_val));
}

TEST(SpdyFrameReaderTest, ReadUInt32WithBufferTooSmall) {
  // Frame data in network byte order.
  const char kFrameData[] = {
      0x00, 0x00, 0x00,  // part of a uint32_t
  };

  SpdyFrameReader frame_reader(kFrameData, SPDY_ARRAYSIZE(kFrameData));
  EXPECT_FALSE(frame_reader.IsDoneReading());

  uint32_t uint32_val;
  EXPECT_FALSE(frame_reader.ReadUInt32(&uint32_val));

  // Also make sure that trying to read a uint16_t, which technically could
  // work, fails immediately due to previously encountered failed read.
  uint16_t uint16_val;
  EXPECT_FALSE(frame_reader.ReadUInt16(&uint16_val));
}

// Tests ReadStringPiece16() with a buffer too small to fit the entire string.
TEST(SpdyFrameReaderTest, ReadStringPiece16WithBufferTooSmall) {
  // Frame data in network byte order.
  const char kFrameData[] = {
      0x00, 0x03,  // uint16_t(3)
      0x48, 0x69,  // "Hi"
  };

  SpdyFrameReader frame_reader(kFrameData, SPDY_ARRAYSIZE(kFrameData));
  EXPECT_FALSE(frame_reader.IsDoneReading());

  SpdyStringPiece stringpiece_val;
  EXPECT_FALSE(frame_reader.ReadStringPiece16(&stringpiece_val));

  // Also make sure that trying to read a uint16_t, which technically could
  // work, fails immediately due to previously encountered failed read.
  uint16_t uint16_val;
  EXPECT_FALSE(frame_reader.ReadUInt16(&uint16_val));
}

// Tests ReadStringPiece16() with a buffer too small even to fit the length.
TEST(SpdyFrameReaderTest, ReadStringPiece16WithBufferWayTooSmall) {
  // Frame data in network byte order.
  const char kFrameData[] = {
      0x00,  // part of a uint16_t
  };

  SpdyFrameReader frame_reader(kFrameData, SPDY_ARRAYSIZE(kFrameData));
  EXPECT_FALSE(frame_reader.IsDoneReading());

  SpdyStringPiece stringpiece_val;
  EXPECT_FALSE(frame_reader.ReadStringPiece16(&stringpiece_val));

  // Also make sure that trying to read a uint16_t, which technically could
  // work, fails immediately due to previously encountered failed read.
  uint16_t uint16_val;
  EXPECT_FALSE(frame_reader.ReadUInt16(&uint16_val));
}

// Tests ReadStringPiece32() with a buffer too small to fit the entire string.
TEST(SpdyFrameReaderTest, ReadStringPiece32WithBufferTooSmall) {
  // Frame data in network byte order.
  const char kFrameData[] = {
      0x00, 0x00, 0x00, 0x03,  // uint32_t(3)
      0x48, 0x69,              // "Hi"
  };

  SpdyFrameReader frame_reader(kFrameData, SPDY_ARRAYSIZE(kFrameData));
  EXPECT_FALSE(frame_reader.IsDoneReading());

  SpdyStringPiece stringpiece_val;
  EXPECT_FALSE(frame_reader.ReadStringPiece32(&stringpiece_val));

  // Also make sure that trying to read a uint16_t, which technically could
  // work, fails immediately due to previously encountered failed read.
  uint16_t uint16_val;
  EXPECT_FALSE(frame_reader.ReadUInt16(&uint16_val));
}

// Tests ReadStringPiece32() with a buffer too small even to fit the length.
TEST(SpdyFrameReaderTest, ReadStringPiece32WithBufferWayTooSmall) {
  // Frame data in network byte order.
  const char kFrameData[] = {
      0x00, 0x00, 0x00,  // part of a uint32_t
  };

  SpdyFrameReader frame_reader(kFrameData, SPDY_ARRAYSIZE(kFrameData));
  EXPECT_FALSE(frame_reader.IsDoneReading());

  SpdyStringPiece stringpiece_val;
  EXPECT_FALSE(frame_reader.ReadStringPiece32(&stringpiece_val));

  // Also make sure that trying to read a uint16_t, which technically could
  // work, fails immediately due to previously encountered failed read.
  uint16_t uint16_val;
  EXPECT_FALSE(frame_reader.ReadUInt16(&uint16_val));
}

TEST(SpdyFrameReaderTest, ReadBytes) {
  // Frame data in network byte order.
  const char kFrameData[] = {
      0x66, 0x6f, 0x6f,  // "foo"
      0x48, 0x69,        // "Hi"
  };

  SpdyFrameReader frame_reader(kFrameData, SPDY_ARRAYSIZE(kFrameData));
  EXPECT_FALSE(frame_reader.IsDoneReading());

  char dest1[3] = {};
  EXPECT_TRUE(frame_reader.ReadBytes(&dest1, SPDY_ARRAYSIZE(dest1)));
  EXPECT_FALSE(frame_reader.IsDoneReading());
  EXPECT_EQ("foo", SpdyStringPiece(dest1, SPDY_ARRAYSIZE(dest1)));

  char dest2[2] = {};
  EXPECT_TRUE(frame_reader.ReadBytes(&dest2, SPDY_ARRAYSIZE(dest2)));
  EXPECT_TRUE(frame_reader.IsDoneReading());
  EXPECT_EQ("Hi", SpdyStringPiece(dest2, SPDY_ARRAYSIZE(dest2)));
}

TEST(SpdyFrameReaderTest, ReadBytesWithBufferTooSmall) {
  // Frame data in network byte order.
  const char kFrameData[] = {
      0x01,
  };

  SpdyFrameReader frame_reader(kFrameData, SPDY_ARRAYSIZE(kFrameData));
  EXPECT_FALSE(frame_reader.IsDoneReading());

  char dest[SPDY_ARRAYSIZE(kFrameData) + 2] = {};
  EXPECT_FALSE(frame_reader.ReadBytes(&dest, SPDY_ARRAYSIZE(kFrameData) + 1));
  EXPECT_STREQ("", dest);
}

}  // namespace spdy
