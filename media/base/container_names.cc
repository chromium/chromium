// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/container_names.h"

#include <stddef.h>
#include <string.h>

#include <limits>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/bit_reader.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace media {

namespace container_names {

#define TAG(a, b, c, d)                                     \
  ((static_cast<uint32_t>(static_cast<uint8_t>(a)) << 24) | \
   (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 16) | \
   (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 8) |  \
   (static_cast<uint32_t>(static_cast<uint8_t>(d))))

#define RCHECK(x)     \
    do {              \
      if (!(x))       \
        return false; \
    } while (0)

#define UTF8_BYTE_ORDER_MARK "\xef\xbb\xbf"

// Helper function to read 2 bytes (16 bits, big endian) from a buffer.
static int Read16(const uint8_t* p) {
  return p[0] << 8 | p[1];
}

// Helper function to read 3 bytes (24 bits, big endian) from a buffer.
static uint32_t Read24(const uint8_t* p) {
  return p[0] << 16 | p[1] << 8 | p[2];
}

// Helper function to read 4 bytes (32 bits, big endian) from a buffer.
static uint32_t Read32(const uint8_t* p) {
  return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

// Helper function to read 4 bytes (32 bits, little endian) from a buffer.
static uint32_t Read32LE(const uint8_t* p) {
  return p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0];
}

// Helper function to do buffer comparisons with a string without going off the
// end of the buffer.
static bool StartsWith(const uint8_t* buffer,
                       size_t buffer_size,
                       const char* prefix) {
  size_t prefix_size = strlen(prefix);
  return (prefix_size <= buffer_size &&
          memcmp(buffer, prefix, prefix_size) == 0);
}

// Helper function to do buffer comparisons with another buffer (to allow for
// embedded \0 in the comparison) without going off the end of the buffer.
static bool StartsWith(const uint8_t* buffer,
                       size_t buffer_size,
                       const uint8_t* prefix,
                       size_t prefix_size) {
  return (prefix_size <= buffer_size &&
          memcmp(buffer, prefix, prefix_size) == 0);
}

// Helper function to read up to 64 bits from a bit stream.
// TODO(chcunningham): Delete this helper and replace with direct calls to
// reader that handle read failure. As-is, we hide failure because returning 0
// is valid for both a successful and failed read.
static uint64_t ReadBits(BitReader* reader, int num_bits) {
  DCHECK_GE(reader->bits_available(), num_bits);
  DCHECK((num_bits > 0) && (num_bits <= 64));
  uint64_t value = 0;

  if (!reader->ReadBits(num_bits, &value))
    return 0;

  return value;
}

const int kAc3FrameSizeTable[38][3] = {
  { 128, 138, 192 }, { 128, 140, 192 }, { 160, 174, 240 }, { 160, 176, 240 },
  { 192, 208, 288 }, { 192, 210, 288 }, { 224, 242, 336 }, { 224, 244, 336 },
  { 256, 278, 384 }, { 256, 280, 384 }, { 320, 348, 480 }, { 320, 350, 480 },
  { 384, 416, 576 }, { 384, 418, 576 }, { 448, 486, 672 }, { 448, 488, 672 },
  { 512, 556, 768 }, { 512, 558, 768 }, { 640, 696, 960 }, { 640, 698, 960 },
  { 768, 834, 1152 }, { 768, 836, 1152 }, { 896, 974, 1344 },
  { 896, 976, 1344 }, { 1024, 1114, 1536 }, { 1024, 1116, 1536 },
  { 1280, 1392, 1920 }, { 1280, 1394, 1920 }, { 1536, 1670, 2304 },
  { 1536, 1672, 2304 }, { 1792, 1950, 2688 }, { 1792, 1952, 2688 },
  { 2048, 2228, 3072 }, { 2048, 2230, 3072 }, { 2304, 2506, 3456 },
  { 2304, 2508, 3456 }, { 2560, 2768, 3840 }, { 2560, 2770, 3840 }
};

// Checks for an ADTS AAC container.
static bool CheckAac(const uint8_t* buffer, int buffer_size) {
  // Audio Data Transport Stream (ADTS) header is 7 or 9 bytes
  // (from http://wiki.multimedia.cx/index.php?title=ADTS)
  RCHECK(buffer_size > 6);

  int offset = 0;
  while (offset + 6 < buffer_size) {
    BitReader reader(buffer + offset, 6);

    // Syncword must be 0xfff.
    RCHECK(ReadBits(&reader, 12) == 0xfff);

    // Skip MPEG version.
    reader.SkipBits(1);

    // Layer is always 0.
    RCHECK(ReadBits(&reader, 2) == 0);

    // Skip protection + profile.
    reader.SkipBits(1 + 2);

    // Check sampling frequency index.
    RCHECK(ReadBits(&reader, 4) != 15);  // Forbidden.

    // Skip private stream, channel configuration, originality, home,
    // copyrighted stream, and copyright_start.
    reader.SkipBits(1 + 3 + 1 + 1 + 1 + 1);

    // Get frame length (includes header).
    int size = ReadBits(&reader, 13);
    RCHECK(size > 0);
    offset += size;
  }
  return true;
}

const uint16_t kAc3SyncWord = 0x0b77;

// Checks for an AC3 container.
static bool CheckAc3(const uint8_t* buffer, int buffer_size) {
  // Reference: ATSC Standard: Digital Audio Compression (AC-3, E-AC-3)
  //            Doc. A/52:2012
  // (http://www.atsc.org/cms/standards/A52-2012(12-17).pdf)

  // AC3 container looks like syncinfo | bsi | audblk * 6 | aux | check.
  RCHECK(buffer_size > 6);

  int offset = 0;
  while (offset + 6 < buffer_size) {
    BitReader reader(buffer + offset, 6);

    // Check syncinfo.
    RCHECK(ReadBits(&reader, 16) == kAc3SyncWord);

    // Skip crc1.
    reader.SkipBits(16);

    // Verify fscod.
    int sample_rate_code = ReadBits(&reader, 2);
    RCHECK(sample_rate_code != 3);  // Reserved.

    // Verify frmsizecod.
    int frame_size_code = ReadBits(&reader, 6);
    RCHECK(frame_size_code < 38);  // Undefined.

    // Verify bsid.
    RCHECK(ReadBits(&reader, 5) < 10);  // Normally 8 or 6, 16 used by EAC3.

    offset += kAc3FrameSizeTable[frame_size_code][sample_rate_code];
  }
  return true;
}

// Checks for an EAC3 container (very similar to AC3)
static bool CheckEac3(const uint8_t* buffer, int buffer_size) {
  // Reference: ATSC Standard: Digital Audio Compression (AC-3, E-AC-3)
  //            Doc. A/52:2012
  // (http://www.atsc.org/cms/standards/A52-2012(12-17).pdf)

  // EAC3 container looks like syncinfo | bsi | audfrm | audblk* | aux | check.
  RCHECK(buffer_size > 6);

  int offset = 0;
  while (offset + 6 < buffer_size) {
    BitReader reader(buffer + offset, 6);

    // Check syncinfo.
    RCHECK(ReadBits(&reader, 16) == kAc3SyncWord);

    // Verify strmtyp.
    RCHECK(ReadBits(&reader, 2) != 3);

    // Skip substreamid.
    reader.SkipBits(3);

    // Get frmsize. Include syncinfo size and convert to bytes.
    int frame_size = (ReadBits(&reader, 11) + 1) * 2;
    RCHECK(frame_size >= 7);

    // Skip fscod, fscod2, acmod, and lfeon.
    reader.SkipBits(2 + 2 + 3 + 1);

    // Verify bsid.
    int bit_stream_id = ReadBits(&reader, 5);
    RCHECK(bit_stream_id >= 11 && bit_stream_id <= 16);

    offset += frame_size;
  }
  return true;
}

// Additional checks for a BINK container.
static bool CheckBink(const uint8_t* buffer, int buffer_size) {
  // Reference: http://wiki.multimedia.cx/index.php?title=Bink_Container
  RCHECK(buffer_size >= 44);

  // Verify number of frames specified.
  RCHECK(Read32LE(buffer + 8) > 0);

  // Verify width in range.
  int width = Read32LE(buffer + 20);
  RCHECK(width > 0 && width <= 32767);

  // Verify height in range.
  int height = Read32LE(buffer + 24);
  RCHECK(height > 0 && height <= 32767);

  // Verify frames per second specified.
  RCHECK(Read32LE(buffer + 28) > 0);

  // Verify video frames per second specified.
  RCHECK(Read32LE(buffer + 32) > 0);

  // Number of audio tracks must be 256 or less.
  return (Read32LE(buffer + 40) <= 256);
}

// Additional checks for a CAF container.
static bool CheckCaf(const uint8_t* buffer, int buffer_size) {
  // Reference: Apple Core Audio Format Specification 1.0
  // (https://developer.apple.com/library/mac/#documentation/MusicAudio/Reference/CAFSpec/CAF_spec/CAF_spec.html)
  RCHECK(buffer_size >= 52);
  BitReader reader(buffer, buffer_size);

  // mFileType should be "caff".
  RCHECK(ReadBits(&reader, 32) == TAG('c', 'a', 'f', 'f'));

  // mFileVersion should be 1.
  RCHECK(ReadBits(&reader, 16) == 1);

  // Skip mFileFlags.
  reader.SkipBits(16);

  // First chunk should be Audio Description chunk, size 32l.
  RCHECK(ReadBits(&reader, 32) == TAG('d', 'e', 's', 'c'));
  RCHECK(ReadBits(&reader, 64) == 32);

  // CAFAudioFormat.mSampleRate(float64) not 0
  RCHECK(ReadBits(&reader, 64) != 0);

  // CAFAudioFormat.mFormatID not 0
  RCHECK(ReadBits(&reader, 32) != 0);

  // Skip CAFAudioFormat.mBytesPerPacket and mFramesPerPacket.
  reader.SkipBits(32 + 32);

  // CAFAudioFormat.mChannelsPerFrame not 0
  RCHECK(ReadBits(&reader, 32) != 0);
  return true;
}

static bool kSamplingFrequencyValid[16] = { false, true, true, true, false,
                                            false, true, true, true, false,
                                            false, true, true, true, false,
                                            false };
static bool kExtAudioIdValid[8] = { true, false, true, false, false, false,
                                    true, false };

// Additional checks for a DTS container.
static bool CheckDts(const uint8_t* buffer, int buffer_size) {
  // Reference: ETSI TS 102 114 V1.3.1 (2011-08)
  // (http://www.etsi.org/deliver/etsi_ts/102100_102199/102114/01.03.01_60/ts_102114v010301p.pdf)
  RCHECK(buffer_size > 11);

  int offset = 0;
  while (offset + 11 < buffer_size) {
    BitReader reader(buffer + offset, 11);

    // Verify sync word.
    RCHECK(ReadBits(&reader, 32) == 0x7ffe8001);

    // Skip frame type and deficit sample count.
    reader.SkipBits(1 + 5);

    // Verify CRC present flag.
    RCHECK(ReadBits(&reader, 1) == 0);  // CPF must be 0.

    // Verify number of PCM sample blocks.
    RCHECK(ReadBits(&reader, 7) >= 5);

    // Verify primary frame byte size.
    int frame_size = ReadBits(&reader, 14);
    RCHECK(frame_size >= 95);

    // Skip audio channel arrangement.
    reader.SkipBits(6);

    // Verify core audio sampling frequency is an allowed value.
    size_t sampling_freq_index = ReadBits(&reader, 4);
    RCHECK(sampling_freq_index < std::size(kSamplingFrequencyValid));
    RCHECK(kSamplingFrequencyValid[sampling_freq_index]);

    // Verify transmission bit rate is valid.
    RCHECK(ReadBits(&reader, 5) <= 25);

    // Verify reserved field is 0.
    RCHECK(ReadBits(&reader, 1) == 0);

    // Skip dynamic range flag, time stamp flag, auxiliary data flag, and HDCD.
    reader.SkipBits(1 + 1 + 1 + 1);

    // Verify extension audio descriptor flag is an allowed value.
    size_t audio_id_index = ReadBits(&reader, 3);
    RCHECK(audio_id_index < std::size(kExtAudioIdValid));
    RCHECK(kExtAudioIdValid[audio_id_index]);

    // Skip extended coding flag and audio sync word insertion flag.
    reader.SkipBits(1 + 1);

    // Verify low frequency effects flag is an allowed value.
    RCHECK(ReadBits(&reader, 2) != 3);

    offset += frame_size + 1;
  }
  return true;
}

// Checks for a DV container.
static bool CheckDV(const uint8_t* buffer, int buffer_size) {
  // Reference: SMPTE 314M (Annex A has differences with IEC 61834).
  // (http://standards.smpte.org/content/978-1-61482-454-1/st-314-2005/SEC1.body.pdf)
  RCHECK(buffer_size > 11);

  int offset = 0;
  int current_sequence_number = -1;
  int last_block_number[6] = {0};
  while (offset + 11 < buffer_size) {
    BitReader reader(buffer + offset, 11);

    // Decode ID data. Sections 5, 6, and 7 are reserved.
    int section = ReadBits(&reader, 3);
    RCHECK(section < 5);

    // Next bit must be 1.
    RCHECK(ReadBits(&reader, 1) == 1);

    // Skip arbitrary bits.
    reader.SkipBits(4);

    int sequence_number = ReadBits(&reader, 4);

    // Skip FSC.
    reader.SkipBits(1);

    // Next 3 bits must be 1.
    RCHECK(ReadBits(&reader, 3) == 7);

    int block_number = ReadBits(&reader, 8);

    if (section == 0) {  // Header.
      // Validate the reserved bits in the next 8 bytes.
      reader.SkipBits(1);
      RCHECK(ReadBits(&reader, 1) == 0);
      RCHECK(ReadBits(&reader, 11) == 0x7ff);
      reader.SkipBits(4);
      RCHECK(ReadBits(&reader, 4) == 0xf);
      reader.SkipBits(4);
      RCHECK(ReadBits(&reader, 4) == 0xf);
      reader.SkipBits(4);
      RCHECK(ReadBits(&reader, 4) == 0xf);
      reader.SkipBits(3);
      RCHECK(ReadBits(&reader, 24) == 0xffffff);
      current_sequence_number = sequence_number;
      for (size_t i = 0; i < std::size(last_block_number); ++i)
        last_block_number[i] = -1;
    } else {
      // Sequence number must match (this will also fail if no header seen).
      RCHECK(sequence_number == current_sequence_number);
      // Block number should be increasing.
      RCHECK(block_number > last_block_number[section]);
      last_block_number[section] = block_number;
    }

    // Move to next block.
    offset += 80;
  }
  return true;
}


// Checks for a GSM container.
static bool CheckGsm(const uint8_t* buffer, int buffer_size) {
  // Reference: ETSI EN 300 961 V8.1.1
  // (http://www.etsi.org/deliver/etsi_en/300900_300999/300961/08.01.01_60/en_300961v080101p.pdf)
  // also http://tools.ietf.org/html/rfc3551#page-24
  // GSM files have a 33 byte block, only first 4 bits are fixed.
  RCHECK(buffer_size >= 1024);  // Need enough data to do a decent check.

  int offset = 0;
  while (offset < buffer_size) {
    // First 4 bits of each block are xD.
    RCHECK((buffer[offset] & 0xf0) == 0xd0);
    offset += 33;
  }
  return true;
}

// Advance to the first set of |num_bits| bits that match |start_code|. |offset|
// is the current location in the buffer, and is updated. |bytes_needed| is the
// number of bytes that must remain in the buffer when |start_code| is found.
// Returns true if start_code found (and enough space in the buffer after it),
// false otherwise.
static bool AdvanceToStartCode(const uint8_t* buffer,
                               int buffer_size,
                               int* offset,
                               int bytes_needed,
                               int num_bits,
                               uint32_t start_code) {
  DCHECK_GE(bytes_needed, 3);
  DCHECK_LE(num_bits, 24);  // Only supports up to 24 bits.

  // Create a mask to isolate |num_bits| bits, once shifted over.
  uint32_t bits_to_shift = 24 - num_bits;
  uint32_t mask = (1 << num_bits) - 1;
  while (*offset + bytes_needed < buffer_size) {
    uint32_t next = Read24(buffer + *offset);
    if (((next >> bits_to_shift) & mask) == start_code)
      return true;
    ++(*offset);
  }
  return false;
}

// Checks for an H.261 container.
static bool CheckH261(const uint8_t* buffer, int buffer_size) {
  // Reference: ITU-T Recommendation H.261 (03/1993)
  // (http://www.itu.int/rec/T-REC-H.261-199303-I/en)
  RCHECK(buffer_size > 16);

  int offset = 0;
  bool seen_start_code = false;
  while (true) {
    // Advance to picture_start_code, if there is one.
    if (!AdvanceToStartCode(buffer, buffer_size, &offset, 4, 20, 0x10)) {
      // No start code found (or off end of buffer), so success if
      // there was at least one valid header.
      return seen_start_code;
    }

    // Now verify the block. AdvanceToStartCode() made sure that there are
    // at least 4 bytes remaining in the buffer.
    BitReader reader(buffer + offset, buffer_size - offset);
    RCHECK(ReadBits(&reader, 20) == 0x10);

    // Skip the temporal reference and PTYPE.
    reader.SkipBits(5 + 6);

    // Skip any extra insertion information. Since this is open-ended, if we run
    // out of bits assume that the buffer is correctly formatted.
    int extra = ReadBits(&reader, 1);
    while (extra == 1) {
      if (!reader.SkipBits(8))
        return seen_start_code;
      if (!reader.ReadBits(1, &extra))
        return seen_start_code;
    }

    // Next should be a Group of Blocks start code. Again, if we run out of
    // bits, then assume that the buffer up to here is correct, and the buffer
    // just happened to end in the middle of a header.
    int next;
    if (!reader.ReadBits(16, &next))
      return seen_start_code;
    RCHECK(next == 1);

    // Move to the next block.
    seen_start_code = true;
    offset += 4;
  }
}

// Checks for an H.263 container.
static bool CheckH263(const uint8_t* buffer, int buffer_size) {
  // Reference: ITU-T Recommendation H.263 (01/2005)
  // (http://www.itu.int/rec/T-REC-H.263-200501-I/en)
  // header is PSC(22b) + TR(8b) + PTYPE(8+b).
  RCHECK(buffer_size > 16);

  int offset = 0;
  bool seen_start_code = false;
  while (true) {
    // Advance to picture_start_code, if there is one.
    if (!AdvanceToStartCode(buffer, buffer_size, &offset, 9, 22, 0x20)) {
      // No start code found (or off end of buffer), so success if
      // there was at least one valid header.
      return seen_start_code;
    }

    // Now verify the block. AdvanceToStartCode() made sure that there are
    // at least 9 bytes remaining in the buffer.
    BitReader reader(buffer + offset, 9);
    RCHECK(ReadBits(&reader, 22) == 0x20);

    // Skip the temporal reference.
    reader.SkipBits(8);

    // Verify that the first 2 bits of PTYPE are 10b.
    RCHECK(ReadBits(&reader, 2) == 2);

    // Skip the split screen indicator, document camera indicator, and full
    // picture freeze release.
    reader.SkipBits(1 + 1 + 1);

    // Verify Source Format.
    int format = ReadBits(&reader, 3);
    RCHECK(format != 0 && format != 6);  // Forbidden or reserved.

    if (format == 7) {
      // Verify full extended PTYPE.
      int ufep = ReadBits(&reader, 3);
      if (ufep == 1) {
        // Verify the optional part of PLUSPTYPE.
        format = ReadBits(&reader, 3);
        RCHECK(format != 0 && format != 7);  // Reserved.
        reader.SkipBits(11);
        // Next 4 bits should be b1000.
        RCHECK(ReadBits(&reader, 4) == 8);  // Not allowed.
      } else {
        RCHECK(ufep == 0);  // Only 0 and 1 allowed.
      }

      // Verify picture type code is not a reserved value.
      int picture_type_code = ReadBits(&reader, 3);
      RCHECK(picture_type_code != 6 && picture_type_code != 7);  // Reserved.

      // Skip picture resampling mode, reduced resolution mode,
      // and rounding type.
      reader.SkipBits(1 + 1 + 1);

      // Next 3 bits should be b001.
      RCHECK(ReadBits(&reader, 3) == 1);  // Not allowed.
    }

    // Move to the next block.
    seen_start_code = true;
    offset += 9;
  }
}

// Checks for an H.264 container.
static bool CheckH264(const uint8_t* buffer, int buffer_size) {
  // Reference: ITU-T Recommendation H.264 (01/2012)
  // (http://www.itu.int/rec/T-REC-H.264)
  // Section B.1: Byte stream NAL unit syntax and semantics.
  RCHECK(buffer_size > 4);

  int offset = 0;
  int parameter_count = 0;
  while (true) {
    // Advance to picture_start_code, if there is one.
    if (!AdvanceToStartCode(buffer, buffer_size, &offset, 4, 24, 1)) {
      // No start code found (or off end of buffer), so success if
      // there was at least one valid header.
      return parameter_count > 0;
    }

    // Now verify the block. AdvanceToStartCode() made sure that there are
    // at least 4 bytes remaining in the buffer.
    BitReader reader(buffer + offset, 4);
    RCHECK(ReadBits(&reader, 24) == 1);

    // Verify forbidden_zero_bit.
    RCHECK(ReadBits(&reader, 1) == 0);

    // Extract nal_ref_idc and nal_unit_type.
    int nal_ref_idc = ReadBits(&reader, 2);
    int nal_unit_type = ReadBits(&reader, 5);

    switch (nal_unit_type) {
      case 5:  // Coded slice of an IDR picture.
        RCHECK(nal_ref_idc != 0);
        break;
      case 6:   // Supplemental enhancement information (SEI).
      case 9:   // Access unit delimiter.
      case 10:  // End of sequence.
      case 11:  // End of stream.
      case 12:  // Filler data.
        RCHECK(nal_ref_idc == 0);
        break;
      case 7:  // Sequence parameter set.
      case 8:  // Picture parameter set.
        ++parameter_count;
        break;
    }

    // Skip the current start_code_prefix and move to the next.
    offset += 4;
  }
}

static const char kHlsSignature[] = "#EXTM3U";
static const char kHls1[] = "#EXT-X-STREAM-INF:";
static const char kHls2[] = "#EXT-X-TARGETDURATION:";
static const char kHls3[] = "#EXT-X-MEDIA-SEQUENCE:";

// Additional checks for a HLS container.
static bool CheckHls(const uint8_t* buffer, int buffer_size) {
  // HLS is simply a play list used for Apple HTTP Live Streaming.
  // Reference: Apple HTTP Live Streaming Overview
  // (http://goo.gl/MIwxj)

  if (StartsWith(buffer, buffer_size, kHlsSignature)) {
    // Need to find "#EXT-X-STREAM-INF:", "#EXT-X-TARGETDURATION:", or
    // "#EXT-X-MEDIA-SEQUENCE:" somewhere in the buffer. Other playlists (like
    // WinAmp) only have additional lines with #EXTINF
    // (http://en.wikipedia.org/wiki/M3U).
    int offset = strlen(kHlsSignature);
    while (offset < buffer_size) {
      if (buffer[offset] == '#') {
        if (StartsWith(buffer + offset, buffer_size - offset, kHls1) ||
            StartsWith(buffer + offset, buffer_size - offset, kHls2) ||
            StartsWith(buffer + offset, buffer_size - offset, kHls3)) {
          return true;
        }
      }
      ++offset;
    }
  }
  return false;
}

// Checks for a MJPEG stream.
static bool CheckMJpeg(const uint8_t* buffer, int buffer_size) {
  // Reference: ISO/IEC 10918-1 : 1993(E), Annex B
  // (http://www.w3.org/Graphics/JPEG/itu-t81.pdf)
  RCHECK(buffer_size >= 16);

  int offset = 0;
  int last_restart = -1;
  int num_codes = 0;
  while (offset + 5 < buffer_size) {
    // Marker codes are always a two byte code with the first byte xFF.
    RCHECK(buffer[offset] == 0xff);
    uint8_t code = buffer[offset + 1];
    RCHECK(code >= 0xc0 || code == 1);

    // Skip sequences of xFF.
    if (code == 0xff) {
      ++offset;
      continue;
    }

    // Success if the next marker code is EOI (end of image)
    if (code == 0xd9)
      return true;

    // Check remaining codes.
    if (code == 0xd8 || code == 1) {
      // SOI (start of image) / TEM (private use). No other data with header.
      offset += 2;
    } else if (code >= 0xd0 && code <= 0xd7) {
      // RST (restart) codes must be in sequence. No other data with header.
      int restart = code & 0x07;
      if (last_restart >= 0)
        RCHECK(restart == (last_restart + 1) % 8);
      last_restart = restart;
      offset += 2;
    } else {
      // All remaining marker codes are followed by a length of the header.
      int length = Read16(buffer + offset + 2) + 2;

      // Special handling of SOS (start of scan) marker since the entropy
      // coded data follows the SOS. Any xFF byte in the data block must be
      // followed by x00 in the data.
      if (code == 0xda) {
        int number_components = buffer[offset + 4];
        RCHECK(length == 8 + 2 * number_components);

        // Advance to the next marker.
        offset += length;
        while (offset + 2 < buffer_size) {
          if (buffer[offset] == 0xff && buffer[offset + 1] != 0)
            break;
          ++offset;
        }
      } else {
        // Skip over the marker data for the other marker codes.
        offset += length;
      }
    }
    ++num_codes;
  }
  return (num_codes > 1);
}

enum Mpeg2StartCodes {
  PROGRAM_END_CODE = 0xb9,
  PACK_START_CODE = 0xba
};

// Checks for a MPEG2 Program Stream.
static bool CheckMpeg2ProgramStream(const uint8_t* buffer, int buffer_size) {
  // Reference: ISO/IEC 13818-1 : 2000 (E) / ITU-T Rec. H.222.0 (2000 E).
  RCHECK(buffer_size > 14);

  int offset = 0;
  while (offset + 14 < buffer_size) {
    BitReader reader(buffer + offset, 14);

    // Must start with pack_start_code.
    RCHECK(ReadBits(&reader, 24) == 1);
    RCHECK(ReadBits(&reader, 8) == PACK_START_CODE);

    // Determine MPEG version (MPEG1 has b0010, while MPEG2 has b01).
    int mpeg_version = ReadBits(&reader, 2);
    if (mpeg_version == 0) {
      // MPEG1, 10 byte header
      // Validate rest of version code
      RCHECK(ReadBits(&reader, 2) == 2);
    } else {
      RCHECK(mpeg_version == 1);
    }

    // Skip system_clock_reference_base [32..30].
    reader.SkipBits(3);

    // Verify marker bit.
    RCHECK(ReadBits(&reader, 1) == 1);

    // Skip system_clock_reference_base [29..15].
    reader.SkipBits(15);

    // Verify next marker bit.
    RCHECK(ReadBits(&reader, 1) == 1);

    // Skip system_clock_reference_base [14..0].
    reader.SkipBits(15);

    // Verify next marker bit.
    RCHECK(ReadBits(&reader, 1) == 1);

    if (mpeg_version == 0) {
      // Verify second marker bit.
      RCHECK(ReadBits(&reader, 1) == 1);

      // Skip mux_rate.
      reader.SkipBits(22);

      // Verify next marker bit.
      RCHECK(ReadBits(&reader, 1) == 1);

      // Update offset to be after this header.
      offset += 12;
    } else {
      // Must be MPEG2.
      // Skip program_mux_rate.
      reader.SkipBits(22);

      // Verify pair of marker bits.
      RCHECK(ReadBits(&reader, 2) == 3);

      // Skip reserved.
      reader.SkipBits(5);

      // Update offset to be after this header.
      int pack_stuffing_length = ReadBits(&reader, 3);
      offset += 14 + pack_stuffing_length;
    }

    // Check for system headers and PES_packets.
    while (offset + 6 < buffer_size && Read24(buffer + offset) == 1) {
      // Next 8 bits determine stream type.
      int stream_id = buffer[offset + 3];

      // Some stream types are reserved and shouldn't occur.
      if (mpeg_version == 0)
        RCHECK(stream_id != 0xbc && stream_id < 0xf0);
      else
        RCHECK(stream_id != 0xfc && stream_id != 0xfd && stream_id != 0xfe);

      // Some stream types are used for pack headers.
      if (stream_id == PACK_START_CODE)  // back to outer loop.
        break;
      if (stream_id == PROGRAM_END_CODE)  // end of stream.
        return true;

      int pes_length = Read16(buffer + offset + 4);
      RCHECK(pes_length > 0);
      offset = offset + 6 + pes_length;
    }
  }
  // Success as we are off the end of the buffer and liked everything
  // in the buffer.
  return true;
}

const uint8_t kMpeg2SyncWord = 0x47;

// Checks for a MPEG2 Transport Stream.
static bool CheckMpeg2TransportStream(const uint8_t* buffer, int buffer_size) {
  // Spec: ISO/IEC 13818-1 : 2000 (E) / ITU-T Rec. H.222.0 (2000 E).
  // Normal packet size is 188 bytes. However, some systems add various error
  // correction data at the end, resulting in packet of length 192/204/208
  // (https://en.wikipedia.org/wiki/MPEG_transport_stream). Determine the
  // length with the first packet.
  RCHECK(buffer_size >= 250);  // Want more than 1 packet to check.

  int offset = 0;
  int packet_length = -1;
  while (buffer[offset] != kMpeg2SyncWord && offset < 20) {
    // Skip over any header in the first 20 bytes.
    ++offset;
  }

  while (offset + 6 < buffer_size) {
    BitReader reader(buffer + offset, 6);

    // Must start with sync byte.
    RCHECK(ReadBits(&reader, 8) == kMpeg2SyncWord);

    // Skip transport_error_indicator, payload_unit_start_indicator, and
    // transport_priority.
    reader.SkipBits(1 + 1 + 1);

    // Verify the pid is not a reserved value.
    int pid = ReadBits(&reader, 13);
    RCHECK(pid < 3 || pid > 15);

    // Skip transport_scrambling_control.
    reader.SkipBits(2);

    // Adaptation_field_control can not be 0.
    int adaptation_field_control = ReadBits(&reader, 2);
    RCHECK(adaptation_field_control != 0);

    // If there is an adaptation_field, verify it.
    if (adaptation_field_control >= 2) {
      // Skip continuity_counter.
      reader.SkipBits(4);

      // Get adaptation_field_length and verify it.
      int adaptation_field_length = ReadBits(&reader, 8);
      if (adaptation_field_control == 2)
        RCHECK(adaptation_field_length == 183);
      else
        RCHECK(adaptation_field_length <= 182);
    }

    // Attempt to determine the packet length on the first packet.
    if (packet_length < 0) {
      if (buffer[offset + 188] == kMpeg2SyncWord)
        packet_length = 188;
      else if (buffer[offset + 192] == kMpeg2SyncWord)
        packet_length = 192;
      else if (buffer[offset + 204] == kMpeg2SyncWord)
        packet_length = 204;
      else
        packet_length = 208;
    }
    offset += packet_length;
  }
  return true;
}

enum Mpeg4StartCodes {
  VISUAL_OBJECT_SEQUENCE_START_CODE = 0xb0,
  VISUAL_OBJECT_SEQUENCE_END_CODE = 0xb1,
  VISUAL_OBJECT_START_CODE = 0xb5,
  VOP_START_CODE = 0xb6
};

// Checks for a raw MPEG4 bitstream container.
static bool CheckMpeg4BitStream(const uint8_t* buffer, int buffer_size) {
  // Defined in ISO/IEC 14496-2:2001.
  // However, no length ... simply scan for start code values.
  // Note tags are very similar to H.264.
  RCHECK(buffer_size > 4);

  int offset = 0;
  int sequence_start_count = 0;
  int sequence_end_count = 0;
  int visual_object_count = 0;
  int vop_count = 0;
  while (true) {
    // Advance to start_code, if there is one.
    if (!AdvanceToStartCode(buffer, buffer_size, &offset, 6, 24, 1)) {
      // Not a complete sequence in memory, so return true if we've seen a
      // visual_object_sequence_start_code and a visual_object_start_code.
      return (sequence_start_count > 0 && visual_object_count > 0);
    }

    // Now verify the block. AdvanceToStartCode() made sure that there are
    // at least 6 bytes remaining in the buffer.
    BitReader reader(buffer + offset, 6);
    RCHECK(ReadBits(&reader, 24) == 1);

    int start_code = ReadBits(&reader, 8);
    RCHECK(start_code < 0x30 || start_code > 0xaf);  // 30..AF and
    RCHECK(start_code < 0xb7 || start_code > 0xb9);  // B7..B9 reserved

    switch (start_code) {
      case VISUAL_OBJECT_SEQUENCE_START_CODE: {
        ++sequence_start_count;
        // Verify profile in not one of many reserved values.
        int profile = ReadBits(&reader, 8);
        RCHECK(profile > 0);
        RCHECK(profile < 0x04 || profile > 0x10);
        RCHECK(profile < 0x13 || profile > 0x20);
        RCHECK(profile < 0x23 || profile > 0x31);
        RCHECK(profile < 0x35 || profile > 0x41);
        RCHECK(profile < 0x43 || profile > 0x60);
        RCHECK(profile < 0x65 || profile > 0x70);
        RCHECK(profile < 0x73 || profile > 0x80);
        RCHECK(profile < 0x83 || profile > 0x90);
        RCHECK(profile < 0x95 || profile > 0xa0);
        RCHECK(profile < 0xa4 || profile > 0xb0);
        RCHECK(profile < 0xb5 || profile > 0xc0);
        RCHECK(profile < 0xc3 || profile > 0xd0);
        RCHECK(profile < 0xe4);
        break;
      }

      case VISUAL_OBJECT_SEQUENCE_END_CODE:
        RCHECK(++sequence_end_count == sequence_start_count);
        break;

      case VISUAL_OBJECT_START_CODE: {
        ++visual_object_count;
        if (ReadBits(&reader, 1) == 1) {
          int visual_object_verid = ReadBits(&reader, 4);
          RCHECK(visual_object_verid > 0 && visual_object_verid < 3);
          RCHECK(ReadBits(&reader, 3) != 0);
        }
        int visual_object_type = ReadBits(&reader, 4);
        RCHECK(visual_object_type > 0 && visual_object_type < 6);
        break;
      }

      case VOP_START_CODE:
        RCHECK(++vop_count <= visual_object_count);
        break;
    }
    // Skip this block.
    offset += 6;
  }
}

// Additional checks for a MOV/QuickTime/MPEG4 container.
static bool CheckMov(const uint8_t* buffer, int buffer_size) {
  // Reference: ISO/IEC 14496-12:2005(E).
  // (http://standards.iso.org/ittf/PubliclyAvailableStandards/c061988_ISO_IEC_14496-12_2012.zip)
  RCHECK(buffer_size > 8);

  int offset = 0;
  int valid_top_level_boxes = 0;
  while (offset + 8 < buffer_size) {
    uint32_t atomsize = Read32(buffer + offset);
    uint32_t atomtype = Read32(buffer + offset + 4);

    // Only need to check for atoms that are valid at the top level. However,
    // "Boxes with an unrecognized type shall be ignored and skipped." So
    // simply make sure that at least two recognized top level boxes are found.
    // This list matches BoxReader::IsValidTopLevelBox().
    switch (atomtype) {
      case TAG('f', 't', 'y', 'p'):
      case TAG('p', 'd', 'i', 'n'):
      case TAG('b', 'l', 'o', 'c'):
      case TAG('m', 'o', 'o', 'v'):
      case TAG('m', 'o', 'o', 'f'):
      case TAG('m', 'f', 'r', 'a'):
      case TAG('m', 'd', 'a', 't'):
      case TAG('f', 'r', 'e', 'e'):
      case TAG('s', 'k', 'i', 'p'):
      case TAG('m', 'e', 't', 'a'):
      case TAG('m', 'e', 'c', 'o'):
      case TAG('s', 't', 'y', 'p'):
      case TAG('s', 'i', 'd', 'x'):
      case TAG('s', 's', 'i', 'x'):
      case TAG('p', 'r', 'f', 't'):
      case TAG('u', 'u', 'i', 'd'):
      case TAG('e', 'm', 's', 'g'):
        ++valid_top_level_boxes;
        break;
    }
    if (atomsize == 1) {
      // Indicates that the length is the next 64bits.
      if (offset + 16 > buffer_size)
        break;
      if (Read32(buffer + offset + 8) != 0)
        break;  // Offset is way past buffer size.
      atomsize = Read32(buffer + offset + 12);
    }
    if (atomsize == 0 || atomsize > static_cast<size_t>(buffer_size))
      break;  // Indicates the last atom or length too big.
    offset += atomsize;
  }
  return valid_top_level_boxes >= 2;
}

enum MPEGVersion {
  VERSION_25 = 0,
  VERSION_RESERVED,
  VERSION_2,
  VERSION_1
};
enum MPEGLayer {
  L_RESERVED = 0,
  LAYER_3,
  LAYER_2,
  LAYER_1
};

static int kSampleRateTable[4][4] = { { 11025, 12000, 8000, 0 },   // v2.5
                                      { 0, 0, 0, 0 },              // not used
                                      { 22050, 24000, 16000, 0 },  // v2
                                      { 44100, 48000, 32000, 0 }   // v1
};

static int kBitRateTableV1L1[16] = { 0, 32, 64, 96, 128, 160, 192, 224, 256,
                                     288, 320, 352, 384, 416, 448, 0 };
static int kBitRateTableV1L2[16] = { 0, 32, 48, 56, 64, 80, 96, 112, 128, 160,
                                     192, 224, 256, 320, 384, 0 };
static int kBitRateTableV1L3[16] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128,
                                     160, 192, 224, 256, 320, 0 };
