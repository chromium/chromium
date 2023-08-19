// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_MT21_MT21_UTIL_H_
#define MEDIA_GPU_V4L2_MT21_MT21_UTIL_H_

// High performance implementation of the MT21 decompression algorithm. It is
// recommended reading the unoptimized implementation here before diving into
// this
// https://source.chromium.org/chromiumos/_/chromium/chromiumos/platform/drm-tests/+/94106a2845911104895c50aa5d70c6e5fc8972fc:pixel_formats/mt21_converter.c;drc=091692f34d333dec8fd3a8e375a4ad5a65682cb2;bpv=0;bpt=0
//
// This algorithm should achieve a throughput of about 156 megapixels per second
// on a single Cortex A72.

// This file contains a lot of SIMD built-ins. Thankfully we're only ever going
// to need this on certain SoCs, so we just wrap everything in a giant include
// guard.
//
// TODO(b/286891480): Convert these Neon intrinsics into Highway, which is more
// portable. We only used Neon because Highway's OrderedTruncate2To(), which we
// need for implementing NarrowToU8, was not released at the time of writing.

#include "build/build_config.h"

#if !defined(ARCH_CPU_ARM_FAMILY)
#error "MT21Decompressor is only intended to run on MT8173 (ARM)"
#endif

#if !(defined(COMPILER_GCC) || defined(__clang__))
#error "MT21Decompressor is only intended to be built with GCC or Clang"
#endif

#include <arm_neon.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

namespace media {

// This file is a little unusual in that it's a header that actually contains a
// bunch of function definitions. This is pretty ugly, but there's a good reason
// for it. When we're writing tight loops, we want the compiler to aggressively
// inline so we don't pay the performance penalty of managing the callstack. If
// we put the function definitions in a different translation unit, we won't get
// any inlining because the linker isn't smart enough for that.
//
// Just in case the compiler doesn't take the hint, we sprinkle some
// "always_inline" attributes in hot functions.
//
// The other alternative would be to just add all of these functions in one .cc
// file. We chose not to do this because then we wouldn't be able to write
// granular unit tests; we would just have to settle for one giant "decompress
// frame" integration test.
//
// This technique alone cuts our latency by ~40%.
namespace {

constexpr size_t kNumOutputLanes = 16;
constexpr size_t kMT21TileWidth = 16;
constexpr size_t kMT21TileHeight = 32;
constexpr size_t kMT21TileSize = kMT21TileWidth * kMT21TileHeight;
constexpr size_t kMT21BlockWidth = kMT21TileWidth;
constexpr size_t kMT21BlockHeight = kMT21TileHeight / 4;
constexpr size_t kMT21BlockSize = kMT21BlockWidth * kMT21BlockHeight;
constexpr size_t kMT21SubblocksInBlock = 2;
constexpr size_t kMT21SubblockWidth = kMT21BlockWidth;
constexpr size_t kMT21SubblockHeight = kMT21BlockHeight / kMT21SubblocksInBlock;
constexpr size_t kMT21SubblockSize = kMT21SubblockWidth * kMT21SubblockHeight;

// Loops can cause branch mispredictions, so we manually unroll them when
// practical.
//
// Of course, compilers have loop unrolling optimizations built into them, but
// these don't actually seem to trigger often in practice, even with -O2 and
// -funroll-loops. I suspect the compiler is actually shy about loop unrolling
// because it's worried about blowing up the I-cache. We have a good idea of
// where the program is going to spend most of its time, however, so we can
// manually unroll hotspots.
#define LOOPN(inner_block, N)                                             \
  {                                                                       \
    _Pragma("clang loop unroll(full)") for (size_t i = 0; i < (N); i++) { \
      inner_block                                                         \
    }                                                                     \
  }

// We take two completely separate approaches to optimizing MT21 decompression:
// a scalar approach, and a vector approach. The vector approach is
// substantially faster than the scalar approach, but the vector approach
// requires us to have at least 16 compressed subblocks to process
// simultaneously. Any remainder subblocks need to go through the scalar
// algorithm.

//////////////////////
// Scalar Algorithm //
//////////////////////

// Efficient scalar class for reading MT21 bitstreams. We buffer the bitstream
// into a 64 bit accumulator and load into it 4 bytes at a time. Note that this
// means we cannot read more than 32 bits at a time.
class MT21BitstreamReader {
 public:
  MT21BitstreamReader(const uint8_t* buf);
  // Look ahead N bits, but do not discard. Note that N cannot be 0.
  int PeekNBits(int n);
  // Discard N bits and possibly load more into the accumulator.
  void DiscardNBits(int n);
  // Combined peek and discard.
  int ReadNBits(int n);
  // Returns total number of consumed bits. Useful for filling in Golomb-Rice
  // lookup tables.
  size_t GetConsumedBits();

 private:
  const uint8_t* buf_;
  size_t consumed_bits_;

  uint64_t accumulator_;
  size_t byte_idx_;
  uint8_t outstanding_reads_;

