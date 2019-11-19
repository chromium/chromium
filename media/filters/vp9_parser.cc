// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of a VP9 bitstream parser.
//
// VERBOSE level:
//  1 something wrong in bitstream
//  2 parsing steps
//  3 parsed values (selected)

#include "media/filters/vp9_parser.h"

#include <algorithm>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/sys_byteorder.h"
#include "media/filters/vp9_compressed_header_parser.h"
#include "media/filters/vp9_uncompressed_header_parser.h"

namespace media {

namespace {

// Coefficients extracted verbatim from "VP9 Bitstream & Decoding Process
// Specification" Version 0.6, Sec 8.6.1 Dequantization functions, see:
// https://www.webmproject.org/vp9/#draft-vp9-bitstream-and-decoding-process-specification
constexpr size_t kQIndexRange = 256;
// clang-format off
// libva is the only user of high bit depth VP9 formats and only supports
// 10 bits per component, see https://github.com/01org/libva/issues/137.
// TODO(mcasas): Add the 12 bit versions of these tables.
const int16_t kDcQLookup[][kQIndexRange] = {
    {
        4,    8,    8,    9,    10,   11,   12,   12,  13,   14,   15,   16,
        17,   18,   19,   19,   20,   21,   22,   23,  24,   25,   26,   26,
        27,   28,   29,   30,   31,   32,   32,   33,  34,   35,   36,   37,
        38,   38,   39,   40,   41,   42,   43,   43,  44,   45,   46,   47,
        48,   48,   49,   50,   51,   52,   53,   53,  54,   55,   56,   57,
        57,   58,   59,   60,   61,   62,   62,   63,  64,   65,   66,   66,
        67,   68,   69,   70,   70,   71,   72,   73,  74,   74,   75,   76,
        77,   78,   78,   79,   80,   81,   81,   82,  83,   84,   85,   85,
        87,   88,   90,   92,   93,   95,   96,   98,  99,   101,  102,  104,
        105,  107,  108,  110,  111,  113,  114,  116, 117,  118,  120,  121,
        123,  125,  127,  129,  131,  134,  136,  138, 140,  142,  144,  146,
        148,  150,  152,  154,  156,  158,  161,  164, 166,  169,  172,  174,
        177,  180,  182,  185,  187,  190,  192,  195, 199,  202,  205,  208,
        211,  214,  217,  220,  223,  226,  230,  233, 237,  240,  243,  247,
        250,  253,  257,  261,  265,  269,  272,  276, 280,  284,  288,  292,
        296,  300,  304,  309,  313,  317,  322,  326, 330,  335,  340,  344,
        349,  354,  359,  364,  369,  374,  379,  384, 389,  395,  400,  406,
        411,  417,  423,  429,  435,  441,  447,  454, 461,  467,  475,  482,
        489,  497,  505,  513,  522,  530,  539,  549, 559,  569,  579,  590,
        602,  614,  626,  640,  654,  668,  684,  700, 717,  736,  755,  775,
        796,  819,  843,  869,  896,  925,  955,  988, 1022, 1058, 1098, 1139,
        1184, 1232, 1282, 1336,
    },
    {
        4,    9,    10,   13,   15,   17,   20,   22,   25,   28,   31,   34,
        37,   40,   43,   47,   50,   53,   57,   60,   64,   68,   71,   75,
        78,   82,   86,   90,   93,   97,   101,  105,  109,  113,  116,  120,
        124,  128,  132,  136,  140,  143,  147,  151,  155,  159,  163,  166,
        170,  174,  178,  182,  185,  189,  193,  197,  200,  204,  208,  212,
        215,  219,  223,  226,  230,  233,  237,  241,  244,  248,  251,  255,
        259,  262,  266,  269,  273,  276,  280,  283,  287,  290,  293,  297,
        300,  304,  307,  310,  314,  317,  321,  324,  327,  331,  334,  337,
        343,  350,  356,  362,  369,  375,  381,  387,  394,  400,  406,  412,
        418,  424,  430,  436,  442,  448,  454,  460,  466,  472,  478,  484,
        490,  499,  507,  516,  525,  533,  542,  550,  559,  567,  576,  584,
        592,  601,  609,  617,  625,  634,  644,  655,  666,  676,  687,  698,
        708,  718,  729,  739,  749,  759,  770,  782,  795,  807,  819,  831,
        844,  856,  868,  880,  891,  906,  920,  933,  947,  961,  975,  988,
        1001, 1015, 1030, 1045, 1061, 1076, 1090, 1105, 1120, 1137, 1153, 1170,
        1186, 1202, 1218, 1236, 1253, 1271, 1288, 1306, 1323, 1342, 1361, 1379,
        1398, 1416, 1436, 1456, 1476, 1496, 1516, 1537, 1559, 1580, 1601, 1624,
        1647, 1670, 1692, 1717, 1741, 1766, 1791, 1817, 1844, 1871, 1900, 1929,
        1958, 1990, 2021, 2054, 2088, 2123, 2159, 2197, 2236, 2276, 2319, 2363,
        2410, 2458, 2508, 2561, 2616, 2675, 2737, 2802, 2871, 2944, 3020, 3102,
        3188, 3280, 3375, 3478, 3586, 3702, 3823, 3953, 4089, 4236, 4394, 4559,
        4737, 4929, 5130, 5347
   }
};

const int16_t kAcQLookup[][kQIndexRange] = {
    {
        4,    8,    9,    10,   11,   12,   13,   14,   15,   16,   17,   18,
        19,   20,   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,
        31,   32,   33,   34,   35,   36,   37,   38,   39,   40,   41,   42,
        43,   44,   45,   46,   47,   48,   49,   50,   51,   52,   53,   54,
        55,   56,   57,   58,   59,   60,   61,   62,   63,   64,   65,   66,
        67,   68,   69,   70,   71,   72,   73,   74,   75,   76,   77,   78,
        79,   80,   81,   82,   83,   84,   85,   86,   87,   88,   89,   90,
        91,   92,   93,   94,   95,   96,   97,   98,   99,   100,  101,  102,
        104,  106,  108,  110,  112,  114,  116,  118,  120,  122,  124,  126,
        128,  130,  132,  134,  136,  138,  140,  142,  144,  146,  148,  150,
        152,  155,  158,  161,  164,  167,  170,  173,  176,  179,  182,  185,
        188,  191,  194,  197,  200,  203,  207,  211,  215,  219,  223,  227,
        231,  235,  239,  243,  247,  251,  255,  260,  265,  270,  275,  280,
        285,  290,  295,  300,  305,  311,  317,  323,  329,  335,  341,  347,
        353,  359,  366,  373,  380,  387,  394,  401,  408,  416,  424,  432,
        440,  448,  456,  465,  474,  483,  492,  501,  510,  520,  530,  540,
        550,  560,  571,  582,  593,  604,  615,  627,  639,  651,  663,  676,
        689,  702,  715,  729,  743,  757,  771,  786,  801,  816,  832,  848,
        864,  881,  898,  915,  933,  951,  969,  988,  1007, 1026, 1046, 1066,
        1087, 1108, 1129, 1151, 1173, 1196, 1219, 1243, 1267, 1292, 1317, 1343,
        1369, 1396, 1423, 1451, 1479, 1508, 1537, 1567, 1597, 1628, 1660, 1692,
        1725, 1759, 1793, 1828,
    },
    {
        4,    9,    11,   13,   16,   18,   21,   24,   27,   30,   33,   37,
        40,   44,   48,   51,   55,   59,   63,   67,   71,   75,   79,   83,
        88,   92,   96,   100,  105,  109,  114,  118,  122,  127,  131,  136,
        140,  145,  149,  154,  158,  163,  168,  172,  177,  181,  186,  190,
        195,  199,  204,  208,  213,  217,  222,  226,  231,  235,  240,  244,
        249,  253,  258,  262,  267,  271,  275,  280,  284,  289,  293,  297,
        302,  306,  311,  315,  319,  324,  328,  332,  337,  341,  345,  349,
        354,  358,  362,  367,  371,  375,  379,  384,  388,  392,  396,  401,
        409,  417,  425,  433,  441,  449,  458,  466,  474,  482,  490,  498,
        506,  514,  523,  531,  539,  547,  555,  563,  571,  579,  588,  596,
        604,  616,  628,  640,  652,  664,  676,  688,  700,  713,  725,  737,
        749,  761,  773,  785,  797,  809,  825,  841,  857,  873,  889,  905,
        922,  938,  954,  970,  986,  1002, 1018, 1038, 1058, 1078, 1098, 1118,
        1138, 1158, 1178, 1198, 1218, 1242, 1266, 1290, 1314, 1338, 1362, 1386,
        1411, 1435, 1463, 1491, 1519, 1547, 1575, 1603, 1631, 1663, 1695, 1727,
        1759, 1791, 1823, 1859, 1895, 1931, 1967, 2003, 2039, 2079, 2119, 2159,
        2199, 2239, 2283, 2327, 2371, 2415, 2459, 2507, 2555, 2603, 2651, 2703,
        2755, 2807, 2859, 2915, 2971, 3027, 3083, 3143, 3203, 3263, 3327, 3391,
        3455, 3523, 3591, 3659, 3731, 3803, 3876, 3952, 4028, 4104, 4184, 4264,
        4348, 4432, 4516, 4604, 4692, 4784, 4876, 4972, 5068, 5168, 5268, 5372,
        5476, 5584, 5692, 5804, 5916, 6032, 6148, 6268, 6388, 6512, 6640, 6768,
        6900, 7036, 7172, 7312
   }
};
// clang-format on

static_assert(base::size(kDcQLookup[0]) == base::size(kAcQLookup[0]),
              "quantizer lookup arrays of incorrect size");

size_t ClampQ(int64_t q) {
  return q < 0 ? 0
               : base::checked_cast<size_t>(
                     std::min(q, static_cast<int64_t>(kQIndexRange - 1)));
}

int ClampLf(int lf) {
  constexpr int kMaxLoopFilterLevel = 63;
  return base::ClampToRange(lf, 0, kMaxLoopFilterLevel);
}

std::string IncrementIV(const std::string& iv, uint32_t by) {
  // What we call the 'IV' value is actually somewhat of a misnomer:
  // "IV" = 0xFFFFFFFFFFFFFFFF0000000000000000
  //          └──actual IV───┘└─block counter┘
  // When we want to 'increment' this structure, we treat them both
  // as big-endian 64 bit unsigned integers, then increment _only_ the
  // block counter, then combine them back into a big-endian bytestring.
  // |by| is usually going to be the number of blocks (aka 16 byte chunks)
  //      of cipher data.
  DCHECK_EQ(iv.size(), 16u);
  uint64_t integral_data[2];
  memcpy(integral_data, reinterpret_cast<const uint8_t*>(iv.data()), 16);
  uint64_t block_counter = base::NetToHost64(integral_data[1]) + by;
  integral_data[1] = base::HostToNet64(block_counter);
  uint8_t new_iv[16];
  memcpy(new_iv, integral_data, 16);
  return std::string(reinterpret_cast<char*>(new_iv), 16);
}

// |frame_size|: The size of the current frame; this controls how long we
//               loop through the subsamples.
// |current_subsample_index|: An index into the |subsamples| vector, we need
//                            to have this saved between function calls.
// |extra_clear_bytes|: The previous call may have set this variable to show
//                      that a subsample mey have already started being parsed
//                      and that only X bytes of free data are left in it.
// |base_decrypt_config|: Not an output parameter, it is just a raw ptr from a
//                        unique_ptr.
// |subsamples|: A vector of subsamples.
// |iv|: The initialization vector (128bit number stored as std::string). This
//       gets incremented by (cipher_bytes % 16) for each frame, and must be
//       preserved across function calls.
std::unique_ptr<DecryptConfig> SplitSubsamples(
    uint32_t frame_size,
    size_t* current_subsample_index,
    size_t* extra_clear_subsample_bytes,
    DecryptConfig* base_decrypt_config,
    const std::vector<SubsampleEntry>& subsamples,
    std::string* iv) {
  // We copy iv so that we can use the starting value in our
  // new config while still incrementing IV for the next frame.
  std::string frame_dc_iv = *iv;
  std::vector<SubsampleEntry> frame_dc_subsamples;
  do {
    if (*current_subsample_index >= subsamples.size()) {
      DVLOG(1) << "Not enough subsamples in the superframe decrypt config";
      return nullptr;
    }

    uint32_t subsample_clear = subsamples[*current_subsample_index].clear_bytes;
    uint32_t subsample_cipher =
        subsamples[*current_subsample_index].cypher_bytes;

    // if clear+cipher bytes would be over the max of uint32_t, we need to
    // quit immediatly, to prevent malicious overflowing.
    if (0xFFFFFFFF - subsample_clear < subsample_cipher) {
      DVLOG(1) << "Invalid subsample alignment";
      return nullptr;
    }

    // It's possible that the previous frame didn't use all the clear bytes
    // in this subsample, in which case we have to start from midway through
    // the clear section.
    if (*extra_clear_subsample_bytes)
      subsample_clear = *extra_clear_subsample_bytes;

    if (subsample_clear > frame_size) {
      // Support scenario where clear section is larger than our frame:
      // The entire length is clear. If |subsample_clear| is the same length,
      // we handle it below.
      frame_dc_subsamples.push_back(SubsampleEntry(frame_size, 0));
      *extra_clear_subsample_bytes = subsample_clear - frame_size;
      frame_size = 0;
    } else if (subsample_clear + subsample_cipher > frame_size) {
      // Only a clear section can cross over a frame boundary, otherwise
      // the frame header for the next frame would be encrypted, which is not
      // spec compliant.
      DVLOG(1) << "Invalid subsample alignment";
      return nullptr;
    } else if (subsample_clear + subsample_cipher <= frame_size) {
      // In this case a subsample is less than or equal to a whole frame
      // This is the most likely case for almost all encrypted media.
      // note that |subsample_cipher| can be 0.
      frame_dc_subsamples.push_back(
          SubsampleEntry(subsample_clear, subsample_cipher));
      frame_size -= (subsample_clear + subsample_cipher);
      *extra_clear_subsample_bytes = 0;

      // IV gets incremented by 1 for every 16 bytes of cypher
      *iv = IncrementIV(*iv, subsample_cipher >> 4);  // uint32 logical shift.
    }

    // Don't go to the next subsample if there are more clear bytes.
    if (!*extra_clear_subsample_bytes)
      (*current_subsample_index)++;

    // It is possible for there to be more than one subsample associated
    // with a single frame, so we need to try again if there are more bytes
    // left unaccounted for in this frame.
  } while (frame_size);

  return base_decrypt_config->CopyNewSubsamplesIV(frame_dc_subsamples,
                                                  frame_dc_iv);
}

bool IsByteNEncrypted(off_t byte,
                      const std::vector<SubsampleEntry>& subsamples) {
  off_t original_byte = byte;
  for (const SubsampleEntry& subsample : subsamples) {
    if (byte < 0)
      return false;
    if (static_cast<uint32_t>(byte) < subsample.clear_bytes)
      return false;
    byte -= subsample.clear_bytes;
    if (static_cast<uint32_t>(byte) < subsample.cypher_bytes)
      return true;
    byte -= subsample.cypher_bytes;
  }
  DVLOG(3) << "Subsamples do not extend to cover offset " << original_byte;
  return false;
}

}  // namespace

bool Vp9FrameHeader::IsKeyframe() const {
  // When show_existing_frame is true, the frame header does not precede an
  // actual frame to be decoded, so frame_type does not apply (and is not read
  // from the stream).
  return !show_existing_frame && frame_type == KEYFRAME;
}

bool Vp9FrameHeader::IsIntra() const {
  return !show_existing_frame && (frame_type == KEYFRAME || intra_only);
}

VideoColorSpace Vp9FrameHeader::GetColorSpace() const {
  VideoColorSpace ret;
  ret.range = color_range ? gfx::ColorSpace::RangeID::FULL
                          : gfx::ColorSpace::RangeID::LIMITED;
  switch (color_space) {
    case Vp9ColorSpace::RESERVED:
    case Vp9ColorSpace::UNKNOWN:
      break;
    case Vp9ColorSpace::BT_601:
    case Vp9ColorSpace::SMPTE_170:
      ret.primaries = VideoColorSpace::PrimaryID::SMPTE170M;
      ret.transfer = VideoColorSpace::TransferID::SMPTE170M;
      ret.matrix = VideoColorSpace::MatrixID::SMPTE170M;
      break;
    case Vp9ColorSpace::BT_709:
      ret.primaries = VideoColorSpace::PrimaryID::BT709;
      ret.transfer = VideoColorSpace::TransferID::BT709;
      ret.matrix = VideoColorSpace::MatrixID::BT709;
      break;
    case Vp9ColorSpace::SMPTE_240:
      ret.primaries = VideoColorSpace::PrimaryID::SMPTE240M;
      ret.transfer = VideoColorSpace::TransferID::SMPTE240M;
      ret.matrix = VideoColorSpace::MatrixID::SMPTE240M;
      break;
    case Vp9ColorSpace::BT_2020:
      ret.primaries = VideoColorSpace::PrimaryID::BT2020;
      ret.transfer = VideoColorSpace::TransferID::BT2020_10;
      ret.matrix = VideoColorSpace::MatrixID::BT2020_NCL;
      break;
    case Vp9ColorSpace::SRGB:
      ret.primaries = VideoColorSpace::PrimaryID::BT709;
      ret.transfer = VideoColorSpace::TransferID::IEC61966_2_1;
      ret.matrix = VideoColorSpace::MatrixID::BT709;
      break;
  }
  return ret;
}

Vp9Parser::FrameInfo::FrameInfo() = default;

Vp9Parser::FrameInfo::~FrameInfo() = default;

Vp9Parser::FrameInfo::FrameInfo(const uint8_t* ptr, off_t size)
    : ptr(ptr), size(size) {}

Vp9Parser::FrameInfo::FrameInfo(const FrameInfo& copy_from)
    : ptr(copy_from.ptr),
      size(copy_from.size),
      allocate_size(copy_from.allocate_size),
      decrypt_config(copy_from.decrypt_config
                         ? copy_from.decrypt_config->Clone()
                         : nullptr) {}

Vp9Parser::FrameInfo& Vp9Parser::FrameInfo::operator=(
    const FrameInfo& copy_from) {
  this->ptr = copy_from.ptr;
  this->size = copy_from.size;
  this->allocate_size = copy_from.allocate_size;
  this->decrypt_config =
      copy_from.decrypt_config ? copy_from.decrypt_config->Clone() : nullptr;
  return *this;
}

bool Vp9FrameContext::IsValid() const {
  // probs should be in [1, 255] range.
  static_assert(sizeof(Vp9Prob) == 1,
                "following checks assuming Vp9Prob is single byte");
  if (memchr(tx_probs_8x8, 0, sizeof(tx_probs_8x8)))
    return false;
  if (memchr(tx_probs_16x16, 0, sizeof(tx_probs_16x16)))
    return false;
  if (memchr(tx_probs_32x32, 0, sizeof(tx_probs_32x32)))
    return false;

  for (auto& a : coef_probs) {
    for (auto& ai : a) {
      for (auto& aj : ai) {
        for (auto& ak : aj) {
          int max_l = (ak == aj[0]) ? 3 : 6;
          for (int l = 0; l < max_l; l++) {
            for (auto& x : ak[l]) {
              if (x == 0)
                return false;
            }
          }
        }
      }
    }
  }
  if (memchr(skip_prob, 0, sizeof(skip_prob)))
    return false;
  if (memchr(inter_mode_probs, 0, sizeof(inter_mode_probs)))
    return false;
  if (memchr(interp_filter_probs, 0, sizeof(interp_filter_probs)))
    return false;
  if (memchr(is_inter_prob, 0, sizeof(is_inter_prob)))
    return false;
  if (memchr(comp_mode_prob, 0, sizeof(comp_mode_prob)))
    return false;
  if (memchr(single_ref_prob, 0, sizeof(single_ref_prob)))
    return false;
  if (memchr(comp_ref_prob, 0, sizeof(comp_ref_prob)))
    return false;
  if (memchr(y_mode_probs, 0, sizeof(y_mode_probs)))
    return false;
  if (memchr(uv_mode_probs, 0, sizeof(uv_mode_probs)))
    return false;
  if (memchr(partition_probs, 0, sizeof(partition_probs)))
    return false;
  if (memchr(mv_joint_probs, 0, sizeof(mv_joint_probs)))
    return false;
  if (memchr(mv_sign_prob, 0, sizeof(mv_sign_prob)))
    return false;
  if (memchr(mv_class_probs, 0, sizeof(mv_class_probs)))
    return false;
  if (memchr(mv_class0_bit_prob, 0, sizeof(mv_class0_bit_prob)))
    return false;
  if (memchr(mv_bits_prob, 0, sizeof(mv_bits_prob)))
    return false;
  if (memchr(mv_class0_fr_probs, 0, sizeof(mv_class0_fr_probs)))
    return false;
  if (memchr(mv_fr_probs, 0, sizeof(mv_fr_probs)))
    return false;
  if (memchr(mv_class0_hp_prob, 0, sizeof(mv_class0_hp_prob)))
    return false;
  if (memchr(mv_hp_prob, 0, sizeof(mv_hp_prob)))
    return false;

  return true;
}

Vp9Parser::Context::Vp9FrameContextManager::Vp9FrameContextManager() {}

Vp9Parser::Context::Vp9FrameContextManager::~Vp9FrameContextManager() = default;

const Vp9FrameContext&
Vp9Parser::Context::Vp9FrameContextManager::frame_context() const {
  DCHECK(initialized_);
  DCHECK(!needs_client_update_);
  return frame_context_;
}

void Vp9Parser::Context::Vp9FrameContextManager::Reset() {
  initialized_ = false;
  needs_client_update_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void Vp9Parser::Context::Vp9FrameContextManager::SetNeedsClientUpdate() {
  DCHECK(!needs_client_update_);
  initialized_ = true;
  needs_client_update_ = true;
}

Vp9Parser::ContextRefreshCallback
Vp9Parser::Context::Vp9FrameContextManager::GetUpdateCb() {
  if (needs_client_update_)
    return base::Bind(&Vp9FrameContextManager::UpdateFromClient,
                      weak_ptr_factory_.GetWeakPtr());
  else
    return Vp9Parser::ContextRefreshCallback();
}

void Vp9Parser::Context::Vp9FrameContextManager::Update(
    const Vp9FrameContext& frame_context) {
  // DCHECK because we can trust values from our parser.
  DCHECK(frame_context.IsValid());
  initialized_ = true;
  frame_context_ = frame_context;

  // For frame context we are updating, it may be still awaiting previous
  // ContextRefreshCallback. Because we overwrite the value of context here and
  // previous ContextRefreshCallback no longer matters, invalidate the weak ptr
  // to prevent previous ContextRefreshCallback run.
  // With this optimization, we may be able to parse more frames while previous
  // are still decoding.
  weak_ptr_factory_.InvalidateWeakPtrs();
  needs_client_update_ = false;
}

void Vp9Parser::Context::Vp9FrameContextManager::UpdateFromClient(
    const Vp9FrameContext& frame_context) {
  DVLOG(2) << "Got external frame_context update";
  DCHECK(needs_client_update_);
  if (!frame_context.IsValid()) {
    DLOG(ERROR) << "Invalid prob value in frame_context";
    return;
  }
  needs_client_update_ = false;
  initialized_ = true;
  frame_context_ = frame_context;
}

void Vp9Parser::Context::Reset() {
  memset(&segmentation_, 0, sizeof(segmentation_));
  memset(&loop_filter_, 0, sizeof(loop_filter_));
  memset(&ref_slots_, 0, sizeof(ref_slots_));
  for (auto& manager : frame_context_managers_)
    manager.Reset();
}

void Vp9Parser::Context::MarkFrameContextForUpdate(size_t frame_context_idx) {
  DCHECK_LT(frame_context_idx, base::size(frame_context_managers_));
  frame_context_managers_[frame_context_idx].SetNeedsClientUpdate();
}

void Vp9Parser::Context::UpdateFrameContext(
    size_t frame_context_idx,
    const Vp9FrameContext& frame_context) {
  DCHECK_LT(frame_context_idx, base::size(frame_context_managers_));
  frame_context_managers_[frame_context_idx].Update(frame_context);
}

const Vp9Parser::ReferenceSlot& Vp9Parser::Context::GetRefSlot(
    size_t ref_type) const {
  DCHECK_LT(ref_type, base::size(ref_slots_));
  return ref_slots_[ref_type];
}

void Vp9Parser::Context::UpdateRefSlot(
    size_t ref_type,
    const Vp9Parser::ReferenceSlot& ref_slot) {
  DCHECK_LT(ref_type, base::size(ref_slots_));
  ref_slots_[ref_type] = ref_slot;
}

Vp9Parser::Vp9Parser(bool parsing_compressed_header)
    : parsing_compressed_header_(parsing_compressed_header) {
  Reset();
}

Vp9Parser::~Vp9Parser() = default;

void Vp9Parser::SetStream(const uint8_t* stream,
                          off_t stream_size,
                          const std::vector<uint32_t>& spatial_layer_frame_size,
                          std::unique_ptr<DecryptConfig> stream_config) {
  DCHECK(stream);
  stream_ = stream;
  bytes_left_ = stream_size;
  frames_.clear();
  spatial_layer_frame_size_ = spatial_layer_frame_size;
  stream_decrypt_config_ = std::move(stream_config);
}

void Vp9Parser::SetStream(const uint8_t* stream,
                          off_t stream_size,
                          std::unique_ptr<DecryptConfig> stream_config) {
  SetStream(stream, stream_size, {}, std::move(stream_config));
}

void Vp9Parser::Reset() {
  stream_ = nullptr;
  bytes_left_ = 0;
  frames_.clear();
  spatial_layer_frame_size_.clear();
  curr_frame_info_.Reset();

  context_.Reset();
}

bool Vp9Parser::ParseUncompressedHeader(const FrameInfo& frame_info,
                                        Vp9FrameHeader* fhdr,
                                        Result* result,
                                        Vp9Parser::Context* context) {
  memset(&curr_frame_header_, 0, sizeof(curr_frame_header_));
  *result = kInvalidStream;

  Vp9UncompressedHeaderParser uncompressed_parser(context);
  if (!uncompressed_parser.Parse(frame_info.ptr, frame_info.size,
                                 &curr_frame_header_)) {
    *result = kInvalidStream;
    return true;
  }

  if (curr_frame_header_.header_size_in_bytes == 0) {
    // Verify padding bits are zero.
    for (off_t i = curr_frame_header_.uncompressed_header_size;
         i < frame_info.size; i++) {
      if (frame_info.ptr[i] != 0) {
        DVLOG(1) << "Padding bits are not zeros.";
        *result = kInvalidStream;
        return true;
      }
    }
    *fhdr = curr_frame_header_;
    *result = kOk;
    return true;
  }
  if (curr_frame_header_.uncompressed_header_size +
          curr_frame_header_.header_size_in_bytes >
      base::checked_cast<size_t>(frame_info.size)) {
    DVLOG(1) << "header_size_in_bytes="
             << curr_frame_header_.header_size_in_bytes
             << " is larger than bytes left in buffer: "
             << frame_info.size - curr_frame_header_.uncompressed_header_size;
    *result = kInvalidStream;
    return true;
  }

  return false;
}

bool Vp9Parser::ParseCompressedHeader(const FrameInfo& frame_info,
                                      Result* result) {
  *result = kInvalidStream;
  size_t frame_context_idx = curr_frame_header_.frame_context_idx;
  const Context::Vp9FrameContextManager& context_to_load =
      context_.frame_context_managers_[frame_context_idx];
  if (!context_to_load.initialized()) {
    // 8.2 Frame order constraints
    // must load an initialized set of probabilities.
    DVLOG(1) << "loading uninitialized frame context, index="
             << frame_context_idx;
    *result = kInvalidStream;
    return true;
  }
  if (context_to_load.needs_client_update()) {
    DVLOG(3) << "waiting frame_context_idx=" << frame_context_idx
             << " to update";
    curr_frame_info_ = frame_info;
    *result = kAwaitingRefresh;
    return true;
  }
  curr_frame_header_.initial_frame_context = curr_frame_header_.frame_context =
      context_to_load.frame_context();

  Vp9CompressedHeaderParser compressed_parser;
  if (!compressed_parser.Parse(
          frame_info.ptr + curr_frame_header_.uncompressed_header_size,
          curr_frame_header_.header_size_in_bytes, &curr_frame_header_)) {
    *result = kInvalidStream;
    return true;
  }

  if (curr_frame_header_.refresh_frame_context) {
    // In frame parallel mode, we can refresh the context without decoding
    // tile data.
    if (curr_frame_header_.frame_parallel_decoding_mode) {
      context_.UpdateFrameContext(frame_context_idx,
                                  curr_frame_header_.frame_context);
    } else {
      context_.MarkFrameContextForUpdate(frame_context_idx);
    }
  }
  return false;
}

Vp9Parser::Result Vp9Parser::ParseNextFrame(
    Vp9FrameHeader* fhdr,
    gfx::Size* allocate_size,
    std::unique_ptr<DecryptConfig>* frame_decrypt_config) {
  DCHECK(fhdr);
  DCHECK(allocate_size);
  DVLOG(2) << "ParseNextFrame";
  FrameInfo frame_info;
  Result result;

  // If |curr_frame_info_| is valid, uncompressed header was parsed into
  // |curr_frame_header_| and we are awaiting context update to proceed with
  // compressed header parsing.
  if (curr_frame_info_.IsValid()) {
    DCHECK(parsing_compressed_header_);
    frame_info = curr_frame_info_;
    curr_frame_info_.Reset();
  } else {
    if (frames_.empty()) {
      // No frames to be decoded, if there is no more stream, request more.
      if (!stream_)
        return kEOStream;

      // New stream to be parsed, parse it and fill frames_.
      if (!spatial_layer_frame_size_.empty()) {
        // If it is SVC stream, we have to parse the stream with
        // |spatial_layer_frame_size_|.
        frames_ = ParseSVCFrame();
      } else {
        frames_ = ParseSuperframe();
      }

      if (frames_.empty()) {
        DVLOG(1) << "Failed parsing superframes/SVC frame";
        return kInvalidStream;
      }
    }

    frame_info = frames_.front();
    frames_.pop_front();
    if (frame_decrypt_config) {
      if (frame_info.decrypt_config)
        *frame_decrypt_config = frame_info.decrypt_config->Clone();
      else
        *frame_decrypt_config = nullptr;
    }

    if (ParseUncompressedHeader(frame_info, fhdr, &result, &context_))
      return result;
  }

  if (parsing_compressed_header_) {
    if (ParseCompressedHeader(frame_info, &result)) {
      DCHECK(result != kAwaitingRefresh || curr_frame_info_.IsValid());
      return result;
    }
  }

  if (!SetupSegmentationDequant())
    return kInvalidStream;
  SetupLoopFilter();
  UpdateSlots(&context_);

  *fhdr = curr_frame_header_;
  // show_frame must be true for the last frame, otherwise false in SVC frame.
  if (!spatial_layer_frame_size_.empty())
    fhdr->show_frame = frames_.empty();

  if (frame_info.allocate_size.IsEmpty()) {
    allocate_size->SetSize(fhdr->frame_width, fhdr->frame_height);
  } else {
    *allocate_size = frame_info.allocate_size;
  }

  return kOk;
}

Vp9Parser::ContextRefreshCallback Vp9Parser::GetContextRefreshCb(
    size_t frame_context_idx) {
  DCHECK_LT(frame_context_idx, base::size(context_.frame_context_managers_));
  auto& frame_context_manager =
      context_.frame_context_managers_[frame_context_idx];

  return frame_context_manager.GetUpdateCb();
}

std::unique_ptr<DecryptConfig> Vp9Parser::NextFrameDecryptContextForTesting() {
  if (frames_.empty()) {
    // No frames to be decoded, if there is no more stream, request more.
    if (!stream_)
      return nullptr;

    // New stream to be parsed, parse it and fill frames_.
    frames_ = ParseSuperframe();
    if (frames_.empty())
      return nullptr;
  }
  FrameInfo frame_info = std::move(frames_.front());
  frames_.pop_front();
  return std::move(frame_info.decrypt_config);
}

std::string Vp9Parser::IncrementIVForTesting(const std::string& iv,
                                             uint32_t by) {
  return IncrementIV(iv, by);
}

// Annex B Superframes
base::circular_deque<Vp9Parser::FrameInfo> Vp9Parser::ParseSuperframe() {
  const uint8_t* stream = stream_;
  off_t bytes_left = bytes_left_;

  // Make sure we don't parse stream_ more than once.
  stream_ = nullptr;
  bytes_left_ = 0;

  base::circular_deque<FrameInfo> frames;

  if (bytes_left < 1)
    return frames;

  // The marker byte might be encrypted, in which case we should treat
  // the stream as a single frame.
  off_t marker_offset = bytes_left - 1;
  if (stream_decrypt_config_) {
    if (IsByteNEncrypted(marker_offset, stream_decrypt_config_->subsamples())) {
      frames.push_back(FrameInfo(stream, bytes_left));
      frames[0].decrypt_config = stream_decrypt_config_->Clone();
      return frames;
    }
  }

  // If this is a superframe, the last byte in the stream will contain the
  // superframe marker. If not, the whole buffer contains a single frame.
  uint8_t marker = *(stream + marker_offset);
  if ((marker & 0xe0) != 0xc0) {
    frames.push_back(FrameInfo(stream, bytes_left));
    if (stream_decrypt_config_)
      frames[0].decrypt_config = stream_decrypt_config_->Clone();
    return frames;
  }

  DVLOG(1) << "Parsing a superframe";

  // The bytes immediately before the superframe marker constitute superframe
  // index, which stores information about sizes of each frame in it.
  // Calculate its size and set index_ptr to the beginning of it.
  size_t num_frames = (marker & 0x7) + 1;
  size_t mag = ((marker >> 3) & 0x3) + 1;
  off_t index_size = 2 + mag * num_frames;

  if (bytes_left < index_size)
    return base::circular_deque<FrameInfo>();

  const uint8_t* index_ptr = stream + bytes_left - index_size;
  if (marker != *index_ptr)
    return base::circular_deque<FrameInfo>();

  ++index_ptr;
  bytes_left -= index_size;

  // Parse frame information contained in the index and add a pointer to and
  // size of each frame to frames.

  // Use this to calculate the per-frame IV value.
  std::string iv;
  std::vector<SubsampleEntry> subsamples;
  size_t current_subsample = 0;
  size_t extra_clear_subsample_bytes = 0;
  if (stream_decrypt_config_) {
    iv = stream_decrypt_config_->iv();
    subsamples = stream_decrypt_config_->subsamples();
  }

  for (size_t i = 0; i < num_frames; ++i) {
    uint32_t size = 0;
    for (size_t j = 0; j < mag; ++j) {
      size |= *index_ptr << (j * 8);
      ++index_ptr;
    }

    if (!base::IsValueInRangeForNumericType<off_t>(size) ||
        static_cast<off_t>(size) > bytes_left) {
      DVLOG(1) << "Not enough data in the buffer for frame " << i;
      frames.clear();
      return frames;
    }

    FrameInfo frame = FrameInfo(stream, size);
    if (subsamples.size()) {
      std::unique_ptr<DecryptConfig> frame_dc = SplitSubsamples(
          size, &current_subsample, &extra_clear_subsample_bytes,
          stream_decrypt_config_.get(), subsamples, &iv);
      if (!frame_dc) {
        DVLOG(1) << "Failed to calculate decrypt config for frame " << i;
        frames.clear();
        return frames;
      }

      frame.decrypt_config = std::move(frame_dc);
    }

    frames.push_back(std::move(frame));
    stream += size;
    bytes_left -= size;

    DVLOG(1) << "Frame " << i << ", size: " << size;
  }

  return frames;
}

base::circular_deque<Vp9Parser::FrameInfo> Vp9Parser::ParseSVCFrame() {
  if (parsing_compressed_header_) {
    LOG(ERROR) << "Vp9Parser doesn't support parsing SVC stream when "
               << "a compressed header needs to be parsed";
    return {};
  }
  if (stream_decrypt_config_) {
    LOG(ERROR) << "Encrypted frame with SVC stream is not supported";
    return {};
  }

  const uint8_t* stream = stream_;
  off_t bytes_left = bytes_left_;

  // Make sure we don't parse stream_ more than once.
  stream_ = nullptr;
  bytes_left_ = 0;

  base::circular_deque<FrameInfo> frames;

  for (size_t i = 0; i < spatial_layer_frame_size_.size(); i++) {
    const uint32_t size = spatial_layer_frame_size_[i];
    if (!base::IsValueInRangeForNumericType<off_t>(size) ||
        static_cast<off_t>(size) > bytes_left) {
      DVLOG(1) << "Not enough data in the buffer for frame " << i;
      return {};
    }

    frames.emplace_back(stream, size);
    stream += size;
    bytes_left -= size;
    DVLOG(1) << "Frame " << i << ", size: " << size;
  }

  DCHECK(!frames_.empty());

  gfx::Size max_frame_size;

  // Context is not copyable because it has base::WeakPtrFactory. The weak
  // pointer is necessary to update context for compressed header. To parse
  // uncompressed header, |segmentation_|, |loop_filter| and |ref_slots_| are
  // sufficient. Copy the variables manually here.
  Context tmp_context;
  tmp_context.segmentation_ = context_.segmentation_;
  tmp_context.loop_filter_ = context_.loop_filter_;
  memcpy(tmp_context.ref_slots_, context_.ref_slots_,
         sizeof(context_.ref_slots_));
  for (const auto& frame_info : frames) {
    // |curr_frame_header_| is used safely because it is reset every
    // ParseUncompressedHeader().
    Vp9FrameHeader dummy_fhdr;
    Result result;
    if (ParseUncompressedHeader(frame_info, &dummy_fhdr, &result,
                                &tmp_context) &&
        result != kOk) {
      return {};
    }
    UpdateSlots(&tmp_context);
    max_frame_size.SetToMax(gfx::Size(curr_frame_header_.frame_width,
                                      curr_frame_header_.frame_height));
  }

  for (auto& frame_info : frames)
    frame_info.allocate_size = max_frame_size;
  return frames;
}

// 8.6.1 Dequantization functions
int64_t Vp9Parser::GetQIndex(const Vp9QuantizationParams& quant,
                             size_t segid) const {
  const Vp9SegmentationParams& segmentation = context_.segmentation();

  if (segmentation.FeatureEnabled(segid,
                                  Vp9SegmentationParams::SEG_LVL_ALT_Q)) {
    int16_t feature_data =
        segmentation.FeatureData(segid, Vp9SegmentationParams::SEG_LVL_ALT_Q);
    int64_t q_index = segmentation.abs_or_delta_update
                          ? feature_data
                          : quant.base_q_idx + feature_data;
    return ClampQ(q_index);
  }
  return quant.base_q_idx;
}

// 8.6.1 Dequantization functions
bool Vp9Parser::SetupSegmentationDequant() {
  const Vp9QuantizationParams& quant = curr_frame_header_.quant_params;
  Vp9SegmentationParams& segmentation = context_.segmentation_;

  if (curr_frame_header_.bit_depth > 10) {
    DLOG(ERROR) << "bit_depth > 10 is not supported yet, kDcQLookup and "
                   "kAcQLookup need to be extended";
    return false;
  }
  const size_t bit_depth_index = (curr_frame_header_.bit_depth == 8) ? 0 : 1;

  if (segmentation.enabled) {
    for (size_t i = 0; i < Vp9SegmentationParams::kNumSegments; ++i) {
      const int64_t q_index = GetQIndex(quant, i);
      segmentation.y_dequant[i][0] =
          kDcQLookup[bit_depth_index][ClampQ(q_index + quant.delta_q_y_dc)];
      segmentation.y_dequant[i][1] =
          kAcQLookup[bit_depth_index][ClampQ(q_index)];
      segmentation.uv_dequant[i][0] =
          kDcQLookup[bit_depth_index][ClampQ(q_index + quant.delta_q_uv_dc)];
      segmentation.uv_dequant[i][1] =
          kAcQLookup[bit_depth_index][ClampQ(q_index + quant.delta_q_uv_ac)];
    }
  } else {
    const int64_t q_index = quant.base_q_idx;
    segmentation.y_dequant[0][0] =
        kDcQLookup[bit_depth_index][ClampQ(q_index + quant.delta_q_y_dc)];
    segmentation.y_dequant[0][1] = kAcQLookup[bit_depth_index][ClampQ(q_index)];
    segmentation.uv_dequant[0][0] =
        kDcQLookup[bit_depth_index][ClampQ(q_index + quant.delta_q_uv_dc)];
    segmentation.uv_dequant[0][1] =
        kAcQLookup[bit_depth_index][ClampQ(q_index + quant.delta_q_uv_ac)];
  }
  return true;
}

// 8.8.1 Loop filter frame init process
void Vp9Parser::SetupLoopFilter() {
  Vp9LoopFilterParams& loop_filter = context_.loop_filter_;
  if (!loop_filter.level)
    return;

  int scale = loop_filter.level < 32 ? 1 : 2;

  for (size_t i = 0; i < Vp9SegmentationParams::kNumSegments; ++i) {
    int level = loop_filter.level;
    const Vp9SegmentationParams& segmentation = context_.segmentation();

    if (segmentation.FeatureEnabled(i, Vp9SegmentationParams::SEG_LVL_ALT_LF)) {
      int feature_data =
          segmentation.FeatureData(i, Vp9SegmentationParams::SEG_LVL_ALT_LF);
      level = ClampLf(segmentation.abs_or_delta_update ? feature_data
                                                       : level + feature_data);
    }

    if (!loop_filter.delta_enabled) {
      memset(loop_filter.lvl[i], level, sizeof(loop_filter.lvl[i]));
    } else {
      loop_filter.lvl[i][Vp9RefType::VP9_FRAME_INTRA][0] = ClampLf(
          level + loop_filter.ref_deltas[Vp9RefType::VP9_FRAME_INTRA] * scale);
      loop_filter.lvl[i][Vp9RefType::VP9_FRAME_INTRA][1] = 0;

      for (size_t type = Vp9RefType::VP9_FRAME_LAST;
           type < Vp9RefType::VP9_FRAME_MAX; ++type) {
        for (size_t mode = 0; mode < Vp9LoopFilterParams::kNumModeDeltas;
             ++mode) {
          loop_filter.lvl[i][type][mode] =
              ClampLf(level + loop_filter.ref_deltas[type] * scale +
                      loop_filter.mode_deltas[mode] * scale);
        }
      }
    }
  }
}

void Vp9Parser::UpdateSlots(Vp9Parser::Context* context) {
  // 8.10 Reference frame update process
  for (size_t i = 0; i < kVp9NumRefFrames; i++) {
    if (curr_frame_header_.RefreshFlag(i)) {
      ReferenceSlot ref_slot;
      ref_slot.initialized = true;

      ref_slot.frame_width = curr_frame_header_.frame_width;
      ref_slot.frame_height = curr_frame_header_.frame_height;
      ref_slot.subsampling_x = curr_frame_header_.subsampling_x;
      ref_slot.subsampling_y = curr_frame_header_.subsampling_y;
      ref_slot.bit_depth = curr_frame_header_.bit_depth;

      ref_slot.profile = curr_frame_header_.profile;
      ref_slot.color_space = curr_frame_header_.color_space;
      context->UpdateRefSlot(i, ref_slot);
    }
  }
}

}  // namespace media
