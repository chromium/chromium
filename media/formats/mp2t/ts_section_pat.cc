// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/ts_section_pat.h"

#include <vector>

#include "base/logging.h"
#include "media/base/bit_reader.h"
#include "media/formats/mp2t/mp2t_common.h"

namespace media {
namespace mp2t {

TsSectionPat::TsSectionPat(RegisterPmtCB register_pmt_cb)
    : register_pmt_cb_(std::move(register_pmt_cb)), version_number_(-1) {}

TsSectionPat::~TsSectionPat() {
}

bool TsSectionPat::ParsePsiSection(BitReader* bit_reader) {
  // Read the fixed section length.
  int table_id;
  int section_syntax_indicator;
  int dummy_zero;
  int reserved;
  int section_length;
  int transport_stream_id;
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
  RCHECK(bit_reader->ReadBits(16, &transport_stream_id));
  RCHECK(bit_reader->ReadBits(2, &reserved));
  RCHECK(bit_reader->ReadBits(5, &version_number));
  RCHECK(bit_reader->ReadBits(1, &current_next_indicator));
  RCHECK(bit_reader->ReadBits(8, &section_number));
  RCHECK(bit_reader->ReadBits(8, &last_section_number));
  section_length -= 5;

  // Perform a few verifications:
  // - Table ID should be 0 for a PAT.
  // - section_syntax_indicator should be one.
  // - section length should not exceed 1021
  RCHECK(table_id == 0x0);
  RCHECK(section_syntax_indicator);
  RCHECK(!dummy_zero);

  // Both the program table and the CRC have a size multiple of 4.
  // Note for pmt_pid_count: minus 4 to account for the CRC.
  RCHECK(section_length > 0);
  RCHECK((section_length % 4) == 0);
  int pmt_pid_count = (section_length - 4) / 4;

  // Read the variable length section: program table & crc.
  std::vector<int> program_number_array(pmt_pid_count);
  std::vector<int> pmt_pid_array(pmt_pid_count);
  for (int k = 0; k < pmt_pid_count; k++) {
    RCHECK(bit_reader->ReadBits(16, &program_number_array[k]));
    RCHECK(bit_reader->ReadBits(3, &reserved));
    RCHECK(bit_reader->ReadBits(13, &pmt_pid_array[k]));
  }
  int crc32;
  RCHECK(bit_reader->ReadBits(32, &crc32));

  // Just ignore the PAT if not applicable yet.
  if (!current_next_indicator) {
    DVLOG(1) << "Not supported: received a PAT not applicable yet";
    return true;
  }

  // Ignore the program table if it hasn't changed.
  if (version_number == version_number_)
    return true;

  // Both the MSE and the HLS spec specifies that TS streams should convey
  // exactly one program.
  if (pmt_pid_count > 1) {
    DVLOG(1) << "Multiple programs detected in the Mpeg2 TS stream";
    return false;
  }

  // Can now register the PMT.
#if !defined(NDEBUG)
  int expected_version_number = version_number;
  if (version_number_ >= 0)
    expected_version_number = (version_number_ + 1) % 32;
  DVLOG_IF(1, version_number != expected_version_number)
      << "Unexpected version number: "
      << version_number << " vs " << version_number_;
#endif
  for (int k = 0; k < pmt_pid_count; k++) {
    if (program_number_array[k] != 0) {
      // Program numbers different from 0 correspond to PMT.
      register_pmt_cb_.Run(program_number_array[k], pmt_pid_array[k]);
      // Even if there are multiple programs, only one can be supported now.
      // HLS: "Transport Stream segments MUST contain a single MPEG-2 Program."
      break;
    }
  }
  version_number_ = version_number;

  return true;
}

void TsSectionPat::ResetPsiSection() {
  version_number_ = -1;
}

}  // namespace mp2t
}  // namespace media
