// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/cdm_result_promise_helper.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"

namespace media {

CdmResultForUMA ConvertCdmExceptionToResultForUMA(
    CdmPromise::Exception exception_code) {
  switch (exception_code) {
    case CdmPromise::Exception::NOT_SUPPORTED_ERROR:
      return NOT_SUPPORTED_ERROR;
    case CdmPromise::Exception::INVALID_STATE_ERROR:
      return INVALID_STATE_ERROR;
    case CdmPromise::Exception::QUOTA_EXCEEDED_ERROR:
      return QUOTA_EXCEEDED_ERROR;
    case CdmPromise::Exception::TYPE_ERROR:
      return TYPE_ERROR;
  }
  NOTREACHED();
  return INVALID_STATE_ERROR;
}

blink::WebContentDecryptionModuleException ConvertCdmException(
    CdmPromise::Exception exception_code) {
  switch (exception_code) {
    case CdmPromise::Exception::NOT_SUPPORTED_ERROR:
      return blink::kWebContentDecryptionModuleExceptionNotSupportedError;
    case CdmPromise::Exception::INVALID_STATE_ERROR:
      return blink::kWebContentDecryptionModuleExceptionInvalidStateError;
    case CdmPromise::Exception::QUOTA_EXCEEDED_ERROR:
      return blink::kWebContentDecryptionModuleExceptionQuotaExceededError;
    case CdmPromise::Exception::TYPE_ERROR:
      return blink::kWebContentDecryptionModuleExceptionTypeError;
  }
  NOTREACHED();
  return blink::kWebContentDecryptionModuleExceptionInvalidStateError;
}

blink::WebEncryptedMediaKeyInformation::KeyStatus ConvertCdmKeyStatus(
    media::CdmKeyInformation::KeyStatus key_status) {
  switch (key_status) {
    case media::CdmKeyInformation::USABLE:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::kUsable;
    case media::CdmKeyInformation::INTERNAL_ERROR:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::kInternalError;
    case media::CdmKeyInformation::EXPIRED:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::kExpired;
    case media::CdmKeyInformation::OUTPUT_RESTRICTED:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::
          kOutputRestricted;
    case media::CdmKeyInformation::OUTPUT_DOWNSCALED:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::
          kOutputDownscaled;
    case media::CdmKeyInformation::KEY_STATUS_PENDING:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::kStatusPending;
    case media::CdmKeyInformation::RELEASED:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::kReleased;
  }
  NOTREACHED();
  return blink::WebEncryptedMediaKeyInformation::KeyStatus::kInternalError;
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

}  // namespace media
