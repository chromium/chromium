// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/cdm_result_promise_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace blink {

CdmResultForUMA ConvertCdmExceptionToResultForUMA(
    media::CdmPromise::Exception exception_code) {
  switch (exception_code) {
    case media::CdmPromise::Exception::NOT_SUPPORTED_ERROR:
      return NOT_SUPPORTED_ERROR;
    case media::CdmPromise::Exception::INVALID_STATE_ERROR:
      return INVALID_STATE_ERROR;
    case media::CdmPromise::Exception::QUOTA_EXCEEDED_ERROR:
      return QUOTA_EXCEEDED_ERROR;
    case media::CdmPromise::Exception::TYPE_ERROR:
      return TYPE_ERROR;
  }
  NOTREACHED_IN_MIGRATION();
  return INVALID_STATE_ERROR;
}

WebContentDecryptionModuleException ConvertCdmException(
    media::CdmPromise::Exception exception_code) {
  switch (exception_code) {
    case media::CdmPromise::Exception::NOT_SUPPORTED_ERROR:
      return kWebContentDecryptionModuleExceptionNotSupportedError;
    case media::CdmPromise::Exception::INVALID_STATE_ERROR:
      return kWebContentDecryptionModuleExceptionInvalidStateError;
    case media::CdmPromise::Exception::QUOTA_EXCEEDED_ERROR:
      return kWebContentDecryptionModuleExceptionQuotaExceededError;
    case media::CdmPromise::Exception::TYPE_ERROR:
      return kWebContentDecryptionModuleExceptionTypeError;
  }
  NOTREACHED_IN_MIGRATION();
  return kWebContentDecryptionModuleExceptionInvalidStateError;
}

WebEncryptedMediaKeyInformation::KeyStatus ConvertCdmKeyStatus(
    media::CdmKeyInformation::KeyStatus key_status) {
  switch (key_status) {
    case media::CdmKeyInformation::USABLE:
      return WebEncryptedMediaKeyInformation::KeyStatus::kUsable;
    case media::CdmKeyInformation::INTERNAL_ERROR:
      return WebEncryptedMediaKeyInformation::KeyStatus::kInternalError;
    case media::CdmKeyInformation::EXPIRED:
      return WebEncryptedMediaKeyInformation::KeyStatus::kExpired;
    case media::CdmKeyInformation::OUTPUT_RESTRICTED:
      return WebEncryptedMediaKeyInformation::KeyStatus::kOutputRestricted;
    case media::CdmKeyInformation::OUTPUT_DOWNSCALED:
      return WebEncryptedMediaKeyInformation::KeyStatus::kOutputDownscaled;
    case media::CdmKeyInformation::KEY_STATUS_PENDING:
      return WebEncryptedMediaKeyInformation::KeyStatus::kStatusPending;
    case media::CdmKeyInformation::RELEASED:
      return WebEncryptedMediaKeyInformation::KeyStatus::kReleased;
  }
  NOTREACHED_IN_MIGRATION();
  return WebEncryptedMediaKeyInformation::KeyStatus::kInternalError;
}

void ReportCdmResultUMA(const std::string& uma_name,
                        uint32_t system_code,
                        CdmResultForUMA result) {
  if (uma_name.empty())
    return;

  // Only report system code on promise rejection.
  if (result != CdmResultForUMA::SUCCESS)
    base::UmaHistogramSparse(uma_name + ".SystemCode", system_code);

  base::UmaHistogramEnumeration(uma_name, result, NUM_RESULT_CODES);
}

}  // namespace blink
