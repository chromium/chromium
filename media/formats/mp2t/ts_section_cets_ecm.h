// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TS_SECTION_CETS_ECM_H_
#define MEDIA_FORMATS_MP2T_TS_SECTION_CETS_ECM_H_

#include <stdint.h>

#include <string>

#include "base/functional/callback.h"
#include "media/formats/mp2t/ts_section.h"

namespace media {

namespace mp2t {

class TsSectionCetsEcm : public TsSection {
 public:
  // RegisterNewKeyIdAndIvCB() may be called multiple times. From
  // ISO/IEC 23001-9:2016, section 7.2: "Key/IV information for every
  // encrypted PID should be carried in a separate ECM PID." So there may be
  // ECM's for each audio and video stream (and more if key rotation is used).
  using RegisterNewKeyIdAndIvCB =
      base::RepeatingCallback<void(const std::string& key_id,
                                   const std::string& iv)>;

  explicit TsSectionCetsEcm(
      const RegisterNewKeyIdAndIvCB& register_new_key_id_and_iv_cb);

  TsSectionCetsEcm(const TsSectionCetsEcm&) = delete;
  TsSectionCetsEcm& operator=(const TsSectionCetsEcm&) = delete;

  ~TsSectionCetsEcm() override;

  // TsSection implementation.
  bool Parse(bool payload_unit_start_indicator,
             base::span<const uint8_t> buf) override;
  void Flush() override;
  void Reset() override;

 private:
  RegisterNewKeyIdAndIvCB register_new_key_id_and_iv_cb_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_TS_SECTION_CETS_ECM_H_
