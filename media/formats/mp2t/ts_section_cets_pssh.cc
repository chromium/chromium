// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/ts_section_cets_pssh.h"

#include "base/check.h"
#include "media/base/bit_reader.h"
#include "media/formats/mp2t/mp2t_common.h"

namespace media {
namespace mp2t {

TsSectionCetsPssh::TsSectionCetsPssh(RegisterPsshBoxesCB register_pssh_boxes_cb)
    : register_pssh_boxes_cb_(std::move(register_pssh_boxes_cb)) {}

TsSectionCetsPssh::~TsSectionCetsPssh() {}

bool TsSectionCetsPssh::Parse(bool payload_unit_start_indicator,
                              const uint8_t* buf,
                              int size) {
  DCHECK(buf);
  // Ignore if doesn't contain PUSI.
  if (!payload_unit_start_indicator)
    return false;
  // TODO(dougsteed). This initial implementation requires the entire CETS-PSSH
  // to fit in one TS packet, so we know that the box length will fit in one
  // byte.
  BitReader bit_reader(buf, size);
  bool md5_flag;
  RCHECK(bit_reader.ReadFlag(&md5_flag) && !md5_flag);
  RCHECK(bit_reader.SkipBits(31));
  int box_length_bits = bit_reader.bits_available();
  std::string pssh;
  RCHECK(bit_reader.ReadString(box_length_bits, &pssh));
  // Now check that the first 4 bytes are of the form {0x00, 0x00, 0x00, X},
  // where X is the box length in bytes.
  RCHECK(pssh[0] == 0x00 && pssh[1] == 0x00 && pssh[2] == 0x00);
  uint8_t declared_box_bytes = static_cast<uint8_t>(pssh[3]);
  RCHECK(declared_box_bytes <= box_length_bits * 8);
  pssh.resize(declared_box_bytes);
  register_pssh_boxes_cb_.Run(std::vector<uint8_t>(pssh.begin(), pssh.end()));
  return true;
}

void TsSectionCetsPssh::Flush() {
  // No pending state.
}

void TsSectionCetsPssh::Reset() {
  // No state to clean up.
}

}  // namespace mp2t
}  // namespace media
