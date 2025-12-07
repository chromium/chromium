// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp4/es_descriptor.h"

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace mp4 {

TEST(ESDescriptorTest, SingleByteLengthTest) {
  ESDescriptor es_desc;
  uint8_t buffer[] = {0x03, 0x19, 0x00, 0x01, 0x00, 0x04, 0x11, 0x40, 0x15,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x05, 0x02, 0x12, 0x10, 0x06, 0x01, 0x02};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_EQ(es_desc.object_type(), kForbidden);
  EXPECT_TRUE(es_desc.Parse(data));
  EXPECT_EQ(es_desc.object_type(), kISO_14496_3);
  EXPECT_EQ(es_desc.decoder_specific_info().size(), 2u);
  EXPECT_EQ(es_desc.decoder_specific_info()[0], 0x12);
  EXPECT_EQ(es_desc.decoder_specific_info()[1], 0x10);
}

TEST(ESDescriptorTest, NonAACTest) {
  ESDescriptor es_desc;
  uint8_t buffer[] = {0x03, 0x19, 0x00, 0x01, 0x00, 0x04, 0x11, 0x66, 0x15,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x05, 0x02, 0x12, 0x10, 0x06, 0x01, 0x02};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(es_desc.Parse(data));
  EXPECT_NE(es_desc.object_type(), kISO_14496_3);
  EXPECT_EQ(es_desc.decoder_specific_info().size(), 2u);
  EXPECT_EQ(es_desc.decoder_specific_info()[0], 0x12);
  EXPECT_EQ(es_desc.decoder_specific_info()[1], 0x10);
}

TEST(ESDescriptorTest, MultiByteLengthTest) {
  ESDescriptor es_desc;
  uint8_t buffer[] = {0x03, 0x80, 0x19, 0x00, 0x01, 0x00, 0x04, 0x80, 0x80,
                      0x11, 0x40, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x80, 0x80, 0x80,
                      0x02, 0x12, 0x10, 0x06, 0x01, 0x02};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(es_desc.Parse(data));
  EXPECT_EQ(es_desc.object_type(), kISO_14496_3);
  EXPECT_EQ(es_desc.decoder_specific_info().size(), 2u);
  EXPECT_EQ(es_desc.decoder_specific_info()[0], 0x12);
  EXPECT_EQ(es_desc.decoder_specific_info()[1], 0x10);
}

TEST(ESDescriptorTest, FiveByteLengthTest) {
  ESDescriptor es_desc;
  uint8_t buffer[] = {0x03, 0x80, 0x19, 0x00, 0x01, 0x00, 0x04, 0x80, 0x80,
                      0x11, 0x40, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x80, 0x80, 0x80,
                      0x80, 0x02, 0x12, 0x10, 0x06, 0x01, 0x02};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(es_desc.Parse(data));
  EXPECT_EQ(es_desc.object_type(), kISO_14496_3);
  EXPECT_EQ(es_desc.decoder_specific_info().size(), 0u);
}

}  // namespace mp4

}  // namespace media
