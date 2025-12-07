// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/ts_section_psi.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/bit_reader.h"
#include "media/base/byte_queue.h"
#include "media/formats/mp2t/mp2t_common.h"

static bool IsCrcValid(base::span<const uint8_t> buf) {
  uint32_t crc = 0xffffffffu;
  constexpr uint32_t kCrcPoly = 0x4c11db7;
  std::ranges::for_each(buf, [&crc](uint32_t data_msb_aligned) {
    int nbits = 8;
    data_msb_aligned <<= (32 - nbits);

    while (nbits > 0) {
      if ((data_msb_aligned ^ crc) & 0x80000000) {
        crc <<= 1;
        crc ^= kCrcPoly;
      } else {
        crc <<= 1;
      }

      data_msb_aligned <<= 1;
      nbits--;
    }
  });

  return (crc == 0);
}

namespace media {
namespace mp2t {

TsSectionPsi::TsSectionPsi()
    : wait_for_pusi_(true),
      leading_bytes_to_discard_(0) {
}

TsSectionPsi::~TsSectionPsi() {
}

bool TsSectionPsi::Parse(bool payload_unit_start_indicator,
                         base::span<const uint8_t> buf) {
  // Ignore partial PSI.
  if (wait_for_pusi_ && !payload_unit_start_indicator)
    return true;

  if (payload_unit_start_indicator) {
    // Reset the state of the PSI section.
    ResetPsiState();

    // Update the state.
    wait_for_pusi_ = false;
    RCHECK(!buf.empty());  // A payload unit must start immediately.
    int pointer_field = buf[0];
    leading_bytes_to_discard_ = pointer_field;
    buf = buf.subspan<1>();
  }

  // Discard some leading bytes if needed.
  if (leading_bytes_to_discard_ > 0) {
    size_t nbytes_to_discard = std::min(leading_bytes_to_discard_, buf.size());
    leading_bytes_to_discard_ -= nbytes_to_discard;
    buf = buf.subspan(nbytes_to_discard);
  }
  if (buf.empty()) {
    return true;
  }

  // Add the data to the parser state.
  RCHECK(psi_byte_queue_.Push(buf));
  base::span<const uint8_t> psi = psi_byte_queue_.Data();

  // Check whether we have enough data to start parsing.
  if (psi.size() < 3) {
    return true;
  }
  size_t section_length =
      ((static_cast<size_t>(psi[1]) << 8) | (static_cast<size_t>(psi[2]))) &
      0xfff;
  if (section_length >= 1021) {
    return false;
  }
  size_t psi_length = section_length + 3;
  if (psi.size() < psi_length) {
    // Don't throw an error when there is not enough data,
    // just wait for more data to come.
    return true;
  }

  // There should not be any trailing bytes after a PMT.
  // Instead, the pointer field should be used to stuff bytes.
  DVLOG_IF(1, psi.size() > psi_length)
      << "Trailing bytes after a PSI section: " << psi_length << " vs "
      << psi.size();

  if (!IsCrcValid(psi.first(psi_length))) {
    DVLOG(1) << "Invalid PSI section crc checksum.";
  }

  // Parse the PSI section.
  BitReader bit_reader(psi);
  bool status = ParsePsiSection(&bit_reader);
  if (status)
    ResetPsiState();

  return status;
}

void TsSectionPsi::Flush() {
}

void TsSectionPsi::Reset() {
  ResetPsiSection();
  ResetPsiState();
}

void TsSectionPsi::ResetPsiState() {
  wait_for_pusi_ = true;
  psi_byte_queue_.Reset();
  leading_bytes_to_discard_ = 0;
}

}  // namespace mp2t
}  // namespace media
