/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "gtest/gtest.h"

#include "nacl_io/fifo_char.h"
#include "nacl_io/socket/fifo_packet.h"
#include "nacl_io/socket/packet.h"

#include "ppapi_simple/ps.h"

using namespace nacl_io;

namespace {
const size_t kTestSize = 32;
const size_t kHalfSize = kTestSize / 2;
const size_t kQuarterSize = kTestSize / 4;
};

/**
 * Test writes that start a wrapped location.  We had
 * a bug where writes that wrapped around were fine but
 * bytes that started at a wrapped location were being
 * written to the wrong loction.
 */
TEST(FIFOChar, WriteWrapped) {
  char temp_wr[kTestSize * 2];
  char temp_rd[kTestSize * 2];
  size_t wr_offs = 0;
  size_t rd_offs = 0;

  memset(temp_rd, 0, sizeof(temp_rd));
  for (size_t index = 0; index < sizeof(temp_wr); index++)
    temp_wr[index] = index;

  FIFOChar fifo(kTestSize);

  // Fill the fifo
  wr_offs += fifo.Write(temp_wr + wr_offs, kTestSize);
  EXPECT_EQ(kTestSize, wr_offs);

  // Read 1/2 of it
  rd_offs += fifo.Read(temp_rd + rd_offs, kHalfSize);
  EXPECT_EQ(kHalfSize, rd_offs);

  // Write the next two quaters.  The second
  // of these calls to write start at a wrapped
  // location 1/4 of the way into the fifo
  wr_offs += fifo.Write(temp_wr + wr_offs, kQuarterSize);
  EXPECT_EQ(kTestSize + kQuarterSize, wr_offs);
  wr_offs += fifo.Write(temp_wr + wr_offs, kQuarterSize);
  EXPECT_EQ(kTestSize + kHalfSize, wr_offs);

  // Finally read all the bytes we wrote.
  rd_offs += fifo.Read(temp_rd + rd_offs, kTestSize);
  EXPECT_EQ(kTestSize + kHalfSize, rd_offs);

  for (size_t i = 0; i < rd_offs; i++)
    ASSERT_EQ((char)i, temp_rd[i]) << "fifo mismatch at pos:" << i;
}

TEST(FIFOChar, Wrap) {
  char temp_wr[kTestSize * 2];
  char temp_rd[kTestSize * 2];
  size_t wr_offs = 0;
  size_t rd_offs = 0;

  FIFOChar fifo(kTestSize);

  memset(temp_rd, 0, sizeof(temp_rd));
  for (size_t index = 0; index < sizeof(temp_wr); index++)
    temp_wr[index] = index;

  EXPECT_TRUE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  // Wrap read and write differently, and verify copy is correct
  EXPECT_EQ(0, fifo.ReadAvailable());
  EXPECT_EQ(kTestSize, fifo.WriteAvailable());

  wr_offs += fifo.Write(temp_wr, kHalfSize);
  EXPECT_EQ(kHalfSize, wr_offs);

  EXPECT_FALSE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  rd_offs += fifo.Read(temp_rd, kQuarterSize);
  EXPECT_EQ(kQuarterSize, rd_offs);

  EXPECT_FALSE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  wr_offs += fifo.Write(&temp_wr[wr_offs], kTestSize);
  EXPECT_EQ(kTestSize + kQuarterSize, wr_offs);

  EXPECT_FALSE(fifo.IsEmpty());

  rd_offs += fifo.Read(&temp_rd[rd_offs], kTestSize);
  EXPECT_EQ(kTestSize + kQuarterSize, rd_offs);

  EXPECT_TRUE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  for (size_t index = 0; index < kQuarterSize + kTestSize; index++)
    EXPECT_EQ((char) index, temp_rd[index]);
}

TEST(FIFOPacket, Packets) {
  char temp_wr[kTestSize];
  FIFOPacket fifo(kTestSize);

  Packet pkt0(NULL);
  Packet pkt1(NULL);
  pkt0.Copy(temp_wr, kHalfSize, 0);
  pkt1.Copy(temp_wr, kTestSize, 0);

  EXPECT_TRUE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  EXPECT_EQ(0, fifo.ReadAvailable());
  EXPECT_EQ(kTestSize, fifo.WriteAvailable());

  fifo.WritePacket(&pkt0);
  EXPECT_FALSE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  EXPECT_EQ(kHalfSize, fifo.ReadAvailable());
  EXPECT_EQ(kHalfSize, fifo.WriteAvailable());

  fifo.WritePacket(&pkt1);
  EXPECT_FALSE(fifo.IsEmpty());
  EXPECT_TRUE(fifo.IsFull());

  EXPECT_EQ(kHalfSize + kTestSize, fifo.ReadAvailable());
  EXPECT_EQ(0, fifo.WriteAvailable());

  EXPECT_EQ(&pkt0, fifo.ReadPacket());
  EXPECT_FALSE(fifo.IsEmpty());
  EXPECT_TRUE(fifo.IsFull());

  EXPECT_EQ(kTestSize, fifo.ReadAvailable());
  EXPECT_EQ(0, fifo.WriteAvailable());

  EXPECT_EQ(&pkt1, fifo.ReadPacket());

  EXPECT_TRUE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  EXPECT_EQ(0, fifo.ReadAvailable());
  EXPECT_EQ(kTestSize, fifo.WriteAvailable());
}
