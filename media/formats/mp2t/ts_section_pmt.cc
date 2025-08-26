// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/ts_section_pmt.h"

#include <map>

#include "base/check.h"
#include "base/numerics/checked_math.h"
#include "media/base/bit_reader.h"
#include "media/formats/mp2t/mp2t_common.h"

namespace media {
namespace mp2t {

TsSectionPmt::TsSectionPmt(RegisterPesCB register_pes_cb)
    : register_pes_cb_(std::move(register_pes_cb)) {}

TsSectionPmt::~TsSectionPmt() {
}

bool TsSectionPmt::ParsePsiSection(BitReader* bit_reader) {
  // Read up to |last_section_number|.
  uint8_t table_id;
  uint8_t section_syntax_indicator;
  uint8_t dummy_zero;
  uint8_t reserved;
  size_t section_length;
  uint16_t program_number;
  uint8_t version_number;
  uint8_t current_next_indicator;
  uint8_t section_number;
  uint8_t last_section_number;
  RCHECK(bit_reader->ReadBits(8, &table_id));
  RCHECK(bit_reader->ReadBits(1, &section_syntax_indicator));
  RCHECK(bit_reader->ReadBits(1, &dummy_zero));
  RCHECK(bit_reader->ReadBits(2, &reserved));
  RCHECK(bit_reader->ReadBits(12, &section_length));
  const size_t section_start_marker = bit_reader->bits_available() / 8;

  RCHECK(bit_reader->ReadBits(16, &program_number));
  RCHECK(bit_reader->ReadBits(2, &reserved));
  RCHECK(bit_reader->ReadBits(5, &version_number));
  RCHECK(bit_reader->ReadBits(1, &current_next_indicator));
  RCHECK(bit_reader->ReadBits(8, &section_number));
  RCHECK(bit_reader->ReadBits(8, &last_section_number));

  // Perform a few verifications:
  // - table ID should be 2 for a PMT.
  // - section_syntax_indicator should be one.
  // - section length should not exceed 1021.
  RCHECK(table_id == 0x2);
  RCHECK(section_syntax_indicator);
  RCHECK(!dummy_zero);
  RCHECK(section_length <= 1021);
  RCHECK(section_number == 0);
  RCHECK(last_section_number == 0);

  // TODO(damienv):
  // Verify that there is no mismatch between the program number
  // and the program number that was provided in a PAT for the current PMT.

  // Read the end of the fixed length section.
  uint16_t pcr_pid;
  uint16_t program_info_length;
  RCHECK(bit_reader->ReadBits(3, &reserved));
  RCHECK(bit_reader->ReadBits(13, &pcr_pid));
  RCHECK(bit_reader->ReadBits(4, &reserved));
  RCHECK(bit_reader->ReadBits(12, &program_info_length));
  RCHECK(program_info_length < 1024);

  // Read the program info descriptor.
  // TODO(damienv): check whether any of the descriptors could be useful.
  // Defined in section 2.6 of ISO-13818.
  RCHECK(bit_reader->SkipBits(8 * program_info_length));

  // Read the ES description table.
  // The end of the PID map if 4 bytes away from the end of the section
  // (4 bytes = size of the CRC).
  const size_t pid_map_end_marker =
      base::CheckAdd(base::CheckSub(section_start_marker, section_length), 4u)
          .ValueOrDie();
  using PidMapValue = std::pair<uint8_t, Descriptors>;
  std::map<uint16_t, PidMapValue> pid_map;
  while (bit_reader->bits_available() > 8 * pid_map_end_marker) {
    uint8_t stream_type;
    uint16_t pid_es;
    uint16_t es_info_length;
    RCHECK(bit_reader->ReadBits(8, &stream_type));
    RCHECK(bit_reader->ReadBits(3, &reserved));
    RCHECK(bit_reader->ReadBits(13, &pid_es));
    RCHECK(bit_reader->ReadBits(4, &reserved));
    RCHECK(bit_reader->ReadBits(12, &es_info_length));

    Descriptors descriptors;
    RCHECK(descriptors.Read(bit_reader, es_info_length));

    // Do not register the PID right away.
    // Wait for the end of the section to be fully parsed
    // to make sure there is no error.
    PidMapValue stream_info(stream_type, descriptors);
    pid_map.insert(std::make_pair(pid_es, stream_info));
  }

  // Read the CRC.
  size_t crc32;
  RCHECK(bit_reader->ReadBits(32, &crc32));

  // Once the PMT has been proved to be correct, register the PIDs.
  for (const auto& [pid_es, stream_info] : pid_map) {
    const auto& [stream_type, descriptors] = stream_info;
    register_pes_cb_.Run(pid_es, stream_type, descriptors);
  }

  return true;
}

void TsSectionPmt::ResetPsiSection() {
}

}  // namespace mp2t
}  // namespace media
