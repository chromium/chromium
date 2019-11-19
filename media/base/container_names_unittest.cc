// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/files/file_util.h"
#include "media/base/container_names.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace container_names {

// Using a macros to simplify tests. Since EXPECT_EQ outputs the second argument
// as a string when it fails, this lets the output identify what item actually
// failed.
#define VERIFY(buffer, name)                                                   \
  EXPECT_EQ(name, DetermineContainer(reinterpret_cast<const uint8_t*>(buffer), \
                                     sizeof(buffer)))

// Test that small buffers are handled correctly.
TEST(ContainerNamesTest, CheckSmallBuffer) {
  // Empty buffer.
  char buffer[1];  // ([0] not allowed on win)
  VERIFY(buffer, CONTAINER_UNKNOWN);

  // Try a simple SRT file.
  char buffer1[] =
      "1\n"
      "00:03:23,550 --> 00:03:24,375\n"
      "You always had a hard time finding your place in this world.\n"
      "\n"
      "2\n"
      "00:03:24,476 --> 00:03:25,175\n"
      "What are you talking about?\n";
  VERIFY(buffer1, CONTAINER_SRT);

  // HLS has it's own loop.
  char buffer2[] = "#EXTM3U"
                   "some other random stuff"
                   "#EXT-X-MEDIA-SEQUENCE:";
  VERIFY(buffer2, CONTAINER_HLS);

  // Try a large buffer all zeros.
  char buffer3[4096];
  memset(buffer3, 0, sizeof(buffer3));
  VERIFY(buffer3, CONTAINER_UNKNOWN);

  // Reuse buffer, but all \n this time.
  memset(buffer3, '\n', sizeof(buffer3));
  VERIFY(buffer3, CONTAINER_UNKNOWN);
}

#define BYTE_ORDER_MARK "\xef\xbb\xbf"

// Note that the comparisons need at least 12 bytes, so make sure the buffer is
// at least that size.
const char kAmrBuffer[12] = "#!AMR";
uint8_t kAsfBuffer[] = {0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
                        0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c};
const char kAss1Buffer[] = "[Script Info]";
const char kAss2Buffer[] = BYTE_ORDER_MARK "[Script Info]";
uint8_t kCafBuffer[] = {
    'c', 'a', 'f', 'f', 0,   1, 0, 0, 'd', 'e', 's', 'c', 0,   0, 0, 0, 0, 0, 0,
    32,  64,  229, 136, 128, 0, 0, 0, 0,   'a', 'a', 'c', ' ', 0, 0, 0, 2, 0, 0,
    0,   0,   0,   0,   4,   0, 0, 0, 0,   2,   0,   0,   0,   0};
const char kDtshdBuffer[12] = "DTSHDHDR";
const char kDxaBuffer[16] = "DEXA";
const char kFlacBuffer[12] = "fLaC";
uint8_t kFlvBuffer[12] = {'F', 'L', 'V', 0, 0, 0, 0, 1, 0, 0, 0, 0};
uint8_t kIrcamBuffer[] = {0x64, 0xa3, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1};
const char kRm1Buffer[12] = ".RMF\0\0";
const char kRm2Buffer[12] = ".ra\xfd";
uint8_t kWtvBuffer[] = {0xb7, 0xd8, 0x00, 0x20, 0x37, 0x49, 0xda, 0x11,
                        0xa6, 0x4e, 0x00, 0x07, 0xe9, 0x5e, 0xad, 0x8d};
uint8_t kBug263073Buffer[] = {
    0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70, 0x6d, 0x70, 0x34,
    0x32, 0x00, 0x00, 0x00, 0x00, 0x69, 0x73, 0x6f, 0x6d, 0x6d, 0x70,
    0x34, 0x32, 0x00, 0x00, 0x00, 0x01, 0x6d, 0x64, 0x61, 0x74, 0x00,
    0x00, 0x00, 0x00, 0xaa, 0x2e, 0x22, 0xcf, 0x00, 0x00, 0x00, 0x37,
    0x67, 0x64, 0x00, 0x28, 0xac, 0x2c, 0xa4, 0x01, 0xe0, 0x08, 0x9f,
    0x97, 0x01, 0x52, 0x02, 0x02, 0x02, 0x80, 0x00, 0x01};
