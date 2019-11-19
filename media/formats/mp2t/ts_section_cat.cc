// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/ts_section_cat.h"

#include <vector>

#include "base/logging.h"
#include "media/base/bit_reader.h"
#include "media/formats/mp2t/descriptors.h"
#include "media/formats/mp2t/mp2t_common.h"

namespace media {
namespace mp2t {

TsSectionCat::TsSectionCat(
    const RegisterCencPidsCb& register_cenc_ids_cb,
    const RegisterEncryptionSchemeCb& register_encryption_scheme_cb)
    : register_cenc_ids_cb_(register_cenc_ids_cb),
      register_encryption_scheme_cb_(register_encryption_scheme_cb),
      version_number_(-1) {}

TsSectionCat::~TsSectionCat() {}

bool TsSectionCat::ParsePsiSection(BitReader* bit_reader) {
  DCHECK(bit_reader);
  // Read the fixed section length.
  int table_id;
  int section_syntax_indicator;
  int dummy_zero;
  int reserved;
  int section_length;
  int version_number;
  int current_next_indicator;
  int section_number;
  int last_section_number;
  RCHECK(bit_reader->ReadBits(8, &table_id));
  RCHECK(bit_reader->ReadBits(1, &section_syntax_indicator));
  RCHECK(bit_reader->ReadBits(1, &dummy_zero));
  RCHECK(bit_reader->ReadBits(2, &reserved));
  RCHECK(bit_reader->ReadBits(12, &section_length));
  RCHECK(section_length >= 5);
  RCHECK(section_length <= 1021);
  RCHECK(bit_reader->ReadBits(18, &reserved));
  RCHECK(bit_reader->ReadBits(5, &version_number));
  RCHECK(bit_reader->ReadBits(1, &current_next_indicator));
  RCHECK(bit_reader->ReadBits(8, &section_number));
  RCHECK(bit_reader->ReadBits(8, &last_section_number));
  section_length -= 5;

  // Perform a few more verifications, including:
  // - Table ID should be 1 for a CAT.
  // - section_syntax_indicator should be one.
  RCHECK(table_id == 0x1);
  RCHECK(section_syntax_indicator);
  RCHECK(!dummy_zero);

  Descriptors descriptors;
  int ca_pid, pssh_pid;
  EncryptionScheme scheme;
  RCHECK(descriptors.Read(bit_reader, section_length - 4));
  RCHECK(descriptors.HasCADescriptorCenc(&ca_pid, &pssh_pid, &scheme));
  int crc32;
  RCHECK(bit_reader->ReadBits(32, &crc32));

  // Just ignore the CAT if not applicable yet.
  if (!current_next_indicator) {
    DVLOG(1) << "Not supported: received a CAT not applicable yet";
    return true;
  }

  // Ignore the CAT if it hasn't changed.
  if (version_number == version_number_)
    return true;

  // Can now register the PIDs and scheme.
  register_cenc_ids_cb_.Run(ca_pid, pssh_pid);
  register_encryption_scheme_cb_.Run(scheme);

  version_number_ = version_number;

  return true;
}

void TsSectionCat::ResetPsiSection() {
  version_number_ = -1;
}

}  // namespace mp2t
}  // namespace media
