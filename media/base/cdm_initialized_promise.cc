// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_initialized_promise.h"

namespace media {

CdmInitializedPromise::CdmInitializedPromise(
    CdmCreatedCB cdm_created_cb,
    scoped_refptr<ContentDecryptionModule> cdm)
    : cdm_created_cb_(std::move(cdm_created_cb)), cdm_(std::move(cdm)) {}

CdmInitializedPromise::~CdmInitializedPromise() = default;

void CdmInitializedPromise::resolve() {
  MarkPromiseSettled();
  std::move(cdm_created_cb_).Run(cdm_, CreateCdmStatus::kSuccess);
}

void CdmInitializedPromise::reject(CdmPromise::Exception exception_code,
                                   uint32_t system_code,
                                   const std::string& error_message) {
  MarkPromiseSettled();
  std::move(cdm_created_cb_).Run(nullptr, CreateCdmStatus::kInitCdmFailed);
  // Usually after this |this| (and the |cdm_| within it) will be destroyed.
}

}  // namespace media
