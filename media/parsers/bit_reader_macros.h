// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_BIT_READER_MACROS_H_
#define MEDIA_PARSERS_BIT_READER_MACROS_H_

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

// Exp-Golomb code parsing as specified in H.26x specifications.
// Read one unsigned exp-Golomb code from the stream and return in |*out|
// with total bits read return in |*bits_read|.
#define READ_UE_WITH_BITS_READ_OR_RETURN(out, bits_read)                    \
  do {                                                                      \
    int _bit = 0;                                                           \
    int _num_bits_processed = -1;                                           \
    do {                                                                    \
      READ_BITS_OR_RETURN(1, &_bit);                                        \
      _num_bits_processed++;                                                \
    } while (_bit == 0);                                                    \
    if (_num_bits_processed > 31) {                                         \
      return kInvalidStream;                                                \
    }                                                                       \
    *out = (1u << _num_bits_processed) - 1u;                                \
    *bits_read = 1 + _num_bits_processed * 2;                               \
    int _rest;                                                              \
    if (_num_bits_processed == 31) {                                        \
      READ_BITS_OR_RETURN(_num_bits_processed, &_rest);                     \
      if (_rest == 0) {                                                     \
        break;                                                              \
      } else {                                                              \
        DVLOG(1)                                                            \
            << "Error in stream: invalid value while trying to read " #out; \
        return kInvalidStream;                                              \
      }                                                                     \
    }                                                                       \
    if (_num_bits_processed > 0) {                                          \
      READ_BITS_OR_RETURN(_num_bits_processed, &_rest);                     \
      *out += _rest;                                                        \
    }                                                                       \
  } while (0)

#define READ_UE_OR_RETURN(out)                          \
  do {                                                  \
    int _bits_read = -1;                                \
    READ_UE_WITH_BITS_READ_OR_RETURN(out, &_bits_read); \
  } while (0)

#define READ_UE_AND_MINUS_BITS_READ_OR_RETURN(out, num_bits_remain) \
  do {                                                              \
    int num_bits_read = -1;                                         \
    READ_UE_WITH_BITS_READ_OR_RETURN(out, &num_bits_read);          \
    *num_bits_remain -= num_bits_read;                              \
  } while (0)

// Read one signed exp-Golomb code from the stream and return in |*out|.
#define READ_SE_OR_RETURN(out)                          \
  do {                                                  \
    int _bits_read = -1;                                \
    int ue = 0;                                         \
    READ_UE_WITH_BITS_READ_OR_RETURN(&ue, &_bits_read); \
    if (ue % 2 == 0) {                                  \
      *out = -(ue / 2);                                 \
    } else {                                            \
      *out = ue / 2 + 1;                                \
    }                                                   \
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

#define GT_OR_RETURN(val1, val2)                                       \
  do {                                                                 \
    if ((val1) <= (val2)) {                                            \
      DVLOG(1) << "Error in stream, " #val1 " is smaller than " #val2; \
      return kInvalidStream;                                           \
    }                                                                  \
  } while (0)

#define LE_OR_RETURN(val1, val2)                                      \
  do {                                                                \
    if ((val1) > (val2)) {                                            \
      DVLOG(1) << "Error in stream, " #val1 " is larger than " #val2; \
      return kInvalidStream;                                          \
    }                                                                 \
  } while (0)

#define GE_OR_RETURN(val1, val2)                                      \
  do {                                                                \
    if ((val1) < (val2)) {                                            \
      DVLOG(1) << "Error in stream, " #val1 " is larger than " #val2; \
      return kInvalidStream;                                          \
    }                                                                 \
  } while (0)

#define BYTE_ALIGNMENT()                            \
  do {                                              \
    int bits_left_to_align = br_.NumBitsLeft() % 8; \
    if (bits_left_to_align) {                       \
      SKIP_BITS_OR_RETURN(bits_left_to_align);      \
    }                                               \
  } while (0)

#endif  // MEDIA_PARSERS_BIT_READER_MACROS_H_
