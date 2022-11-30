// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/content_decryption_module.h"

#include "media/base/cdm_promise.h"

namespace media {

ContentDecryptionModule::ContentDecryptionModule() = default;

ContentDecryptionModule::~ContentDecryptionModule() = default;

// By default a CDM does not support this method.
void ContentDecryptionModule::GetStatusForPolicy(
    HdcpVersion min_hdcp_version,
    std::unique_ptr<KeyStatusCdmPromise> promise) {
  promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                  "GetStatusForPolicy() is not supported.");
}

void ContentDecryptionModule::DeleteOnCorrectThread() const {
  delete this;
}

// static
void ContentDecryptionModuleTraits::Destruct(
    const ContentDecryptionModule* cdm) {
  cdm->DeleteOnCorrectThread();
}

}  // namespace media
