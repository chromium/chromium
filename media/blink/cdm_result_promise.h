// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_CDM_RESULT_PROMISE_H_
#define MEDIA_BLINK_CDM_RESULT_PROMISE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "media/blink/cdm_result_promise_helper.h"
#include "third_party/blink/public/platform/web_content_decryption_module_result.h"
#include "third_party/blink/public/platform/web_string.h"

namespace media {

const char kTimeUMAPrefix[] = "TimeTo.";

// Used to convert a WebContentDecryptionModuleResult into a CdmPromiseTemplate
// so that it can be passed through Chromium. When resolve(T) is called, the
// appropriate complete...() method on WebContentDecryptionModuleResult will be
// invoked. If reject() is called instead,
// WebContentDecryptionModuleResult::completeWithError() is called.
// If constructed with a |uma_name|, CdmResultPromise will report the promise
// result (success or rejection code) to UMA.
template <typename... T>
class CdmResultPromise : public CdmPromiseTemplate<T...> {
 public:
  CdmResultPromise(const blink::WebContentDecryptionModuleResult& result,
                   const std::string& key_system_uma_prefix,
                   const std::string& uma_name);
  ~CdmResultPromise() override;

  // CdmPromiseTemplate<T> implementation.
  void resolve(const T&... result) override;
  void reject(CdmPromise::Exception exception_code,
              uint32_t system_code,
              const std::string& error_message) override;

 private:
  using CdmPromiseTemplate<T...>::IsPromiseSettled;
  using CdmPromiseTemplate<T...>::MarkPromiseSettled;
  using CdmPromiseTemplate<T...>::RejectPromiseOnDestruction;

  blink::WebContentDecryptionModuleResult web_cdm_result_;

  // UMA prefix and name to report result and time to.
  std::string key_system_uma_prefix_;
  std::string uma_name_;

  // Time when |this| is created.
  base::TimeTicks creation_time_;

  DISALLOW_COPY_AND_ASSIGN(CdmResultPromise);
};

template <typename... T>
CdmResultPromise<T...>::CdmResultPromise(
    const blink::WebContentDecryptionModuleResult& result,
    const std::string& key_system_uma_prefix,
    const std::string& uma_name)
    : web_cdm_result_(result),
      key_system_uma_prefix_(key_system_uma_prefix),
      uma_name_(uma_name),
      creation_time_(base::TimeTicks::Now()) {
  DCHECK(!key_system_uma_prefix_.empty());
  DCHECK(!uma_name_.empty());
}

template <typename... T>
CdmResultPromise<T...>::~CdmResultPromise() {
  if (!IsPromiseSettled())
    RejectPromiseOnDestruction();
}

// "inline" is needed to prevent multiple definition error.

template <>
inline void CdmResultPromise<>::resolve() {
  MarkPromiseSettled();
  ReportCdmResultUMA(key_system_uma_prefix_ + uma_name_, 0, SUCCESS);

  // Only report time for promise resolution (not rejection).
  base::UmaHistogramTimes(key_system_uma_prefix_ + kTimeUMAPrefix + uma_name_,
                          base::TimeTicks::Now() - creation_time_);

  web_cdm_result_.Complete();
}

template <>
inline void CdmResultPromise<CdmKeyInformation::KeyStatus>::resolve(
    const CdmKeyInformation::KeyStatus& key_status) {
  MarkPromiseSettled();
  ReportCdmResultUMA(key_system_uma_prefix_ + uma_name_, 0, SUCCESS);

  // Only report time for promise resolution (not rejection).
  base::UmaHistogramTimes(key_system_uma_prefix_ + kTimeUMAPrefix + uma_name_,
                          base::TimeTicks::Now() - creation_time_);

  web_cdm_result_.CompleteWithKeyStatus(ConvertCdmKeyStatus(key_status));
}

template <typename... T>
void CdmResultPromise<T...>::reject(CdmPromise::Exception exception_code,
                                    uint32_t system_code,
                                    const std::string& error_message) {
  MarkPromiseSettled();
  ReportCdmResultUMA(key_system_uma_prefix_ + uma_name_, system_code,
                     ConvertCdmExceptionToResultForUMA(exception_code));
  web_cdm_result_.CompleteWithError(ConvertCdmException(exception_code),
                                    system_code,
                                    blink::WebString::FromUTF8(error_message));
}

}  // namespace media

#endif  // MEDIA_BLINK_CDM_RESULT_PROMISE_H_