static int kBitRateTableV2L1[16] = { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144,
                                     160, 176, 192, 224, 256, 0 };
static int kBitRateTableV2L23[16] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96,
                                      112, 128, 144, 160, 0 };

static bool ValidMpegAudioFrameHeader(const uint8_t* header,
                                      int header_size,
                                      int* framesize) {
  // Reference: http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm.
  DCHECK_GE(header_size, 4);
  *framesize = 0;
  BitReader reader(header, 4);  // Header can only be 4 bytes long.

  // Verify frame sync (11 bits) are all set.
  RCHECK(ReadBits(&reader, 11) == 0x7ff);

  // Verify MPEG audio version id.
  int version = ReadBits(&reader, 2);
  RCHECK(version != 1);  // Reserved.

  // Verify layer.
  int layer = ReadBits(&reader, 2);
  RCHECK(layer != 0);

  // Skip protection bit.
  reader.SkipBits(1);

  // Verify bitrate index.
  int bitrate_index = ReadBits(&reader, 4);
  RCHECK(bitrate_index != 0xf);

  // Verify sampling rate frequency index.
  int sampling_index = ReadBits(&reader, 2);
  RCHECK(sampling_index != 3);

  // Get padding bit.
  int padding = ReadBits(&reader, 1);

  // Frame size:
  // For Layer I files = (12 * BitRate / SampleRate + Padding) * 4
  // For others = 144 * BitRate / SampleRate + Padding
  // Unfortunately, BitRate and SampleRate are coded.
  int sampling_rate = kSampleRateTable[version][sampling_index];
  int bitrate;
  if (version == VERSION_1) {
    if (layer == LAYER_1)
      bitrate = kBitRateTableV1L1[bitrate_index];
    else if (layer == LAYER_2)
      bitrate = kBitRateTableV1L2[bitrate_index];
    else
      bitrate = kBitRateTableV1L3[bitrate_index];
  } else {
    if (layer == LAYER_1)
      bitrate = kBitRateTableV2L1[bitrate_index];
    else
      bitrate = kBitRateTableV2L23[bitrate_index];
  }
  if (layer == LAYER_1)
    *framesize = ((12000 * bitrate) / sampling_rate + padding) * 4;
  else
    *framesize = (144000 * bitrate) / sampling_rate + padding;
  return (bitrate > 0 && sampling_rate > 0);
}

