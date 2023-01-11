// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TS_SECTION_PAT_H_
#define MEDIA_FORMATS_MP2T_TS_SECTION_PAT_H_

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "media/formats/mp2t/ts_section_psi.h"

namespace media {
namespace mp2t {

class TsSectionPat : public TsSectionPsi {
 public:
  // RegisterPmtCB::Run(int program_number, int pmt_pid);
  using RegisterPmtCB = base::RepeatingCallback<void(int, int)>;

  explicit TsSectionPat(RegisterPmtCB register_pmt_cb);

  TsSectionPat(const TsSectionPat&) = delete;
  TsSectionPat& operator=(const TsSectionPat&) = delete;

  ~TsSectionPat() override;

  // TsSectionPsi implementation.
  bool ParsePsiSection(BitReader* bit_reader) override;
  void ResetPsiSection() override;

 private:
  const RegisterPmtCB register_pmt_cb_;

  // Parameters from the PAT.
  int version_number_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_TS_SECTION_PAT_H_