uint8_t kBug584401Buffer[] = {
    0x1a, 0x45, 0xdf, 0xa3, 0x01, 0x00, 0x3b, 0x00, 0xb1, 0x00, 0x00, 0x1f,
    0x42, 0x86, 0x81, 0x01, 0x42, 0xf7, 0x81, 0x01, 0x42, 0xf2, 0x0b, 0x77,
    0x01, 0xb2, 0x74, 0x87, 0xc0, 0x00, 0x20, 0x84, 0x21, 0x08, 0x23, 0x00,
    0xae, 0x06, 0x06, 0x01, 0x81, 0x87, 0xbb, 0x8e, 0x0f, 0x9f, 0x3e, 0x7c,
    0xfa, 0x61, 0xeb, 0x8e, 0x97, 0x74, 0x37, 0xd0, 0x5f, 0x5c, 0x75, 0x1e,
    0x71, 0x30, 0xfe, 0x7c, 0xe0, 0xf9, 0xf3, 0xe7, 0xcf, 0x9f, 0x3e, 0x7c,
    0xf8, 0x00, 0xc7, 0x32, 0xe2, 0x31, 0xb9, 0x58, 0x1a, 0xe7, 0xf9, 0x98,
    0x0c, 0xdd, 0x6f, 0xd4, 0x20, 0xb7, 0xe4, 0xb4, 0x7e, 0xaa, 0xdf, 0x29,
    0xd6, 0x9d, 0x2b, 0x37, 0x2f, 0x7a, 0xc2, 0x5f, 0x52, 0x44, 0x01, 0xdc,
    0x32, 0x96, 0xbb, 0xcb, 0x25, 0x52, 0x52, 0xac, 0x6f, 0xb1, 0xbd, 0x85,
    0x02, 0x83, 0x7f, 0x5e, 0x43, 0x98, 0xa6, 0x81, 0x04, 0x42, 0xf3, 0x9b,
    0x81, 0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11, 0xa6, 0xd9, 0x00,
    0xaa, 0x00, 0x62, 0xce, 0x6c, 0x6d, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x02, 0xa1, 0xdc, 0xab, 0x8c, 0x47,
    0xa9, 0xcf, 0x11, 0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65, 0x68,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x42, 0x82, 0x84, 0x77, 0x2a,
    0x65, 0x62, 0x6d, 0x42, 0xbe, 0x4c, 0xda, 0xc9, 0x4c, 0xa5, 0x0f, 0x15,
    0xe8, 0xfd, 0x5b, 0x09, 0x8c, 0x38, 0xd7, 0x18, 0x0a, 0x68, 0x41, 0x46,
    0x63, 0x18, 0xf9, 0xf4, 0xcb, 0xc7, 0x57, 0x95, 0xd8, 0x0b, 0x2c, 0x91,
    0x70, 0x1b, 0x81, 0xd3, 0xda, 0xa0, 0x62, 0x87, 0x2d, 0x03, 0x50, 0x6d,
    0x26, 0xb1, 0xcc, 0xb8, 0x8c, 0x81, 0x6e, 0x56};
uint8_t kBug585243Buffer[] = {0x1a, 0x45, 0xdf, 0xa3, 0x01, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
                              0x42, 0x82, 0x42, 0x82, 0x00, 0x00};

// Test that containers that start with fixed strings are handled correctly.
// This is to verify that the TAG matches the first 4 characters of the string.
TEST(ContainerNamesTest, CheckFixedStrings) {
  VERIFY(kAmrBuffer, CONTAINER_AMR);
  VERIFY(kAsfBuffer, CONTAINER_ASF);
  VERIFY(kAss1Buffer, CONTAINER_ASS);
  VERIFY(kAss2Buffer, CONTAINER_ASS);
  VERIFY(kCafBuffer, CONTAINER_CAF);
  VERIFY(kDtshdBuffer, CONTAINER_DTSHD);
  VERIFY(kDxaBuffer, CONTAINER_DXA);
  VERIFY(kFlacBuffer, CONTAINER_FLAC);
  VERIFY(kFlvBuffer, CONTAINER_FLV);
  VERIFY(kIrcamBuffer, CONTAINER_IRCAM);
  VERIFY(kRm1Buffer, CONTAINER_RM);
  VERIFY(kRm2Buffer, CONTAINER_RM);
  VERIFY(kWtvBuffer, CONTAINER_WTV);
  VERIFY(kBug263073Buffer, CONTAINER_MOV);
  VERIFY(kBug584401Buffer, CONTAINER_EAC3);
  VERIFY(kBug585243Buffer, CONTAINER_UNKNOWN);
}

// Determine the container type of a specified file.
void TestFile(MediaContainerName expected, const base::FilePath& filename) {
  char buffer[8192];

  // Windows implementation of ReadFile fails if file smaller than desired size,
  // so use file length if file less than 8192 bytes (http://crbug.com/243885).
  int read_size = sizeof(buffer);
  int64_t actual_size;
  if (base::GetFileSize(filename, &actual_size) && actual_size < read_size)
    read_size = actual_size;
  int read = base::ReadFile(filename, buffer, read_size);

  // Now verify the type.
  EXPECT_EQ(expected,
            DetermineContainer(reinterpret_cast<const uint8_t*>(buffer), read))
      << "Failure with file " << filename.value();
}

TEST(ContainerNamesTest, FileCheckOGG) {
  TestFile(CONTAINER_OGG, GetTestDataFilePath("bear.ogv"));
  TestFile(CONTAINER_OGG, GetTestDataFilePath("9ch.ogg"));
}

