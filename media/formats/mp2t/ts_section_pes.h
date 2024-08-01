// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TS_SECTION_PES_H_
#define MEDIA_FORMATS_MP2T_TS_SECTION_PES_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "media/base/byte_queue.h"
#include "media/formats/mp2t/ts_section.h"

namespace media {
namespace mp2t {

class EsParser;
class TimestampUnroller;

class TsSectionPes : public TsSection {
 public:
  TsSectionPes(std::unique_ptr<EsParser> es_parser,
               TimestampUnroller* timestamp_unroller);

  TsSectionPes(const TsSectionPes&) = delete;
  TsSectionPes& operator=(const TsSectionPes&) = delete;

  ~TsSectionPes() override;

  // TsSection implementation.
  bool Parse(bool payload_unit_start_indicator,
             base::span<const uint8_t> buf) override;
  void Flush() override;
  void Reset() override;

 private:
  // Emit a reassembled PES packet.
  // Return true if successful.
  // |emit_for_unknown_size| is used to force emission for PES packets
  // whose size is unknown.
  bool Emit(bool emit_for_unknown_size);

  // Parse a PES packet, return true if successful.
  bool ParseInternal(const uint8_t* raw_pes, int raw_pes_size);

  void ResetPesState();

  // Bytes of the current PES.
  ByteQueue pes_byte_queue_;

  // ES parser.
  std::unique_ptr<EsParser> es_parser_;

  // Do not start parsing before getting a unit start indicator.
  bool wait_for_pusi_;

  // Used to unroll PTS and DTS.
  const raw_ptr<TimestampUnroller> timestamp_unroller_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_TS_SECTION_PES_H_