// Additional checks for a MP3 container.
static bool CheckMp3(const uint8_t* buffer, int buffer_size) {
  // This function assumes that the ID3 header is not present in the file and
  // simply checks for several valid MPEG audio buffers after skipping any
  // optional padding characters.
  int numSeen = 0;
  int offset = 0;

  // Skip over any padding (0's).
  while (offset < buffer_size && buffer[offset] == 0)
    ++offset;

  while (offset + 3 < buffer_size) {
    int framesize;
    RCHECK(ValidMpegAudioFrameHeader(
        buffer + offset, buffer_size - offset, &framesize));

    // Have we seen enough valid headers?
    if (++numSeen > 10)
      return true;
    offset += framesize;
  }
  // Off the end of the buffer, return success if a few valid headers seen.
  return numSeen > 2;
}

// Check that the next characters in |buffer| represent a number. The format
// accepted is optional whitespace followed by 1 or more digits. |max_digits|
// specifies the maximum number of digits to process. Returns true if a valid
// number is found, false otherwise.
static bool VerifyNumber(const uint8_t* buffer,
                         int buffer_size,
                         int* offset,
                         int max_digits) {
  RCHECK(*offset < buffer_size);

  // Skip over any leading space.
  while (absl::ascii_isspace(buffer[*offset])) {
    ++(*offset);
    RCHECK(*offset < buffer_size);
  }

  // Need to process up to max_digits digits.
  int numSeen = 0;
  while (--max_digits >= 0 && absl::ascii_isdigit(buffer[*offset])) {
    ++numSeen;
    ++(*offset);
    if (*offset >= buffer_size)
      return true;  // Out of space but seen a digit.
  }

  // Success if at least one digit seen.
  return (numSeen > 0);
}

