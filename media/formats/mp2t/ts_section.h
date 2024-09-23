// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TS_SECTION_H_
#define MEDIA_FORMATS_MP2T_TS_SECTION_H_

#include <stdint.h>

namespace media {
namespace mp2t {

class TsSection {
 public:
  // From ISO/IEC 13818-1 or ITU H.222 spec: Table 2-3 - PID table.
  enum SpecialPid {
    kPidPat = 0x0,
    kPidCat = 0x1,
    kPidTsdt = 0x2,
    kPidNullPacket = 0x1fff,
    kPidMax = 0x1fff,
  };

  virtual ~TsSection() {}

  // Parse the data bytes of the TS packet.
  // Return true if parsing is successful.
  virtual bool Parse(bool payload_unit_start_indicator,
                     base::span<const uint8_t> buf) = 0;

  // Process bytes that have not been processed yet (pending buffers in the
  // pipe). Flush might thus results in frame emission, as an example.
  virtual void Flush() = 0;

  // Reset the state of the parser to its initial state.
  virtual void Reset() = 0;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_TS_SECTION_H_