  // Responsible for keeping the accumulator full.
  void MaybeRefillAccumulator();
};

MT21BitstreamReader::MT21BitstreamReader(const uint8_t* buf) {
  buf_ = buf;
  accumulator_ = *(uint64_t*)(buf + 8);
  byte_idx_ = 4;
  outstanding_reads_ = 0;
  consumed_bits_ = 0;
}

void MT21BitstreamReader::MaybeRefillAccumulator() {
  if (outstanding_reads_ >= 32) {
    uint32_t next_dword = *(uint32_t*)(buf_ + byte_idx_);
    outstanding_reads_ -= 32;
    accumulator_ |= ((uint64_t)next_dword) << outstanding_reads_;

    // Advance to the next row if we've exhausted the current one.
    // I experimented with eliminating this branch, but it doesn't seem to make
    // much of a difference for efficiency.
    if ((byte_idx_ & 0xF) == 0) {
      byte_idx_ += 32;
    }
    // Rows are read right to left.
    byte_idx_ -= 4;
  }
}

int MT21BitstreamReader::PeekNBits(int n) {
  // N cannot be 0, because shifting right by 64 bits is technically undefined
  // behavior. One some platforms, this will return unexpected results.
  return (int)(accumulator_ >> (64 - n));
}

void MT21BitstreamReader::DiscardNBits(int n) {
  accumulator_ <<= n;
  outstanding_reads_ += n;
  consumed_bits_ += n;
  MaybeRefillAccumulator();
}

int MT21BitstreamReader::ReadNBits(int n) {
  if (!n) {
    return 0;
  }

  int ret = PeekNBits(n);
  DiscardNBits(n);

  return ret;
}

size_t MT21BitstreamReader::GetConsumedBits() {
  return consumed_bits_;
}

// "Slow" method of reading a Golomb-Rice symbol. Needed for miscellaneous
// functions like populating the lookup table and fallback logic if the symbol
// isn't in the table.
int ReadGolombRiceSymbol(MT21BitstreamReader& reader, int k) {
  const int escape_sequence_num = 7 + k;
  int num_ones = 0;
  int ret = 0;

  // Read the unary component.
  while (1) {
    const int curr_bit = reader.ReadNBits(1);
    if (curr_bit) {
      num_ones++;

      if (num_ones == escape_sequence_num) {
        break;
      }
    } else {
      break;
    }
  }

  if (num_ones == escape_sequence_num) {
    // We've hit the escape sequence, so we switch to limited length mode.
    ret = reader.ReadNBits(8 - (k >= 4));

    ret += num_ones * (1 << k);
  } else if (num_ones) {
    ret = (num_ones * (1 << k)) + reader.ReadNBits(k);
  } else {
    // Special case unary components of 0, because 0 symbols don't have a sign
    // bit.
    ret = reader.ReadNBits(k - 1);
    if (ret) {
      ret <<= 1;
      ret += reader.ReadNBits(1);
    }
  }

  // Map unsigned symbol to signed symbol.
  if (ret & 1) {
    return -1 * (ret >> 1);
  } else {
    return ret >> 1;
  }
}

}  // namespace

// "Fast" method of reading Golomb-Rice symbols that uses a lookahead window and
// a lookup table. This will fall back to the slow method if the symbol exceeds
// kGolombRiceTableLookaheadLen, because we need to keep the size of the lookup
// table small enough to fit in L1.
//
// Note that we need to break this definition out of the anonymous namespace
// because we want to forward declare it in mt21_decompressor.h.
struct GolombRiceTableEntry {
  // Size of the compressed symbol.
  int8_t in_size;
  // Value of the symbol.
  int8_t symbol;
};

namespace {

constexpr size_t kMaxKValue = 8;
// Lookahead len chosen experimentally. We want it to be big enough that we
// maximize how often we hit the lookup table, but small enough to fit in the
// A72's L1 cache. Some testing indicated that 10 bits was the magic number.
constexpr size_t kGolombRiceTableLookaheadLen = 10;
constexpr size_t kGolombRiceTableSize = (1 << kGolombRiceTableLookaheadLen);
constexpr size_t kGolombRiceCacheSize = kGolombRiceTableSize * (kMaxKValue - 1);
constexpr size_t kBitsInByte = 8;

// Initializes the lookup tables for all possible k values
void PopulateGolombRiceCache(GolombRiceTableEntry* cache) {
  uint8_t tmp_buf[kMT21SubblockSize];
  for (size_t k = 1; k < kMaxKValue; k++) {
    GolombRiceTableEntry* table = cache + (k - 1) * kGolombRiceTableSize;
    for (size_t lookahead_val = 0;
         lookahead_val < (1 << kGolombRiceTableLookaheadLen); lookahead_val++) {
      GolombRiceTableEntry* entry = table + lookahead_val;

      // Compressed symbol size 0 indicates a cache miss.
      entry->in_size = 0;

      // Create a fake Subblock that just contains our target value in the first
      // 2 bytes.
      tmp_buf[kMT21SubblockWidth - 1] =
          lookahead_val >> (kGolombRiceTableLookaheadLen - kBitsInByte);
      tmp_buf[kMT21SubblockWidth - 2] =
          (lookahead_val << (kBitsInByte -
                             (kGolombRiceTableLookaheadLen - kBitsInByte))) &
          0xFF;

      MT21BitstreamReader reader(tmp_buf);

      // Try to read a symbol. If it was small enough, put it in the lookup
      // table.
      int symbol = ReadGolombRiceSymbol(reader, k);
      if (reader.GetConsumedBits() <= kGolombRiceTableLookaheadLen) {
        entry->in_size = reader.GetConsumedBits();
        entry->symbol = symbol;
      }
    }
  }
}

int FastReadGolombRiceSymbol(MT21BitstreamReader& reader,
                             int k,
                             const GolombRiceTableEntry* table) {
  const int lookahead_window = reader.PeekNBits(kGolombRiceTableLookaheadLen);
  if (table[lookahead_window].in_size) {
    reader.DiscardNBits(table[lookahead_window].in_size);
    return table[lookahead_window].symbol;
  } else {
    // Cache miss, fall back to slow method.
    return ReadGolombRiceSymbol(reader, k);
  }
}

// Prediction functions for all 4 subblock regions. We split this logic up into
// separate functions rather than using "if" statements because the "if"
// statements were causing high rates of branch misprediction.
uint8_t FirstRowPrediction(uint8_t right) {
  return right;
}

uint8_t LastColPrediction(uint8_t up) {
  return up;
}

// We use a lookup table approach because, again, we want to avoid if statements
// to avoid branch mispredictions. It's cheaper to just compute all 3 possible
// prediction values and select which one we want to use later.
uint8_t FirstColPrediction(uint8_t up, uint8_t up_right, uint8_t right) {
  int max_up_right = up > right ? up : right;
  int min_up_right = up > right ? right : up;
  int horiz_grad_prediction = right + (up - up_right);
  uint8_t ret[3];
  int idx = ((up_right > max_up_right) << 1) | (up_right < min_up_right);
  ret[0b00] = horiz_grad_prediction;
  ret[0b01] = min_up_right;
  ret[0b10] = max_up_right;
  return ret[idx];
}

// Same deal as with first column prediction, but with 4 possible prediction
// values.
uint8_t BodyPrediction(uint8_t up_left,
                       uint8_t up,
                       uint8_t up_right,
                       uint8_t right) {
  int max_up_right = up > right ? up : right;
  int min_up_right = up > right ? right : up;
  int right_grad = right + (up - up_right);
  int left_grad = right + (up - up_left);
  int use_right_grad = up_right <= max_up_right && up_right >= min_up_right;
  int idx = (use_right_grad << 1 | use_right_grad) |
            (up_left > max_up_right) << 1 | up_left < min_up_right;
  uint8_t ret[4];
  ret[0b00] = left_grad;
  ret[0b01] = min_up_right;
  ret[0b10] = max_up_right;
  ret[0b11] = right_grad;
  return ret[idx];
}

// Core (scalar) decompression functions.
//
// We're abusing templates rather than taking subblock dims as a parameter
// because we want to try to coax the compiler into evaluating as many
// expressions as possible during compile time and maybe even unrolling the
// loops.
struct MT21Subblock {
  const uint8_t* src;
  RAW_PTR_EXCLUSION uint8_t* dest;
  size_t len;
};
struct MT21YSubblock : MT21Subblock {};
struct MT21UVSubblock : MT21Subblock {};

template <int width>
void DecompressSubblock(MT21BitstreamReader& reader,
                        uint8_t* dest,
                        const GolombRiceTableEntry* symbol_cache) {
  int k = reader.ReadNBits(3) + 1;
  dest[width - 1] = reader.ReadNBits(8);

  if (k == 8) {
    // This is a solid color block, set everything equal to the top right corner
    // value.
    memset(dest, dest[width - 1], width * kMT21SubblockHeight);
    return;
  }

  // Find which table in the cache we should be using. Sometimes the compiler
  // doesn't bother factoring this calculation out of the loop, so we do it
  // manually.
  const GolombRiceTableEntry* symbol_table =
      symbol_cache + (k - 1) * kGolombRiceTableSize;

  // Pixels get processed right to left, top to bottom.
  uint8_t curr;
  uint8_t up_left;
  uint8_t up = dest[width - 1];
  uint8_t up_right;
  uint8_t right = up;
  for (int x = width - 2; x >= 0; x--) {
    curr = FirstRowPrediction(right) +
           FastReadGolombRiceSymbol(reader, k, symbol_table);
    right = curr;
    dest[x] = curr;
  }
  for (size_t y = 1; y < kMT21SubblockHeight; y++) {
    up = dest[y * width - 1];
    curr = LastColPrediction(up) +
           FastReadGolombRiceSymbol(reader, k, symbol_table);
    dest[y * width + width - 1] = curr;
    right = curr;
    up_right = up;
    up = dest[y * width - 2];
    for (size_t x = width - 2; x >= 1; x--) {
      up_left = dest[y * width - width + x - 1];
      curr = BodyPrediction(up_left, up, up_right, right) +
             FastReadGolombRiceSymbol(reader, k, symbol_table);
      dest[y * width + x] = curr;
      right = curr;
      up_right = up;
      up = up_left;
    }
    dest[y * width] = FirstColPrediction(up, up_right, right) +
                      FastReadGolombRiceSymbol(reader, k, symbol_table);
  }
}

// UV subblocks are half the size of normal subblocks and are written one after
// another with no padding. We use the DecompressSubblockHelper template to help
// us differentiate this behavior.
template <typename T>
void DecompressSubblockHelper(T subblock,
                              const GolombRiceTableEntry* symbol_cache);
template <>
void DecompressSubblockHelper(MT21YSubblock subblock,
                              const GolombRiceTableEntry* symbol_cache) {
  MT21BitstreamReader reader(subblock.src);
  DecompressSubblock<kMT21SubblockWidth>(reader, subblock.dest, symbol_cache);
}
// Interleaves a U and V subblock into a combined UV subblock.
void InterleaveUVSubblock(const uint8_t* src_u,
                          const uint8_t* src_v,
                          uint8_t* dest_uv) {
  uint8x16_t tmp_u, tmp_v;
  uint8x16x2_t store_tmp;
  LOOPN(
      {
        tmp_u = vld1q_u8(src_u);
        src_u += 16;
        tmp_v = vld1q_u8(src_v);
        src_v += 16;
        store_tmp.val[0] = tmp_u;
        store_tmp.val[1] = tmp_v;
        vst2q_u8(dest_uv, store_tmp);
        dest_uv += 32;
      },
      2)
}
template <>
void DecompressSubblockHelper(MT21UVSubblock subblock,
                              const GolombRiceTableEntry* symbol_cache) {
  MT21BitstreamReader reader(subblock.src);
  uint8_t scratch_u[kMT21SubblockSize / 2] __attribute__((aligned(16)));
  uint8_t scratch_v[kMT21SubblockSize / 2] __attribute__((aligned(16)));
  DecompressSubblock<kMT21SubblockWidth / 2>(reader, scratch_v, symbol_cache);
  DecompressSubblock<kMT21SubblockWidth / 2>(reader, scratch_u, symbol_cache);
  InterleaveUVSubblock(scratch_u, scratch_v, subblock.dest);
}

/////////////////////////
// SIMD Implementation //
/////////////////////////

// This SIMD implementation operates on 16 subblocks at a time, even though we
// only have 4 lanes to work with since the accumulator needs to be 32-bit. This
// algorithm was originally developed to only operate on 4 subblocks at a time
// to match the number of lanes, but we had a lot of trouble keeping the
// Cortex A72's 2 Neon pipelines full. So, we manually unroll the loop a little
// and interleave operations from each iteration.
//
// Generally the compiler and the CPU itself are good enough at instructions
// scheduling, but in this case, manually scheduling our instructions
// dramatically increases our throughput. There may be more optimizations to do
// yet in this regard; our current throughput is about 1.3 IPC, when it should
// actually be closer to 1.5 IPC.

static const uint8x16_t byte_literal_1 = vdupq_n_u8(1);
static const uint32x4_t dword_literal_1 = vdupq_n_u32(1);
static const uint32x4_t dword_literal_4 = vdupq_n_u32(4);
static const uint32x4_t dword_literal_7 = vdupq_n_u32(7);
static const uint32x4_t dword_literal_8 = vdupq_n_u32(8);
static const uint32x4_t dword_literal_11 = vdupq_n_u32(11);
static const uint32x4_t dword_literal_31 = vdupq_n_u32(31);
static const uint32x4_t dword_literal_32 = vdupq_n_u32(32);

// Helpful utility for taking 4 vectors with uint32_t elements and combining
// them into 1 vector with uint8_t elements. Note that this necessarily discards
// the upper 24 bits of each element. Our accumulators need to be 32-bit because
// our longest Golomb-Rice code is 20 bits, but we can narrow for other parts of
// the algorithm.

__attribute__((always_inline)) uint8x16_t NarrowToU8(uint32x4_t& vec1,
                                                     uint32x4_t& vec2,
                                                     uint32x4_t& vec3,
                                                     uint32x4_t& vec4) {
  return vcombine_u8(vmovn_u16(vcombine_u16(vmovn_u32(vec1), vmovn_u32(vec2))),
                     vmovn_u16(vcombine_u16(vmovn_u32(vec3), vmovn_u32(vec4))));
}

// 32-bit ARM machines don't actually support unaligned memory access. The
// accumulator management code was originally written for Aarch64, which
// supports unaligned accesses without issue. In order to make that code work
// with Chrome, we need this hacky workaround. If performance is drastically
// different between the Aarch64 prototype and the production version of the
// code, this portion of the code is a good place to start poking. Supposedly
// there was a penalty for unaligned reads on Aarch64 as well, but I can't find
// any documentation for how many cycles that is on a Cortex A72 or A53.
__attribute__((always_inline)) uint32_t LoadUnalignedDword(uint32_t* ptr) {
  uint32_t ret;
  memcpy(&ret, ptr, sizeof(uint32_t));
  return ret;
}

// Helpful utility for managing the accumulator. This function effectively
// discards |discard_size| bits and loads in more bytes from the bitstream as
// needed.
__attribute__((always_inline)) void VectorManageAccumulator(
    uint32x4_t* accumulator,
    uint32x4_t* outstanding_reads,
    const uint32x4_t& discard_size,
    int i,
    uint8_t** compressed_ptr) {
  // We always load in a fresh dword. Often it will be from the same offsets.
  // This is inefficient, but it's offset by the speedup of vectorization.
  outstanding_reads[i] = vaddq_u32(outstanding_reads[i], discard_size);
  uint32x4_t offsets = vshrq_n_u32(outstanding_reads[i], 3);
  compressed_ptr[i * 4] -= offsets[0];
  compressed_ptr[i * 4 + 1] -= offsets[1];
  compressed_ptr[i * 4 + 2] -= offsets[2];
  compressed_ptr[i * 4 + 3] -= offsets[3];
  outstanding_reads[i] = vandq_u32(outstanding_reads[i], dword_literal_7);
  accumulator[i][0] = LoadUnalignedDword((uint32_t*)compressed_ptr[i * 4]);
  accumulator[i][1] = LoadUnalignedDword((uint32_t*)compressed_ptr[i * 4 + 1]);
  accumulator[i][2] = LoadUnalignedDword((uint32_t*)compressed_ptr[i * 4 + 2]);
  accumulator[i][3] = LoadUnalignedDword((uint32_t*)compressed_ptr[i * 4 + 3]);
  accumulator[i] =
      vshlq_u32(accumulator[i], vreinterpretq_s32_u32(outstanding_reads[i]));
}

// Golomb-Rice decompression. The core algorithm looks like this:
// 1. escape_seq = k + 7
// 2. unary_component = min(count_leading_zero(~accumulator), escape_seq)
// 3. unary_len = unary_component + (unary_component == escape_seq)
// 5. binary_len = (unary_component == escape_seq) ? 8 : k
// 6. binary_component = (accumulator << unary_len) >> (32 - binary_len)
// 7. symbol = (k == 8) ? 0 : (unary_component << k) + binary_component
// 8. symbol = symbol / 2 * (symbol % 2 ? -1 : 1)
__attribute__((always_inline)) uint8x16_t VectorReadGolombRiceSymbol(
    uint32x4_t* accumulator,
    uint32x4_t* outstanding_reads,
    uint32x4_t* escape_codes,
    uint32x4_t* escape_binary_len_diff,
    uint32x4_t* k_vals,
    uint32x4_t* dword_solid_color_mask,
    uint8_t** compressed_ptr) {
  // leading_ones = min(count_leading_zero(~accumulator), escape_codes)
  // escape_lanes = leading_ones == escape_codes
  uint32x4_t leading_ones[4];
  uint32x4_t escape_lanes[4];
  LOOPN(
      {
        leading_ones[i] =
            vminq_u32(vclzq_u32(vmvnq_u32(accumulator[i])), escape_codes[i]);
        escape_lanes[i] = vceqq_u32(leading_ones[i], escape_codes[i]);
      },
      4)

  // binary_len = k + (escape_lanes * (8 - k))
  // unary_len = leading_ones + !escape_lanes
  uint32x4_t binary_len[4];
  uint32x4_t unary_len[4];
  LOOPN(
      {
        binary_len[i] = vaddq_u32(
            k_vals[i], vandq_u32(escape_lanes[i], escape_binary_len_diff[i]));
        unary_len[i] =
            vaddq_u32(leading_ones[i],
                      vandq_u32(dword_literal_1, vmvnq_u32(escape_lanes[i])));
      },
      4)

  // output = (leading_ones << k)
  // output += ((accumulator << unary_len) >> (32 - binary_len)
  uint32x4_t dword_output[4];
  LOOPN(
      {
        dword_output[i] =
            vshlq_u32(leading_ones[i], vreinterpretq_s32_u32(k_vals[i]));
        dword_output[i] = vaddq_u32(
            dword_output[i],
            vshlq_u32(
                vshlq_u32(accumulator[i], vreinterpretq_s32_u32(unary_len[i])),
                vsubq_s32(vreinterpretq_s32_u32(binary_len[i]),
                          dword_literal_32)));
      },
      4)

  // total_len = unary_len + binary_len - (output <= 1)
  // total_len = solid_color_mask ? total_len : 0
  uint32x4_t total_len[4];
  LOOPN(
      {
        total_len[i] =
            vsubq_u32(vaddq_u32(unary_len[i], binary_len[i]),
                      vandq_u32(dword_literal_1,
                                vcleq_u32(dword_output[i], dword_literal_1)));
        total_len[i] = vandq_u32(total_len[i], dword_solid_color_mask[i]);
      },
      4)

  // Handle accumulator.
  LOOPN(
      {
        VectorManageAccumulator(accumulator, outstanding_reads, total_len[i], i,
                                compressed_ptr);
      },
      4)

  // output = (output / 2) * (output % 2 ? -1 : 1)
  // This is a hack that relies on how two's complement arithmetic works.
  uint8x16_t output = NarrowToU8(dword_output[0], dword_output[1],
                                 dword_output[2], dword_output[3]);
  uint8x16_t negative_lanes = vandq_u8(output, byte_literal_1);
  output = vaddq_u8(
      veorq_u8(vshrq_n_u8(output, 1), vtstq_u8(negative_lanes, negative_lanes)),
      negative_lanes);

  return output;
}

// Initializes the accumulator with the first 4 bytes of compressed data.
__attribute__((always_inline)) void VectorInitializeAccumulator(
    uint32x4_t* accumulator,
    uint8_t** compressed_ptr) {
  LOOPN(
      {
        accumulator[i][0] =
            LoadUnalignedDword((uint32_t*)compressed_ptr[i * 4]);
        accumulator[i][1] =
            LoadUnalignedDword((uint32_t*)compressed_ptr[i * 4 + 1]);
        accumulator[i][2] =
            LoadUnalignedDword((uint32_t*)compressed_ptr[i * 4 + 2]);
        accumulator[i][3] =
            LoadUnalignedDword((uint32_t*)compressed_ptr[i * 4 + 3]);
      },
      4);
}

// Reads the 11 bit header on the compressed data and initializes some important
// vectors.
__attribute__((always_inline)) uint8x16_t VectorReadCompressedHeader(
    uint32x4_t* accumulator,
    uint32x4_t* outstanding_reads,
    uint32x4_t* escape_codes,
    uint32x4_t* escape_binary_len_diff,
    uint32x4_t* k_vals,
    uint32x4_t* dword_solid_color_mask,
    uint8x16_t& solid_color_mask,
    uint8_t** compressed_ptr) {
  // Parse out our K value.
  // k = (accumulator >> 29) + 1
  // 29 comes from 32 - 3, since 32 is the size of the accumulator and 3 is the
  // size of k.
  LOOPN(
      {
        k_vals[i] =
            vaddq_u32(vshrq_n_u32(accumulator[i], 32 - 3), dword_literal_1);
      },
      4)

  // Calculate what our escape code should be for each lane based on the K
  // values.
  // escape_codes = k + 7
  LOOPN({ escape_codes[i] = vaddq_u32(k_vals[i], dword_literal_7); }, 4)

  // Compute the length of the binary components of "escaped" symbols.
  // Note that we abuse the fact that 0xFFFFFFFF == -1
  // escaped_binary_len_diff = 8 - k - (k >= 4)
  LOOPN(
      {
        escape_binary_len_diff[i] =
            vaddq_u32(vsubq_u32(dword_literal_8, k_vals[i]),
                      vcgeq_u32(k_vals[i], dword_literal_4));
      },
      4)

  // Figure out which lanes are actually operating in solid color mode. Yes, we
  // do a lot of wasted computation and then throw away the results for solid
  // color blocks. Unfortunately this is also the price we pay for
  // vectorization.
  // solid_color_mask = 0xFF * (k < 8)
  LOOPN({ dword_solid_color_mask[i] = vcltq_u32(k_vals[i], dword_literal_8); },
        4)
  solid_color_mask =
      NarrowToU8(dword_solid_color_mask[0], dword_solid_color_mask[1],
                 dword_solid_color_mask[2], dword_solid_color_mask[3]);

  // Parse the top right pixel value
  // accumulator <<= 3
  // top_right = accumulator >> 24
  // accumulator <<= 8
  uint32x4_t top_right[4];
  LOOPN(
      {
        accumulator[i] = vshlq_n_u32(accumulator[i], 3);
        top_right[i] = vshrq_n_u32(accumulator[i], 24);
        accumulator[i] = vshlq_n_u32(accumulator[i], 8);
      },
      4)

  // Manage the accumulator (shift in new bits if possible). For Y subblocks,
  // this isn't strictly necessary because the longest prefix code is 20 bits,
  // and we've only consumed 11 bits, so we should technically have enough to
  // read the first Golomb-Rice symbol. But U subblocks are appended directly to
  // the end of V subblocks with no padding, so it's possible that the subblock
  // we are currently decompressing does not start at a byte boundary, so we can
  // no longer make this assumption.
  LOOPN(
      {
        VectorManageAccumulator(accumulator, outstanding_reads,
                                dword_literal_11, i, compressed_ptr);
      },
      4)

  return NarrowToU8(top_right[0], top_right[1], top_right[2], top_right[3]);
}

// Straightforward vector implementations of the prediction methods. The only
// hangup is that we don't use a lookup table exactly. Neon actually has a
// lookup table instruction, and the first iteration of the code used that, but
// it turns out that using a series of ternary instructions (vbslq_u8) is
// slightly faster.

__attribute__((always_inline)) uint8x16_t VectorFirstRowPrediction(
    const uint8x16_t& right) {
  return right;
}

__attribute__((always_inline)) uint8x16_t VectorLastColPrediction(
    const uint8x16_t& up) {
  return up;
}

__attribute__((always_inline)) uint8x16_t VectorFirstColPrediction(
    const uint8x16_t& up,
    const uint8x16_t& up_right,
    const uint8x16_t& right) {
  const uint8x16_t min_pred = vminq_u8(up, right);
  const uint8x16_t max_pred = vmaxq_u8(up, right);
  const uint8x16_t right_grad = vreinterpretq_u8_s8(vaddq_s8(
      right, vsubq_s8(vreinterpretq_s8_u8(up), vreinterpretq_s8_u8(up_right))));
  const uint8x16_t up_right_above_max = vcgtq_u8(up_right, max_pred);
  const uint8x16_t up_right_below_min = vcltq_u8(up_right, min_pred);
  uint8x16_t pred = vbslq_u8(up_right_above_max, max_pred, min_pred);
  pred = vbslq_u8(vorrq_u8(up_right_above_max, up_right_below_min), pred,
                  right_grad);
  return pred;
}

__attribute__((always_inline)) uint8x16_t VectorBodyPrediction(
    const uint8x16_t& up_left,
    const uint8x16_t& up,
    const uint8x16_t& up_right,
    const uint8x16_t& right) {
  uint8x16_t min_pred = vminq_u8(up, right);
  uint8x16_t max_pred = vmaxq_u8(up, right);
  uint8x16_t right_grad = vreinterpretq_u8_s8(vaddq_s8(
      vreinterpretq_s8_u8(right),
      vsubq_s8(vreinterpretq_s8_u8(up), vreinterpretq_s8_u8(up_right))));
  uint8x16_t left_grad = vreinterpretq_u8_s8(vaddq_s8(
      vreinterpretq_s8_u8(right),
      vsubq_s8(vreinterpretq_s8_u8(up), vreinterpretq_s8_u8(up_left))));
  uint8x16_t up_left_above_max = vcgtq_u8(up_left, max_pred);
  uint8x16_t up_left_below_min = vcltq_u8(up_left, min_pred);
  uint8x16_t use_right_grad =
      vandq_u8(vcleq_u8(up_right, max_pred), vcgeq_u8(up_right, min_pred));
  uint8x16_t pred = vbslq_u8(up_left_above_max, max_pred, min_pred);
  pred =
      vbslq_u8(vorrq_u8(up_left_above_max, up_left_below_min), pred, left_grad);
  pred = vbslq_u8(use_right_grad, right_grad, pred);
  return pred;
}

// In order for our vectorized accumulator management to work, we have to
// flip our subblocks vertically. Our decompression routine always decrements
// the compressed data pointer to avoid having to deal with moving to the next
// row. This is a routine for taking all 16 target subblocks, flipping them, and
// copying them into scratch memory.
constexpr size_t kMT21ScratchMemorySize = 4096;
constexpr size_t kMT21RedZoneSize = 1024;

template <class T>
void SubblockGather(const std::vector<T>& subblock_list,
                    int start_idx,
                    uint8_t* aligned_scratch_memory,
                    uint8_t** compressed_ptr) {
  // Our scratch memory is 4096 bytes. We use the first and last 1KB as a "red
  // zone" to catch any overread from malformed bitstreams. This is much more
  // performant than bounds checking. Our longest Golomb-Rice code is 20-bits,
  // so really our red zone only needs to be
  // 20/8*kMT21SubblockHeight*kMT21SubblockWidth = 160 bytes. We can consider
  // relaxing the red zone size if memory becomes more of an issue.
  aligned_scratch_memory += kMT21RedZoneSize;

  for (size_t i = 0; i < kNumOutputLanes; i++) {
    compressed_ptr[i] = aligned_scratch_memory;
    aligned_scratch_memory += kMT21SubblockSize;
    for (size_t j = 0; j < subblock_list[i + start_idx].len;
         j += kMT21SubblockWidth) {
      memcpy(compressed_ptr[i] + 3 * kMT21SubblockWidth - j,
             subblock_list[i + start_idx].src + j, kMT21SubblockWidth);
    }
    compressed_ptr[i] += kMT21SubblockSize - sizeof(uint32_t);
  }
}

// We compute 1 output per lane with the SIMD decompression algorithm. We could
// store lane individually, but instead we batch the output of all 16
// subblocks, and then do 4 16x16 transposes and write out the results of our
// decompression row by row. This is substantially faster because it's fewer
// instructions and it takes advantage of the write combiner.
//
// The transpose algorithm we use is to break the data into 4x4 blocks, where
// each block is stored in its own register. We then perform 4x4 transposes on
// all 16 of the 4x4 blocks. Then, we rearrange the blocks to complete the
// transpose.
//
// We actually have to transpose the 4x4 blocks in 2 64-bit registers because
// 32-bit ARM lacks a vqtbl1q_u8 instruction. I actually don't know if this is
// terrible for performance since the A72 has the Neon throughput capped at 3
// 64-bit vector registers per cycle anyway. But if we see a drop in performance
// compared to our Aarch64 benchmarks, this may be worth looking into.
static const uint8x8_t kTableTranspose4x4UpperIndices = {
    0, 4, 8, 12, 1, 5, 9, 13,
};
static const uint8x8_t kTableTranspose4x4LowerIndices = {
    2, 6, 10, 14, 3, 7, 11, 15,
};
void SubblockTransposeScatter(uint8_t*& src, uint8_t** decompressed_ptr) {
  uint32x4x4_t load_regs[4];
  uint32x4x4_t store_regs[4];

  // Load 4x4 blocks
  LOOPN(
      {
        load_regs[i] = vld4q_u32((uint32_t*)src);
        src += 64;
      },
      4)

  // Move the source pointer to the next row.
  src -= 2 * kMT21SubblockWidth * kNumOutputLanes;

  // 4x4 transposes using lookup table
  LOOPN(
      {
        uint8x8x2_t table;
        table.val[0] =
            vget_low_u8(vreinterpretq_u8_u32(load_regs[i / 4].val[i % 4]));
        table.val[1] =
            vget_high_u8(vreinterpretq_u8_u32(load_regs[i / 4].val[i % 4]));
        load_regs[i / 4].val[i % 4] =
            vcombine_u32(vreinterpret_u32_u8(
                             vtbl2_u8(table, kTableTranspose4x4UpperIndices)),
                         vreinterpret_u32_u8(
                             vtbl2_u8(table, kTableTranspose4x4LowerIndices)));
      },
      16)

  // Rearrange 4x4 blocks. This probably won't generate any instructions since
  // we're basically just renaming some registers?
  LOOPN({ store_regs[i / 4].val[i % 4] = load_regs[i % 4].val[i / 4]; }, 16)

  // Store the rows.
  // Apparently vst4q_lane_u32 requires a constant integer for the third
  // argument and clang isn't smart enough to realize that unrolling the loop
  // would make the third argument const. So, ctrl-c, ctrl-v.
  vst4q_lane_u32((uint32_t*)decompressed_ptr[0], store_regs[0], 0);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[1], store_regs[0], 1);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[2], store_regs[0], 2);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[3], store_regs[0], 3);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[4], store_regs[1], 0);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[5], store_regs[1], 1);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[6], store_regs[1], 2);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[7], store_regs[1], 3);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[8], store_regs[2], 0);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[9], store_regs[2], 1);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[10], store_regs[2], 2);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[11], store_regs[2], 3);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[12], store_regs[3], 0);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[13], store_regs[3], 1);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[14], store_regs[3], 2);
  vst4q_lane_u32((uint32_t*)decompressed_ptr[15], store_regs[3], 3);
}

