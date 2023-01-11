// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TS_SECTION_PMT_H_
#define MEDIA_FORMATS_MP2T_TS_SECTION_PMT_H_

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "media/formats/mp2t/descriptors.h"
#include "media/formats/mp2t/ts_section_psi.h"

namespace media {
namespace mp2t {

class TsSectionPmt : public TsSectionPsi {
 public:
  // |stream_type| is defined in "Table 2-34 – Stream type assignments" in H.222
  // TODO(damienv): add the program number.
  using RegisterPesCB = base::RepeatingCallback<
      void(int pes_pid, int stream_type, const Descriptors& descriptors)>;

  explicit TsSectionPmt(RegisterPesCB register_pes_cb);

  TsSectionPmt(const TsSectionPmt&) = delete;
  TsSectionPmt& operator=(const TsSectionPmt&) = delete;

  ~TsSectionPmt() override;

  // Mpeg2TsPsiParser implementation.
  bool ParsePsiSection(BitReader* bit_reader) override;
  void ResetPsiSection() override;

 private:
  const RegisterPesCB register_pes_cb_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_TS_SECTION_PMT_H_
