// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TS_SECTION_CETS_PSSH_H_
#define MEDIA_FORMATS_MP2T_TS_SECTION_CETS_PSSH_H_

#include <stdint.h>
#include <vector>

#include "base/functional/callback.h"
#include "media/base/byte_queue.h"
#include "media/formats/mp2t/ts_section.h"

namespace media {
namespace mp2t {

class TsSectionCetsPssh : public TsSection {
 public:
  using RegisterPsshBoxesCB =
      base::RepeatingCallback<void(const std::vector<uint8_t>&)>;

  explicit TsSectionCetsPssh(RegisterPsshBoxesCB register_pssh_boxes_cb);

  TsSectionCetsPssh(const TsSectionCetsPssh&) = delete;
  TsSectionCetsPssh& operator=(const TsSectionCetsPssh&) = delete;

  ~TsSectionCetsPssh() override;

  // TsSection implementation.
  bool Parse(bool payload_unit_start_indicator,
             base::span<const uint8_t> buf) override;
  void Flush() override;
  void Reset() override;

 private:
  const RegisterPsshBoxesCB register_pssh_boxes_cb_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_TS_SECTION_CETS_PSSH_H_