// Check that the next character in |buffer| is one of |c1| or |c2|. |c2| is
// optional. Returns true if there is a match, false if no match or out of
// space.
static inline bool VerifyCharacters(const uint8_t* buffer,
                                    int buffer_size,
                                    int* offset,
                                    char c1,
                                    char c2) {
  RCHECK(*offset < buffer_size);
  char c = static_cast<char>(buffer[(*offset)++]);
  return (c == c1 || (c == c2 && c2 != 0));
}

// Checks for a SRT container.
static bool CheckSrt(const uint8_t* buffer, int buffer_size) {
  // Reference: http://en.wikipedia.org/wiki/SubRip
  RCHECK(buffer_size > 20);

  // First line should just be the subtitle sequence number.
  int offset = StartsWith(buffer, buffer_size, UTF8_BYTE_ORDER_MARK) ? 3 : 0;
  RCHECK(VerifyNumber(buffer, buffer_size, &offset, 100));
  RCHECK(VerifyCharacters(buffer, buffer_size, &offset, '\n', '\r'));

  // Skip any additional \n\r.
  while (VerifyCharacters(buffer, buffer_size, &offset, '\n', '\r')) {}
  --offset;  // Since VerifyCharacters() gobbled up the next non-CR/LF.

  // Second line should look like the following:
  //   00:00:10,500 --> 00:00:13,000
  // Units separator can be , or .
  RCHECK(VerifyNumber(buffer, buffer_size, &offset, 100));
  RCHECK(VerifyCharacters(buffer, buffer_size, &offset, ':', 0));
  RCHECK(VerifyNumber(buffer, buffer_size, &offset, 2));
  RCHECK(VerifyCharacters(buffer, buffer_size, &offset, ':', 0));
  RCHECK(VerifyNumber(buffer, buffer_size, &offset, 2));
  RCHECK(VerifyCharacters(buffer, buffer_size, &offset, ',', '.'));
  RCHECK(VerifyNumber(buffer, buffer_size, &offset, 3));
  RCHECK(VerifyCharacters(buffer, buffer_size, &offset, ' ', 0));
  RCHECK(VerifyCharacters(buffer, buffer_size, &offset, '-', 0));
  RCHECK(VerifyCharacters(buffer, buffer_size, &offset, '-', 0));
  RCHECK(VerifyCharacters(buffer, buffer_size, &offset, '>', 0));
  RCHECK(VerifyCharacters(buffer, buffer_size, &offset, ' ', 0));
  RCHECK(VerifyNumber(buffer, buffer_size, &offset, 100));
  RCHECK(VerifyCharacters(buffer, buffer_size, &offset, ':', 0));
  RCHECK(VerifyNumber(buffer, buffer_size, &offset, 2));
  RCHECK(VerifyCharacters(buffer, buffer_size, &offset, ':', 0));
  RCHECK(VerifyNumber(buffer, buffer_size, &offset, 2));
  RCHECK(VerifyCharacters(buffer, buffer_size, &offset, ',', '.'));
  RCHECK(VerifyNumber(buffer, buffer_size, &offset, 3));
  return true;
}