// Decompresses a sublock. We take width and stride parameters to let us recycle
// code between Y subblock and UV subblock decompression routines. UV subblocks
// just halve the width, but skip every other pixel.
template <class T, int width, int stride>
void VectorDecompressSubblock(const std::vector<T>& subblock_list,
                              int start_idx,
                              uint8_t** compressed_ptr,
                              uint8_t* output_buf,
                              uint32x4_t* outstanding_reads,
                              uint32x4_t* accumulator) {
  static const int pixel_distance = stride / width;

  uint32x4_t escape_codes[4];
  uint32x4_t escape_binary_len_diff[4];
  uint32x4_t k_vals[4];
  uint32x4_t dword_solid_color_mask[4];
  uint8x16_t solid_color_mask;
  uint8x16_t output, up_left, up, up_right, right;

  output = VectorReadCompressedHeader(
      accumulator, outstanding_reads, escape_codes, escape_binary_len_diff,
      k_vals, dword_solid_color_mask, solid_color_mask, compressed_ptr);
  right = output;
  vst1q_u8(output_buf, output);
  output_buf -= pixel_distance * kNumOutputLanes;

  for (int i = 0; i < width - 1; i++) {
    // Handle first row
    output = VectorReadGolombRiceSymbol(
        accumulator, outstanding_reads, escape_codes, escape_binary_len_diff,
        k_vals, dword_solid_color_mask, compressed_ptr);

    output = vandq_u8(output, solid_color_mask);
    output = vaddq_u8(output, VectorFirstRowPrediction(right));

    right = output;

    vst1q_u8(output_buf, output);
    output_buf -= pixel_distance * kNumOutputLanes;
  }
  for (size_t y = 1; y < kMT21SubblockHeight; y++) {
    // Handle last col
    up = vld1q_u8(output_buf + stride * kNumOutputLanes);

    output = VectorReadGolombRiceSymbol(
        accumulator, outstanding_reads, escape_codes, escape_binary_len_diff,
        k_vals, dword_solid_color_mask, compressed_ptr);

    output = vandq_u8(output, solid_color_mask);
    output = vaddq_u8(output, VectorLastColPrediction(up));

    up_right = up;
    right = output;

    vst1q_u8(output_buf, output);
    output_buf -= pixel_distance * kNumOutputLanes;

    up = vld1q_u8(output_buf + stride * kNumOutputLanes);

    for (int x = width - 2; x >= 1; x--) {
      // Handle body
      up_left =
          vld1q_u8(output_buf + (stride - pixel_distance) * kNumOutputLanes);

      output = VectorReadGolombRiceSymbol(
          accumulator, outstanding_reads, escape_codes, escape_binary_len_diff,
          k_vals, dword_solid_color_mask, compressed_ptr);

      output = vandq_u8(output, solid_color_mask);
      output =
          vaddq_u8(output, VectorBodyPrediction(up_left, up, up_right, right));

      right = output;
      up_right = up;
      up = up_left;

      vst1q_u8(output_buf, output);
      output_buf -= pixel_distance * kNumOutputLanes;
    }
    // Handle first col
    output = VectorReadGolombRiceSymbol(
        accumulator, outstanding_reads, escape_codes, escape_binary_len_diff,
        k_vals, dword_solid_color_mask, compressed_ptr);

    output = vandq_u8(output, solid_color_mask);
    output = vaddq_u8(output, VectorFirstColPrediction(up, up_right, right));
    vst1q_u8(output_buf, output);
    output_buf -= pixel_distance * kNumOutputLanes;
  }
}

