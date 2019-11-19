// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TS_SECTION_CAT_H_
#define MEDIA_FORMATS_MP2T_TS_SECTION_CAT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/encryption_scheme.h"
#include "media/formats/mp2t/ts_section_psi.h"

namespace media {
namespace mp2t {

class TsSectionCat : public TsSectionPsi {
 public:
  // RegisterCencPidsCb::Run(int ca_pid, int pssh_pid);
  using RegisterCencPidsCb = base::RepeatingCallback<void(int, int)>;
  // RegisterEncryptionScheme::Run(EncryptionScheme scheme);
  using RegisterEncryptionSchemeCb =
      base::RepeatingCallback<void(EncryptionScheme)>;
  TsSectionCat(const RegisterCencPidsCb& register_cenc_ids_cb,
               const RegisterEncryptionSchemeCb& register_encryption_scheme_cb);
  ~TsSectionCat() override;

  // TsSectionPsi implementation.
  bool ParsePsiSection(BitReader* bit_reader) override;
  void ResetPsiSection() override;

 private:
  RegisterCencPidsCb register_cenc_ids_cb_;
  RegisterEncryptionSchemeCb register_encryption_scheme_cb_;

  // Parameters from the CAT.
  int version_number_;

  DISALLOW_COPY_AND_ASSIGN(TsSectionCat);
};

}  // namespace mp2t
}  // namespace media

#endif