// Read a Matroska Element Id.
static int GetElementId(BitReader* reader) {
  // Element ID is coded with the leading zero bits (max 3) determining size.
  // If it is an invalid encoding or the end of the buffer is reached,
  // return -1 as a tag that won't be expected.
  if (reader->bits_available() >= 8) {
    int num_bits_to_read = 0;
    static int prefix[] = { 0x80, 0x4000, 0x200000, 0x10000000 };
    for (int i = 0; i < 4; ++i) {
      num_bits_to_read += 7;
      if (ReadBits(reader, 1) == 1) {
        if (reader->bits_available() < num_bits_to_read)
          break;
        // prefix[] adds back the bits read individually.
        return ReadBits(reader, num_bits_to_read) | prefix[i];
      }
    }
  }
  // Invalid encoding, return something not expected.
  return -1;
}

// Read a Matroska Unsigned Integer (VINT).
static uint64_t GetVint(BitReader* reader) {
  // Values are coded with the leading zero bits (max 7) determining size.
  // If it is an invalid coding or the end of the buffer is reached,
  // return something that will go off the end of the buffer.
  if (reader->bits_available() >= 8) {
    int num_bits_to_read = 0;
    for (int i = 0; i < 8; ++i) {
      num_bits_to_read += 7;
      if (ReadBits(reader, 1) == 1) {
        if (reader->bits_available() < num_bits_to_read)
          break;
        return ReadBits(reader, num_bits_to_read);
      }
    }
  }
  // Incorrect format (more than 7 leading 0's) or off the end of the buffer.
  // Since the return value is used as a byte size, return a value that will
  // cause a failure when used.
  return (reader->bits_available() / 8) + 2;
}