// Main entrypoint for vector decompression.
template <class T>
void VectorDecompressSubblockHelper(const std::vector<T>& subblock_list,
                                    int start_idx,
                                    uint8_t* aligned_scratch) {}
template <>
void VectorDecompressSubblockHelper(
    const std::vector<MT21YSubblock>& subblock_list,
    int start_idx,
    uint8_t* aligned_scratch) {
  uint8_t* compressed_ptr[kNumOutputLanes];
  uint8_t* decompressed_ptr[kNumOutputLanes];
  uint8_t* output_buf = aligned_scratch + 2 * kMT21RedZoneSize +
                        kNumOutputLanes * kMT21SubblockSize - kNumOutputLanes;
  uint32x4_t outstanding_reads[4] = {{0}};
  uint32x4_t accumulator[4];

  SubblockGather<MT21YSubblock>(subblock_list, start_idx, aligned_scratch,
                                compressed_ptr);
  for (size_t i = 0; i < kNumOutputLanes; i++) {
    decompressed_ptr[i] = subblock_list[start_idx + i].dest;
  }

  VectorInitializeAccumulator(accumulator, compressed_ptr);

  VectorDecompressSubblock<MT21YSubblock, kMT21SubblockWidth,
                           kMT21SubblockWidth>(subblock_list, start_idx,
                                               compressed_ptr, output_buf,
                                               outstanding_reads, accumulator);

  output_buf -= kNumOutputLanes * kMT21SubblockWidth - kNumOutputLanes;
  for (int i = 0; i < 4; i++) {
    SubblockTransposeScatter(output_buf, decompressed_ptr);
    for (int j = 0; j < 16; j++) {
      decompressed_ptr[j] += kMT21SubblockWidth;
    }
  }
}
template <>
void VectorDecompressSubblockHelper(
    const std::vector<MT21UVSubblock>& subblock_list,
    int start_idx,
    uint8_t* aligned_scratch) {
  uint8_t* compressed_ptr[16];
  uint8_t* decompressed_ptr[16];
  uint8_t* output_buf = aligned_scratch + 2 * kMT21RedZoneSize +
                        kNumOutputLanes * kMT21SubblockSize - kNumOutputLanes;
  uint32x4_t outstanding_reads[4] = {{0}};
  uint32x4_t accumulator[4];

  SubblockGather<MT21UVSubblock>(subblock_list, start_idx, aligned_scratch,
                                 compressed_ptr);
  for (int i = 0; i < 16; i++) {
    decompressed_ptr[i] = subblock_list[start_idx + i].dest;
  }

  VectorInitializeAccumulator(accumulator, compressed_ptr);

  VectorDecompressSubblock<MT21UVSubblock, kMT21SubblockWidth / 2,
                           kMT21SubblockWidth>(subblock_list, start_idx,
                                               compressed_ptr, output_buf,
                                               outstanding_reads, accumulator);
  VectorDecompressSubblock<MT21UVSubblock, kMT21SubblockWidth / 2,
                           kMT21SubblockWidth>(subblock_list, start_idx,
                                               compressed_ptr, output_buf - 16,
                                               outstanding_reads, accumulator);

  output_buf -= 16 * 16 - 16;
  for (int i = 0; i < 4; i++) {
    SubblockTransposeScatter(output_buf, decompressed_ptr);
    for (int j = 0; j < 16; j++) {
      decompressed_ptr[j] += kMT21SubblockWidth;
    }
  }
}

