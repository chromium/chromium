// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/parsers/vp9_compressed_header_parser.h"

#include "base/logging.h"

namespace media {

namespace {

// 6.3.5 Inv remap prob syntax, inv_remap_prob().
Vp9Prob InvRemapProb(uint8_t delta_prob, uint8_t prob) {
  static const uint8_t inv_map_table[kVp9MaxProb] = {
      7,   20,  33,  46,  59,  72,  85,  98,  111, 124, 137, 150, 163, 176,
      189, 202, 215, 228, 241, 254, 1,   2,   3,   4,   5,   6,   8,   9,
      10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  21,  22,  23,  24,
      25,  26,  27,  28,  29,  30,  31,  32,  34,  35,  36,  37,  38,  39,
      40,  41,  42,  43,  44,  45,  47,  48,  49,  50,  51,  52,  53,  54,
      55,  56,  57,  58,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,
      70,  71,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,
      86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  99,  100,
      101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 112, 113, 114, 115,
      116, 117, 118, 119, 120, 121, 122, 123, 125, 126, 127, 128, 129, 130,
      131, 132, 133, 134, 135, 136, 138, 139, 140, 141, 142, 143, 144, 145,
      146, 147, 148, 149, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
      161, 162, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
      177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 190, 191,
      192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 203, 204, 205, 206,
      207, 208, 209, 210, 211, 212, 213, 214, 216, 217, 218, 219, 220, 221,
      222, 223, 224, 225, 226, 227, 229, 230, 231, 232, 233, 234, 235, 236,
      237, 238, 239, 240, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251,
      252, 253, 253};
  uint8_t v = delta_prob;
  DCHECK_LT(v, std::size(inv_map_table));
  return inv_map_table[v];
}

}  // namespace

Vp9CompressedHeaderParser::Vp9CompressedHeaderParser() = default;

// 6.3 Compressed header syntax
bool Vp9CompressedHeaderParser::ParseNoContext(const uint8_t* stream,
                                               off_t frame_size,
                                               Vp9FrameHeader* fhdr) {
  memset(&fhdr->frame_context, 0, sizeof(fhdr->frame_context));
  return ParseInternal(stream, frame_size, fhdr);
}

// 6.3.1 Tx mode syntax
void Vp9CompressedHeaderParser::ReadTxMode(Vp9FrameHeader* fhdr) {
  int tx_mode;
  if (fhdr->quant_params.IsLossless()) {
    tx_mode = Vp9CompressedHeader::ONLY_4X4;
  } else {
    tx_mode = reader_.ReadLiteral(2);
    if (tx_mode == Vp9CompressedHeader::ALLOW_32X32)
      tx_mode += reader_.ReadLiteral(1);
  }
  fhdr->compressed_header.tx_mode =
      static_cast<Vp9CompressedHeader::Vp9TxMode>(tx_mode);
}

// 6.3.4 Decode term subexp syntax
uint8_t Vp9CompressedHeaderParser::DecodeTermSubexp() {
  if (reader_.ReadLiteral(1) == 0)
    return reader_.ReadLiteral(4);
  if (reader_.ReadLiteral(1) == 0)
    return reader_.ReadLiteral(4) + 16;
  if (reader_.ReadLiteral(1) == 0)
    return reader_.ReadLiteral(5) + 32;
  uint8_t v = reader_.ReadLiteral(7);
  if (v < 65)
    return v + 64;
  return (v << 1) - 1 + reader_.ReadLiteral(1);
}

// 6.3.3 Diff update prob syntax
void Vp9CompressedHeaderParser::DiffUpdateProb(Vp9Prob* prob) {
  const Vp9Prob kUpdateProb = 252;
  const bool must_update_probabilities = reader_.ReadBool(kUpdateProb);

  if (!must_update_probabilities)
    return;

  uint8_t delta_prob = DecodeTermSubexp();
  *prob = InvRemapProb(delta_prob, *prob);
}

// Helper function to DiffUpdateProb an array of probs.
template <int N>
void Vp9CompressedHeaderParser::DiffUpdateProbArray(Vp9Prob (&prob_array)[N]) {
  for (auto& x : prob_array) {
    DiffUpdateProb(&x);
  }
}

// 6.3.2 Tx mode probs syntax
void Vp9CompressedHeaderParser::ReadTxModeProbs(
    Vp9FrameContext* frame_context) {
  for (auto& a : frame_context->tx_probs_8x8) {
    DiffUpdateProbArray(a);
  }
  for (auto& a : frame_context->tx_probs_16x16) {
    DiffUpdateProbArray(a);
  }
  for (auto& a : frame_context->tx_probs_32x32) {
    DiffUpdateProbArray(a);
  }
}

// 6.3.7 Coef probs syntax
void Vp9CompressedHeaderParser::ReadCoefProbs(Vp9FrameHeader* fhdr) {
  const int tx_mode_to_biggest_tx_size[Vp9CompressedHeader::TX_MODES] = {
      0, 1, 2, 3, 3,
  };
  const int max_tx_size =
      tx_mode_to_biggest_tx_size[fhdr->compressed_header.tx_mode];
  for (int tx_size = 0; tx_size <= max_tx_size; tx_size++) {
    if (reader_.ReadLiteral(1) == 0)
      continue;

    for (auto& ai : fhdr->frame_context.coef_probs[tx_size]) {
      for (auto& aj : ai) {
        for (auto& ak : aj) {
          int max_l = (+ak == +aj[0]) ? 3 : 6;
          for (int l = 0; l < max_l; l++) {
            DiffUpdateProbArray(ak[l]);
          }
        }
      }
    }
  }
}

// 6.3.8 Skip probs syntax
void Vp9CompressedHeaderParser::ReadSkipProb(Vp9FrameContext* frame_context) {
  DiffUpdateProbArray(frame_context->skip_prob);
}

// 6.3.9 Inter mode probs syntax
void Vp9CompressedHeaderParser::ReadInterModeProbs(
    Vp9FrameContext* frame_context) {
  for (auto& a : frame_context->inter_mode_probs)
    DiffUpdateProbArray(a);
}

// 6.3.10 Interp filter probs syntax
void Vp9CompressedHeaderParser::ReadInterpFilterProbs(
    Vp9FrameContext* frame_context) {
  for (auto& a : frame_context->interp_filter_probs)
    DiffUpdateProbArray(a);
}

// 6.3.11 Intra inter probs syntax
void Vp9CompressedHeaderParser::ReadIsInterProbs(
    Vp9FrameContext* frame_context) {
  DiffUpdateProbArray(frame_context->is_inter_prob);
}

// 6.3.12 Frame reference mode syntax
void Vp9CompressedHeaderParser::ReadFrameReferenceMode(Vp9FrameHeader* fhdr) {
  bool compound_reference_allowed = false;
  for (int i = VP9_FRAME_LAST + 1; i < VP9_FRAME_MAX; i++)
    if (fhdr->ref_frame_sign_bias[i] != fhdr->ref_frame_sign_bias[1])
      compound_reference_allowed = true;

  if (compound_reference_allowed && reader_.ReadLiteral(1)) {
    fhdr->compressed_header.reference_mode =
        reader_.ReadLiteral(1) ? REFERENCE_MODE_SELECT : COMPOUND_REFERENCE;
  } else {
    fhdr->compressed_header.reference_mode = SINGLE_REFERENCE;
  }
}

// 6.3.13 Frame reference mode probs syntax
void Vp9CompressedHeaderParser::ReadFrameReferenceModeProbs(
    Vp9FrameHeader* fhdr) {
  Vp9FrameContext* frame_context = &fhdr->frame_context;
  if (fhdr->compressed_header.reference_mode == REFERENCE_MODE_SELECT)
    DiffUpdateProbArray(frame_context->comp_mode_prob);

  if (fhdr->compressed_header.reference_mode != COMPOUND_REFERENCE)
    for (auto& a : frame_context->single_ref_prob)
      DiffUpdateProbArray(a);

  if (fhdr->compressed_header.reference_mode != SINGLE_REFERENCE)
    DiffUpdateProbArray(frame_context->comp_ref_prob);
}

// 6.3.14 Y mode probs syntax
void Vp9CompressedHeaderParser::ReadYModeProbs(Vp9FrameContext* frame_context) {
  for (auto& a : frame_context->y_mode_probs)
    DiffUpdateProbArray(a);
}

// 6.3.15 Partition probs syntax
void Vp9CompressedHeaderParser::ReadPartitionProbs(
    Vp9FrameContext* frame_context) {
  for (auto& a : frame_context->partition_probs)
    DiffUpdateProbArray(a);
}

// 6.3.16 MV probs syntax
void Vp9CompressedHeaderParser::ReadMvProbs(bool allow_high_precision_mv,
                                            Vp9FrameContext* frame_context) {
  UpdateMvProbArray(frame_context->mv_joint_probs);

  for (int i = 0; i < 2; i++) {
    UpdateMvProb(&frame_context->mv_sign_prob[i]);
    UpdateMvProbArray(frame_context->mv_class_probs[i]);
    UpdateMvProb(&frame_context->mv_class0_bit_prob[i]);
    UpdateMvProbArray(frame_context->mv_bits_prob[i]);
  }

  for (int i = 0; i < 2; i++) {
    for (auto& a : frame_context->mv_class0_fr_probs[i])
      UpdateMvProbArray(a);
    UpdateMvProbArray(frame_context->mv_fr_probs[i]);
  }

  if (allow_high_precision_mv) {
    for (int i = 0; i < 2; i++) {
      UpdateMvProb(&frame_context->mv_class0_hp_prob[i]);
      UpdateMvProb(&frame_context->mv_hp_prob[i]);
    }
  }
}

// 6.3.17 Update mv prob syntax
void Vp9CompressedHeaderParser::UpdateMvProb(Vp9Prob* prob) {
  if (reader_.ReadBool(252))
    *prob = reader_.ReadLiteral(7) << 1 | 1;
}

// Helper function to UpdateMvProb an array of probs.
template <int N>
void Vp9CompressedHeaderParser::UpdateMvProbArray(Vp9Prob (&prob_array)[N]) {
  for (auto& x : prob_array) {
    UpdateMvProb(&x);
  }
}

// 6.3 Compressed header syntax
bool Vp9CompressedHeaderParser::ParseInternal(const uint8_t* stream,
                                              off_t frame_size,
                                              Vp9FrameHeader* fhdr) {
  DVLOG(2) << "Vp9CompressedHeaderParser::Parse";
  if (!reader_.Initialize(stream, frame_size))
    return false;

  ReadTxMode(fhdr);
  if (fhdr->compressed_header.tx_mode == Vp9CompressedHeader::TX_MODE_SELECT)
    ReadTxModeProbs(&fhdr->frame_context);

  ReadCoefProbs(fhdr);
  ReadSkipProb(&fhdr->frame_context);

  if (!fhdr->IsIntra()) {
    ReadInterModeProbs(&fhdr->frame_context);
    if (fhdr->interpolation_filter == SWITCHABLE)
      ReadInterpFilterProbs(&fhdr->frame_context);
    ReadIsInterProbs(&fhdr->frame_context);
    ReadFrameReferenceMode(fhdr);
    ReadFrameReferenceModeProbs(fhdr);
    ReadYModeProbs(&fhdr->frame_context);
    ReadPartitionProbs(&fhdr->frame_context);
    ReadMvProbs(fhdr->allow_high_precision_mv, &fhdr->frame_context);
  }

  if (!reader_.IsValid()) {
    DVLOG(1) << "parser reads beyond the end of buffer";
    return false;
  }
  if (!reader_.ConsumePaddingBits()) {
    DVLOG(1) << "padding bits are not zero";
    return false;
  }
  return true;
}

}  // namespace media