// Additional checks for a WEBM container.
static bool CheckWebm(const uint8_t* buffer, int buffer_size) {
  // Reference: http://www.matroska.org/technical/specs/index.html
  RCHECK(buffer_size > 12);

  BitReader reader(buffer, buffer_size);

  // Verify starting Element Id.
  RCHECK(GetElementId(&reader) == 0x1a45dfa3);

  // Get the header size, and ensure there are enough bits to check.
  // Using saturated_cast<> in case the size read is really large
  // (in which case the bits_available() check will fail).
  int header_size = base::saturated_cast<int>(GetVint(&reader));
  RCHECK(reader.bits_available() / 8 >= header_size);

  // Loop through the header.
  while (reader.bits_available() > 0) {
    int tag = GetElementId(&reader);
    int tagsize = base::saturated_cast<int>(GetVint(&reader));
    switch (tag) {
      case 0x4286:  // EBMLVersion
      case 0x42f7:  // EBMLReadVersion
      case 0x42f2:  // EBMLMaxIdLength
      case 0x42f3:  // EBMLMaxSizeLength
      case 0x4287:  // DocTypeVersion
      case 0x4285:  // DocTypeReadVersion
      case 0xec:    // void
      case 0xbf:    // CRC32
        RCHECK(reader.bits_available() / 8 >= tagsize);
        RCHECK(reader.SkipBits(tagsize * 8));
        break;

      case 0x4282:  // EBMLDocType
        // Need to see "webm" or "matroska" next.
        RCHECK(reader.bits_available() >= 32);
        switch (ReadBits(&reader, 32)) {
          case TAG('w', 'e', 'b', 'm') :
            return true;
          case TAG('m', 'a', 't', 'r') :
            RCHECK(reader.bits_available() >= 32);
            return (ReadBits(&reader, 32) == TAG('o', 's', 'k', 'a'));
        }
        return false;

      default:  // Unrecognized tag
        return false;
    }
  }
  return false;
}

