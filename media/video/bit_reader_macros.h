// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_BIT_READER_MACROS_H_
#define MEDIA_VIDEO_BIT_READER_MACROS_H_

// Warning! Should only be included in .cc files.
// Common bit reader macros shared by H.26x parsers.
#define READ_BITS_OR_RETURN(num_bits, out)                                 \
  do {                                                                     \
    int _out;                                                              \
    if (!br_.ReadBits(num_bits, &_out)) {                                  \
      DVLOG(1)                                                             \
          << "Error in stream: unexpected EOS while trying to read " #out; \
      return kInvalidStream;                                               \
    }                                                                      \
    *out = _out;                                                           \
  } while (0)

#define READ_BITS_AND_MINUS_BITS_READ_OR_RETURN(num_bits, out,             \
                                                num_bits_remain)           \
  do {                                                                     \
    int _out;                                                              \
    if (!br_.ReadBits(num_bits, &_out)) {                                  \
      DVLOG(1)                                                             \
          << "Error in stream: unexpected EOS while trying to read " #out; \
      return kInvalidStream;                                               \
    }                                                                      \
    *num_bits_remain -= num_bits;                                          \
    *out = _out;                                                           \
  } while (0)

#define SKIP_BITS_OR_RETURN(num_bits)                                       \
  do {                                                                      \
    int bits_left = num_bits;                                               \
    int discard;                                                            \
    while (bits_left > 0) {                                                 \
      if (!br_.ReadBits(bits_left > 16 ? 16 : bits_left, &discard)) {       \
        DVLOG(1) << "Error in stream: unexpected EOS while trying to skip"; \
        return kInvalidStream;                                              \
      }                                                                     \
      bits_left -= 16;                                                      \
    }                                                                       \
  } while (0)

#define READ_BOOL_OR_RETURN(out)                                           \
  do {                                                                     \
    int _out;                                                              \
    if (!br_.ReadBits(1, &_out)) {                                         \
      DVLOG(1)                                                             \
          << "Error in stream: unexpected EOS while trying to read " #out; \
      return kInvalidStream;                                               \
    }                                                                      \
    *out = _out != 0;                                                      \
  } while (0)

#define READ_BOOL_AND_MINUS_BITS_READ_OR_RETURN(out, num_bits_remain)      \
  do {                                                                     \
    int _out;                                                              \
    if (!br_.ReadBits(1, &_out)) {                                         \
      DVLOG(1)                                                             \
          << "Error in stream: unexpected EOS while trying to read " #out; \
      return kInvalidStream;                                               \
    }                                                                      \
    *num_bits_remain -= 1;                                                 \
    *out = _out != 0;                                                      \
  } while (0)

#define READ_UE_OR_RETURN(out)                                                 \
  do {                                                                         \
    if (ReadUE(out, nullptr) != kOk) {                                         \
      DVLOG(1) << "Error in stream: invalid value while trying to read " #out; \
      return kInvalidStream;                                                   \
    }                                                                          \
  } while (0)

#define READ_UE_AND_MINUS_BITS_READ_OR_RETURN(out, num_bits_remain)            \
  do {                                                                         \
    int num_bits_read;                                                         \
    if (ReadUE(out, &num_bits_read) != kOk) {                                  \
      DVLOG(1) << "Error in stream: invalid value while trying to read " #out; \
      return kInvalidStream;                                                   \
    }                                                                          \
    *num_bits_remain -= num_bits_read;                                         \
  } while (0)

#define READ_SE_OR_RETURN(out)                                                 \
  do {                                                                         \
    if (ReadSE(out, nullptr) != kOk) {                                         \
      DVLOG(1) << "Error in stream: invalid value while trying to read " #out; \
      return kInvalidStream;                                                   \
    }                                                                          \
  } while (0)

#define IN_RANGE_OR_RETURN(val, min, max)                                   \
  do {                                                                      \
    if ((val) < (min) || (val) > (max)) {                                   \
      DVLOG(1) << "Error in stream: invalid value, expected " #val " to be" \
               << " in range [" << (min) << ":" << (max) << "]"             \
               << " found " << (val) << " instead";                         \
      return kInvalidStream;                                                \
    }                                                                       \
  } while (0)

#define TRUE_OR_RETURN(a)                                            \
  do {                                                               \
    if (!(a)) {                                                      \
      DVLOG(1) << "Error in stream: invalid value, expected " << #a; \
      return kInvalidStream;                                         \
    }                                                                \
  } while (0)

#define EQ_OR_RETURN(shdr1, shdr2, field)                                 \
  do {                                                                    \
    if ((shdr1->field) != (shdr2->field)) {                               \
      DVLOG(1) << "Error in stream, slice header fields must match for: " \
               << #field;                                                 \
      return kInvalidStream;                                              \
    }                                                                     \
  } while (0)

#endif  // MEDIA_VIDEO_BIT_READER_MACROS_H_
