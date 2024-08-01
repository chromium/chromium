// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TS_SECTION_PSI_H_
#define MEDIA_FORMATS_MP2T_TS_SECTION_PSI_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "media/base/byte_queue.h"
#include "media/formats/mp2t/ts_section.h"

namespace media {

class BitReader;

namespace mp2t {

class TsSectionPsi : public TsSection {
 public:
  TsSectionPsi();

  TsSectionPsi(const TsSectionPsi&) = delete;
  TsSectionPsi& operator=(const TsSectionPsi&) = delete;

  ~TsSectionPsi() override;

  // TsSection implementation.
  bool Parse(bool payload_unit_start_indicator,
             base::span<const uint8_t> buf) override;
  void Flush() override;
  void Reset() override;

  // Parse the content of the PSI section.
  virtual bool ParsePsiSection(BitReader* bit_reader) = 0;

  // Reset the state of the PSI section.
  virtual void ResetPsiSection() = 0;

 private:
  void ResetPsiState();

  // Bytes of the current PSI.
  ByteQueue psi_byte_queue_;

  // Do not start parsing before getting a unit start indicator.
  bool wait_for_pusi_;

  // Number of leading bytes to discard (pointer field).
  size_t leading_bytes_to_discard_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_TS_SECTION_PSI_H_