enum VC1StartCodes {
  VC1_FRAME_START_CODE = 0x0d,
  VC1_ENTRY_POINT_START_CODE = 0x0e,
  VC1_SEQUENCE_START_CODE = 0x0f
};

// Checks for a VC1 bitstream container.
static bool CheckVC1(const uint8_t* buffer, int buffer_size) {
  // Reference: SMPTE 421M
  // (http://standards.smpte.org/content/978-1-61482-555-5/st-421-2006/SEC1.body.pdf)
  // However, no length ... simply scan for start code values.
  // Expect to see SEQ | [ [ ENTRY ] PIC* ]*
  // Note tags are very similar to H.264.

  RCHECK(buffer_size >= 24);

  // First check for Bitstream Metadata Serialization (Annex L)
  if (buffer[0] == 0xc5 &&
      Read32(buffer + 4) == 0x04 &&
      Read32(buffer + 20) == 0x0c) {
    // Verify settings in STRUCT_C and STRUCT_A
    BitReader reader(buffer + 8, 12);

    int profile = ReadBits(&reader, 4);
    if (profile == 0 || profile == 4) {  // simple or main
      // Skip FRMRTQ_POSTPROC, BITRTQ_POSTPROC, and LOOPFILTER.
      reader.SkipBits(3 + 5 + 1);

      // Next bit must be 0.
      RCHECK(ReadBits(&reader, 1) == 0);

      // Skip MULTIRES.
      reader.SkipBits(1);

      // Next bit must be 1.
      RCHECK(ReadBits(&reader, 1) == 1);

      // Skip FASTUVMC, EXTENDED_MV, DQUANT, and VSTRANSFORM.
      reader.SkipBits(1 + 1 + 2 + 1);

      // Next bit must be 0.
      RCHECK(ReadBits(&reader, 1) == 0);

      // Skip OVERLAP, SYNCMARKER, RANGERED, MAXBFRAMES, QUANTIZER, and
      // FINTERPFLAG.
      reader.SkipBits(1 + 1 + 1 + 3 + 2 + 1);

      // Next bit must be 1.
      RCHECK(ReadBits(&reader, 1) == 1);

    } else {
      RCHECK(profile == 12);  // Other profile values not allowed.
      RCHECK(ReadBits(&reader, 28) == 0);
    }

    // Now check HORIZ_SIZE and VERT_SIZE, which must be 8192 or less.
    RCHECK(ReadBits(&reader, 32) <= 8192);
    RCHECK(ReadBits(&reader, 32) <= 8192);
    return true;
  }

  // Buffer isn't Bitstream Metadata, so scan for start codes.
  int offset = 0;
  int sequence_start_code = 0;
  int frame_start_code = 0;
  while (true) {
    // Advance to start_code, if there is one.
    if (!AdvanceToStartCode(buffer, buffer_size, &offset, 5, 24, 1)) {
      // Not a complete sequence in memory, so return true if we've seen a
      // sequence start and a frame start (not checking entry points since
      // they only occur in advanced profiles).
      return (sequence_start_code > 0 && frame_start_code > 0);
    }

    // Now verify the block. AdvanceToStartCode() made sure that there are
    // at least 5 bytes remaining in the buffer.
    BitReader reader(buffer + offset, 5);
    RCHECK(ReadBits(&reader, 24) == 1);

    // Keep track of the number of certain types received.
    switch (ReadBits(&reader, 8)) {
      case VC1_SEQUENCE_START_CODE: {
        ++sequence_start_code;
        switch (ReadBits(&reader, 2)) {
          case 0:  // simple
          case 1:  // main
            RCHECK(ReadBits(&reader, 2) == 0);
            break;
          case 2:  // complex
            return false;
          case 3:  // advanced
            RCHECK(ReadBits(&reader, 3) <= 4);  // Verify level = 0..4
            RCHECK(ReadBits(&reader, 2) == 1);  // Verify colordiff_format = 1
            break;
        }
        break;
      }

      case VC1_ENTRY_POINT_START_CODE:
        // No fields in entry data to check. However, it must occur after
        // sequence header.
        RCHECK(sequence_start_code > 0);
        break;

      case VC1_FRAME_START_CODE:
        ++frame_start_code;
        break;
    }
    offset += 5;
  }
}

// For some formats the signature is a bunch of characters. They are defined
// below. Note that the first 4 characters of the string may be used as a TAG
// in LookupContainerByFirst4. For signatures that contain embedded \0, use
// uint8_t[].
static const char kAmrSignature[] = "#!AMR";
static const uint8_t kAsfSignature[] = {0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66,
                                        0xcf, 0x11, 0xa6, 0xd9, 0x00, 0xaa,
                                        0x00, 0x62, 0xce, 0x6c};
static const char kAssSignature[] = "[Script Info]";
static const char kAssBomSignature[] = UTF8_BYTE_ORDER_MARK "[Script Info]";
static const uint8_t kWtvSignature[] = {0xb7, 0xd8, 0x00, 0x20, 0x37, 0x49,
                                        0xda, 0x11, 0xa6, 0x4e, 0x00, 0x07,
                                        0xe9, 0x5e, 0xad, 0x8d};

