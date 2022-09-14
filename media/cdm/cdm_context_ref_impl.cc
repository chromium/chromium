// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_context_ref_impl.h"

#include <ostream>

#include "base/check_op.h"
#include "media/base/content_decryption_module.h"

namespace media {

CdmContextRefImpl::CdmContextRefImpl(scoped_refptr<ContentDecryptionModule> cdm)
    : cdm_(std::move(cdm)) {
  DCHECK(cdm_);
}

CdmContextRefImpl::~CdmContextRefImpl() {
  // This will release |cdm_|.
}

CdmContext* CdmContextRefImpl::GetCdmContext() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return cdm_->GetCdmContext();
}

}  // namespace media