TEST(ContainerNamesTest, FileCheckWAV) {
  TestFile(CONTAINER_WAV, GetTestDataFilePath("4ch.wav"));
  TestFile(CONTAINER_WAV, GetTestDataFilePath("sfx_f32le.wav"));
  TestFile(CONTAINER_WAV, GetTestDataFilePath("sfx_s16le.wav"));
}

TEST(ContainerNamesTest, FileCheckMOV) {
  TestFile(CONTAINER_MOV, GetTestDataFilePath("bear_rotate_90.mp4"));
  TestFile(CONTAINER_MOV, GetTestDataFilePath("bear-1280x720.mp4"));
  TestFile(CONTAINER_MOV, GetTestDataFilePath("crbug657437.mp4"));
  TestFile(CONTAINER_MOV, GetTestDataFilePath("sfx.m4a"));
}

TEST(ContainerNamesTest, FileCheckWEBM) {
  TestFile(CONTAINER_WEBM, GetTestDataFilePath("bear-320x240.webm"));
  TestFile(CONTAINER_WEBM, GetTestDataFilePath("no_streams.webm"));
  TestFile(CONTAINER_WEBM, GetTestDataFilePath("webm_ebml_element"));
}

TEST(ContainerNamesTest, FileCheckMP3) {
  TestFile(CONTAINER_MP3, GetTestDataFilePath("bear-audio-10s-CBR-no-TOC.mp3"));
  TestFile(CONTAINER_MP3, GetTestDataFilePath("id3_png_test.mp3"));
  TestFile(CONTAINER_MP3, GetTestDataFilePath("id3_test.mp3"));
  TestFile(CONTAINER_MP3, GetTestDataFilePath("midstream_config_change.mp3"));
  TestFile(CONTAINER_MP3, GetTestDataFilePath("sfx.mp3"));
}

TEST(ContainerNamesTest, FileCheckAC3) {
  TestFile(CONTAINER_AC3, GetTestDataFilePath("bear.ac3"));
}

TEST(ContainerNamesTest, FileCheckAAC) {
  TestFile(CONTAINER_AAC, GetTestDataFilePath("bear.adts"));
}

TEST(ContainerNamesTest, FileCheckAIFF) {
  TestFile(CONTAINER_AIFF, GetTestDataFilePath("bear.aiff"));
}

TEST(ContainerNamesTest, FileCheckASF) {
  TestFile(CONTAINER_ASF, GetTestDataFilePath("bear.asf"));
}

TEST(ContainerNamesTest, FileCheckAVI) {
  TestFile(CONTAINER_AVI, GetTestDataFilePath("bear.avi"));
}

TEST(ContainerNamesTest, FileCheckEAC3) {
  TestFile(CONTAINER_EAC3, GetTestDataFilePath("bear.eac3"));
}

TEST(ContainerNamesTest, FileCheckFLAC) {
  TestFile(CONTAINER_FLAC, GetTestDataFilePath("bear.flac"));
}

TEST(ContainerNamesTest, FileCheckFLV) {
  TestFile(CONTAINER_FLV, GetTestDataFilePath("bear.flv"));
}

TEST(ContainerNamesTest, FileCheckH261) {
  TestFile(CONTAINER_H261, GetTestDataFilePath("bear.h261"));
}

TEST(ContainerNamesTest, FileCheckH263) {
  TestFile(CONTAINER_H263, GetTestDataFilePath("bear.h263"));
}

TEST(ContainerNamesTest, FileCheckMJPEG) {
  TestFile(CONTAINER_MJPEG, GetTestDataFilePath("bear.mjpeg"));
}

TEST(ContainerNamesTest, FileCheckMPEG2PS) {
  TestFile(CONTAINER_MPEG2PS, GetTestDataFilePath("bear.mpeg"));
}

TEST(ContainerNamesTest, FileCheckMPEG2TS) {
  TestFile(CONTAINER_MPEG2TS, GetTestDataFilePath("bear.m2ts"));
}

TEST(ContainerNamesTest, FileCheckRM) {
  TestFile(CONTAINER_RM, GetTestDataFilePath("bear.rm"));
}

TEST(ContainerNamesTest, FileCheckSWF) {
  TestFile(CONTAINER_SWF, GetTestDataFilePath("bear.swf"));
}

// Try a few non containers.
TEST(ContainerNamesTest, FileCheckUNKNOWN) {
  TestFile(CONTAINER_UNKNOWN, GetTestDataFilePath("ten_byte_file"));
  TestFile(CONTAINER_UNKNOWN, GetTestDataFilePath("README"));
  TestFile(CONTAINER_UNKNOWN, GetTestDataFilePath("webm_vp8_track_entry"));
}

}  // namespace container_names

}  // namespace media
