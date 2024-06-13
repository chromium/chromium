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

std::optional<media::HdcpVersion> MaybeHdcpVersionFromString(
    const std::string& hdcp_version_string) {
  // The strings are specified in the explainer doc:
  // https://github.com/WICG/hdcp-detection/blob/master/explainer.md
  if (hdcp_version_string.empty()) {
    return media::HdcpVersion::kHdcpVersionNone;
  } else if (hdcp_version_string == "1.0") {
    return media::HdcpVersion::kHdcpVersion1_0;
  } else if (hdcp_version_string == "1.1") {
    return media::HdcpVersion::kHdcpVersion1_1;
  } else if (hdcp_version_string == "1.2") {
    return media::HdcpVersion::kHdcpVersion1_2;
  } else if (hdcp_version_string == "1.3") {
    return media::HdcpVersion::kHdcpVersion1_3;
  } else if (hdcp_version_string == "1.4") {
    return media::HdcpVersion::kHdcpVersion1_4;
  } else if (hdcp_version_string == "2.0") {
    return media::HdcpVersion::kHdcpVersion2_0;
  } else if (hdcp_version_string == "2.1") {
    return media::HdcpVersion::kHdcpVersion2_1;
  } else if (hdcp_version_string == "2.2") {
    return media::HdcpVersion::kHdcpVersion2_2;
  } else if (hdcp_version_string == "2.3") {
    return media::HdcpVersion::kHdcpVersion2_3;
  }

  return std::nullopt;
}

}  // namespace media