// Attempt to determine the container type from the buffer provided. This is
// a simple pass, that uses the first 4 bytes of the buffer as an index to get
// a rough idea of the container format.
static MediaContainerName LookupContainerByFirst4(const uint8_t* buffer,
                                                  int buffer_size) {
  // Minimum size that the code expects to exist without checking size.
  if (buffer_size < kMinimumContainerSize)
    return MediaContainerName::kContainerUnknown;

  uint32_t first4 = Read32(buffer);
  switch (first4) {
    case 0x1a45dfa3:
      if (CheckWebm(buffer, buffer_size))
        return MediaContainerName::kContainerWEBM;
      break;

    case 0x3026b275:
      if (StartsWith(buffer,
                     buffer_size,
                     kAsfSignature,
                     sizeof(kAsfSignature))) {
        return MediaContainerName::kContainerASF;
      }
      break;

    case TAG('#','!','A','M'):
      if (StartsWith(buffer, buffer_size, kAmrSignature))
        return MediaContainerName::kContainerAMR;
      break;

    case TAG('#','E','X','T'):
      if (CheckHls(buffer, buffer_size))
        return MediaContainerName::kContainerHLS;
      break;

    case TAG('.','R','M','F'):
      if (buffer[4] == 0 && buffer[5] == 0)
        return MediaContainerName::kContainerRM;
      break;

    case TAG('.','r','a','\xfd'):
      return MediaContainerName::kContainerRM;

    case TAG('B','I','K','b'):
    case TAG('B','I','K','d'):
    case TAG('B','I','K','f'):
    case TAG('B','I','K','g'):
    case TAG('B','I','K','h'):
    case TAG('B','I','K','i'):
      if (CheckBink(buffer, buffer_size))
        return MediaContainerName::kContainerBink;
      break;

    case TAG('c','a','f','f'):
      if (CheckCaf(buffer, buffer_size))
        return MediaContainerName::kContainerCAF;
      break;

    case TAG('D','E','X','A'):
      if (buffer_size > 15 &&
          Read16(buffer + 11) <= 2048 &&
          Read16(buffer + 13) <= 2048) {
        return MediaContainerName::kContainerDXA;
      }
      break;

    case TAG('D','T','S','H'):
      if (Read32(buffer + 4) == TAG('D','H','D','R'))
        return MediaContainerName::kContainerDTSHD;
      break;

    case 0x64a30100:
    case 0x64a30200:
    case 0x64a30300:
    case 0x64a30400:
    case 0x0001a364:
    case 0x0002a364:
    case 0x0003a364:
      if (Read32(buffer + 4) != 0 && Read32(buffer + 8) != 0)
        return MediaContainerName::kContainerIRCAM;
      break;

    case TAG('f','L','a','C'):
      return MediaContainerName::kContainerFLAC;

    case TAG('F','L','V',0):
    case TAG('F','L','V',1):
    case TAG('F','L','V',2):
    case TAG('F','L','V',3):
    case TAG('F','L','V',4):
      if (buffer[5] == 0 && Read32(buffer + 5) > 8)
        return MediaContainerName::kContainerFLV;
      break;

    case TAG('F','O','R','M'):
      switch (Read32(buffer + 8)) {
        case TAG('A','I','F','F'):
        case TAG('A','I','F','C'):
          return MediaContainerName::kContainerAIFF;
      }
      break;

    case TAG('M','A','C',' '):
      return MediaContainerName::kContainerAPE;

    case TAG('O','N','2',' '):
      if (Read32(buffer + 8) == TAG('O','N','2','f'))
        return MediaContainerName::kContainerAVI;
      break;

    case TAG('O','g','g','S'):
      if (buffer[5] <= 7)
        return MediaContainerName::kContainerOgg;
      break;

    case TAG('R','F','6','4'):
      if (buffer_size > 16 && Read32(buffer + 12) == TAG('d','s','6','4'))
        return MediaContainerName::kContainerWAV;
      break;

    case TAG('R','I','F','F'):
      switch (Read32(buffer + 8)) {
        case TAG('A','V','I',' '):
        case TAG('A','V','I','X'):
        case TAG('A','V','I','\x19'):
        case TAG('A','M','V',' '):
          return MediaContainerName::kContainerAVI;
        case TAG('W','A','V','E'):
          return MediaContainerName::kContainerWAV;
      }
      break;

    case TAG('[','S','c','r'):
      if (StartsWith(buffer, buffer_size, kAssSignature))
        return MediaContainerName::kContainerASS;
      break;

    case TAG('\xef','\xbb','\xbf','['):
      if (StartsWith(buffer, buffer_size, kAssBomSignature))
        return MediaContainerName::kContainerASS;
      break;

    case 0x7ffe8001:
    case 0xfe7f0180:
    case 0x1fffe800:
    case 0xff1f00e8:
      if (CheckDts(buffer, buffer_size))
        return MediaContainerName::kContainerDTS;
      break;

    case 0xb7d80020:
      if (StartsWith(buffer,
                     buffer_size,
                     kWtvSignature,
                     sizeof(kWtvSignature))) {
        return MediaContainerName::kContainerWTV;
      }
      break;
  }

  // Now try a few different ones that look at something other
  // than the first 4 bytes.
  uint32_t first3 = first4 & 0xffffff00;
  switch (first3) {
    case TAG('C','W','S',0):
    case TAG('F','W','S',0):
      return MediaContainerName::kContainerSWF;

    case TAG('I','D','3',0):
      return MediaContainerName::kContainerMP3;
  }

  // Maybe the first 2 characters are something we can use.
  uint32_t first2 = Read16(buffer);
  switch (first2) {
    case kAc3SyncWord:
      if (CheckAc3(buffer, buffer_size))
        return MediaContainerName::kContainerAC3;
      if (CheckEac3(buffer, buffer_size))
        return MediaContainerName::kContainerEAC3;
      break;

    case 0xfff0:
    case 0xfff1:
    case 0xfff8:
    case 0xfff9:
      if (CheckAac(buffer, buffer_size))
        return MediaContainerName::kContainerAAC;
      break;
  }

  // Check if the file is in MP3 format without the ID3 header.
  if (CheckMp3(buffer, buffer_size))
    return MediaContainerName::kContainerMP3;

  return MediaContainerName::kContainerUnknown;
}

// Attempt to determine the container name from the buffer provided.
MediaContainerName DetermineContainer(const uint8_t* buffer, int buffer_size) {
  DCHECK(buffer);

  // Since MOV/QuickTime/MPEG4 streams are common, check for them first.
  if (CheckMov(buffer, buffer_size))
    return MediaContainerName::kContainerMOV;

  // Next attempt the simple checks, that typically look at just the
  // first few bytes of the file.
  MediaContainerName result = LookupContainerByFirst4(buffer, buffer_size);
  if (result != MediaContainerName::kContainerUnknown) {
    return result;
  }

  // Additional checks that may scan a portion of the buffer.
  if (CheckMpeg2ProgramStream(buffer, buffer_size))
    return MediaContainerName::kContainerMPEG2PS;
  if (CheckMpeg2TransportStream(buffer, buffer_size))
    return MediaContainerName::kContainerMPEG2TS;
  if (CheckMJpeg(buffer, buffer_size))
    return MediaContainerName::kContainerMJPEG;
  if (CheckDV(buffer, buffer_size))
    return MediaContainerName::kContainerDV;
  if (CheckH261(buffer, buffer_size))
    return MediaContainerName::kContainerH261;
  if (CheckH263(buffer, buffer_size))
    return MediaContainerName::kContainerH263;
  if (CheckH264(buffer, buffer_size))
    return MediaContainerName::kContainerH264;
  if (CheckMpeg4BitStream(buffer, buffer_size))
    return MediaContainerName::kContainerMPEG4BS;
  if (CheckVC1(buffer, buffer_size))
    return MediaContainerName::kContainerVC1;
  if (CheckSrt(buffer, buffer_size))
    return MediaContainerName::kContainerSRT;
  if (CheckGsm(buffer, buffer_size))
    return MediaContainerName::kContainerGSM;

  // AC3/EAC3 might not start at the beginning of the stream,
  // so scan for a start code.
  int offset = 1;  // No need to start at byte 0 due to First4 check.
  if (AdvanceToStartCode(buffer, buffer_size, &offset, 4, 16, kAc3SyncWord)) {
    if (CheckAc3(buffer + offset, buffer_size - offset))
      return MediaContainerName::kContainerAC3;
    if (CheckEac3(buffer + offset, buffer_size - offset))
      return MediaContainerName::kContainerEAC3;
  }

  return MediaContainerName::kContainerUnknown;
}

}  // namespace container_names

}  // namespace media
