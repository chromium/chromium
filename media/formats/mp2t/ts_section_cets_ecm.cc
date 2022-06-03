// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/ts_section_cets_ecm.h"

#include "base/check.h"
#include "media/base/bit_reader.h"
#include "media/formats/mp2t/mp2t_common.h"

namespace media {
namespace mp2t {

TsSectionCetsEcm::TsSectionCetsEcm(
    const RegisterNewKeyIdAndIvCB& register_new_key_id_and_iv_cb)
    : register_new_key_id_and_iv_cb_(register_new_key_id_and_iv_cb) {}

TsSectionCetsEcm::~TsSectionCetsEcm() {}

bool TsSectionCetsEcm::Parse(bool payload_unit_start_indicator,
                             const uint8_t* buf,
                             int size) {
  DCHECK(buf);
  BitReader bit_reader(buf, size);
  int num_states;
  bool next_key_id_flag;
  bool no_byte_align;
  int iv_size;
  std::string key_id;
  int transport_scrambling_control;
  int num_au;
  bool key_id_flag;
  int au_byte_offset_size;
  std::string iv;
  // TODO(dougsteed). Currently we allow only a subset of the possible values.
  // When we flesh out this implementation to cover all of ISO/IEC 23001-9 we
  // will need to generalize this.
  RCHECK(bit_reader.ReadBits(2, &num_states));
  RCHECK(num_states == 1);
  RCHECK(bit_reader.ReadFlag(&next_key_id_flag) && !next_key_id_flag);
  // TODO(dougsteed). The standard (ISO/IEC 23001-9:2014) reserves 3 bits,
  // whereas it likely was intended to be 5 bits to follow the usual practice of
  // syncing to a byte boundary for the byte oriented fields that follow.
  // For now, we plan to use it with byte alignment for convenience. Rather than
  // just having an unadvertized deviation from the standard, I have repurposed
  // the first reserved bit as a flag. This approach gives flexibility for the
  // future if the standard is fixed or comes into wide use in its present form.
  RCHECK(bit_reader.ReadFlag(&no_byte_align));
  if (no_byte_align)
    RCHECK(bit_reader.SkipBits(2));
  else
    RCHECK(bit_reader.SkipBits(4));
  RCHECK(bit_reader.ReadBits(8, &iv_size));
  RCHECK(iv_size == 16);
  RCHECK(bit_reader.ReadString(128, &key_id));
  RCHECK(bit_reader.ReadBits(2, &transport_scrambling_control));
  RCHECK(transport_scrambling_control == 0);
  RCHECK(bit_reader.ReadBits(6, &num_au));
  RCHECK(num_au == 1);
  RCHECK(bit_reader.ReadFlag(&key_id_flag) && !key_id_flag);
  RCHECK(bit_reader.SkipBits(3));
  RCHECK(bit_reader.ReadBits(4, &au_byte_offset_size));
  RCHECK(au_byte_offset_size == 0);
  RCHECK(bit_reader.ReadString(128, &iv));
  // The CETS-ECM is supposed to  use adaptation field stuffing to fill the TS
  // packet, so there should be no data left to read.
  RCHECK(bit_reader.bits_available() == 0);
  register_new_key_id_and_iv_cb_.Run(key_id, iv);
  return true;
}

void TsSectionCetsEcm::Flush() {
  // No pending state.
}

void TsSectionCetsEcm::Reset() {
  // No state to clean up.
}

}  // namespace mp2t
}  // namespace media