////////////////////
// Footer Parsing //
////////////////////

constexpr uint64_t kMT21YFooterAlignment = 4096;
constexpr uint64_t kMT21UVFooterAlignment = 2048;

// MT21 always puts the footer for the Y plane at the beginning of the last page
// in the buffer. For some reason, it puts the UV plane at the beginning of the
// last half-page, meaning the UV footer buffer is 2048 bytes aligned.
size_t ComputeFooterOffset(size_t plane_size,
                           size_t buf_size,
                           size_t alignment) {
  size_t footer_size = plane_size / kMT21BlockSize / 2;
  return ((buf_size - footer_size) & (~(alignment - 1)));
}

// The footer consists of packed 2-bit fields indicating the size of every
// subblock in 16 byte rows.
void ParseBlockMetadata(const uint8_t* footer,
                        size_t block_offset,
                        size_t& subblock1_len,
                        size_t& subblock2_len) {
  // Footer metadata is packed in 2-bit pairs from LSB to MSB. This means we can
  // pack 4 subblocks, or 2 blocks into every byte of footer.
  const size_t block_idx = block_offset / kMT21BlockSize;
  subblock1_len =
      kMT21BlockWidth *
      (((footer[block_idx / 2] >> ((block_idx % 2) * 4)) & 0x3) + 1);
  subblock2_len =
      kMT21BlockWidth *
      (((footer[block_idx / 2] >> ((block_idx % 2) * 4 + 2)) & 0x3) + 1);
}

// Subblocks with a compressed size of 64 bytes are actually passthrough
// subblocks. This is a handy function for sorting subblocks into passthrough
// and non-passthrough bins. We can just use a memcpy for passthrough, which is
// cheaper than somehow incorporating passthrough logic into our decompression
// routines.
template <class T>
void BinSubblocks(const uint8_t* src,
                  const uint8_t* footer,
                  uint8_t* dest,
                  size_t block_offset,
                  std::vector<T>* subblock_bins) {
  size_t subblock1_len, subblock2_len;
  ParseBlockMetadata(footer, block_offset, subblock1_len, subblock2_len);
  T subblock1 = {src + block_offset, dest + block_offset, subblock1_len};
  T subblock2 = {src + block_offset + subblock1_len,
                 dest + block_offset + kMT21SubblockSize, subblock2_len};
  int subblock1_type = subblock1_len == kMT21SubblockSize;
  int subblock2_type = subblock2_len == kMT21SubblockSize;

  subblock_bins[subblock1_type].push_back(subblock1);
  subblock_bins[subblock2_type].push_back(subblock2);
}

}  // namespace

}  // namespace media

#endif  // MEDIA_GPU_V4L2_MT21_MT21_UTIL_H_
