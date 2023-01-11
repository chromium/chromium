// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TS_SECTION_CAT_H_
#define MEDIA_FORMATS_MP2T_TS_SECTION_CAT_H_

#include "base/functional/callback.h"
#include "media/base/encryption_scheme.h"
#include "media/formats/mp2t/ts_section_psi.h"

namespace media {
namespace mp2t {

class TsSectionCat : public TsSectionPsi {
 public:
  // RegisterCencPidsCB::Run(int ca_pid, int pssh_pid);
  using RegisterCencPidsCB = base::RepeatingCallback<void(int, int)>;
  // RegisterEncryptionScheme::Run(EncryptionScheme scheme);
  using RegisterEncryptionSchemeCB =
      base::RepeatingCallback<void(EncryptionScheme)>;
  TsSectionCat(const RegisterCencPidsCB& register_cenc_ids_cb,
               const RegisterEncryptionSchemeCB& register_encryption_scheme_cb);

  TsSectionCat(const TsSectionCat&) = delete;
  TsSectionCat& operator=(const TsSectionCat&) = delete;

  ~TsSectionCat() override;

  // TsSectionPsi implementation.
  bool ParsePsiSection(BitReader* bit_reader) override;
  void ResetPsiSection() override;

 private:
  RegisterCencPidsCB register_cenc_ids_cb_;
  RegisterEncryptionSchemeCB register_encryption_scheme_cb_;

  // Parameters from the CAT.
  int version_number_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_TS_SECTION_CAT_H_
