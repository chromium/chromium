// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/new_session_cdm_result_promise.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "media/blink/cdm_result_promise_helper.h"
#include "third_party/blink/public/platform/web_string.h"

namespace media {

namespace {

const char kTimeUMAPrefix[] = "TimeTo.";

CdmResultForUMA ConvertStatusToUMAResult(SessionInitStatus status) {
  switch (status) {
    case SessionInitStatus::UNKNOWN_STATUS:
      break;
    case SessionInitStatus::NEW_SESSION:
      return SUCCESS;
    case SessionInitStatus::SESSION_NOT_FOUND:
      return SESSION_NOT_FOUND;
    case SessionInitStatus::SESSION_ALREADY_EXISTS:
      return SESSION_ALREADY_EXISTS;
  }
  NOTREACHED();
  return INVALID_STATE_ERROR;
}

}  // namespace

static blink::WebContentDecryptionModuleResult::SessionStatus ConvertStatus(
    SessionInitStatus status) {
  switch (status) {
    case SessionInitStatus::UNKNOWN_STATUS:
      break;
    case SessionInitStatus::NEW_SESSION:
      return blink::WebContentDecryptionModuleResult::kNewSession;
    case SessionInitStatus::SESSION_NOT_FOUND:
      return blink::WebContentDecryptionModuleResult::kSessionNotFound;
    case SessionInitStatus::SESSION_ALREADY_EXISTS:
      return blink::WebContentDecryptionModuleResult::kSessionAlreadyExists;
  }
  NOTREACHED();
  return blink::WebContentDecryptionModuleResult::kSessionNotFound;
}

NewSessionCdmResultPromise::NewSessionCdmResultPromise(
    const blink::WebContentDecryptionModuleResult& result,
    const std::string& key_system_uma_prefix,
    const std::string& uma_name,
    const SessionInitializedCB& new_session_created_cb,
    const std::vector<SessionInitStatus>& expected_statuses)
    : web_cdm_result_(result),
      key_system_uma_prefix_(key_system_uma_prefix),
      uma_name_(uma_name),
      new_session_created_cb_(new_session_created_cb),
      expected_statuses_(expected_statuses),
      creation_time_(base::TimeTicks::Now()) {}

NewSessionCdmResultPromise::~NewSessionCdmResultPromise() {
  if (!IsPromiseSettled())
    RejectPromiseOnDestruction();
}

void NewSessionCdmResultPromise::resolve(const std::string& session_id) {
  DVLOG(1) << __func__ << ": session_id = " << session_id;

  // |new_session_created_cb_| uses a WeakPtr<> and may not do anything
  // if the session object has been destroyed.
  SessionInitStatus status = SessionInitStatus::UNKNOWN_STATUS;
  new_session_created_cb_.Run(session_id, &status);

  if (!base::Contains(expected_statuses_, status)) {
    reject(Exception::INVALID_STATE_ERROR, 0,
           "Cannot finish session initialization");
    return;
  }

  MarkPromiseSettled();
  ReportCdmResultUMA(key_system_uma_prefix_ + uma_name_, 0,
                     ConvertStatusToUMAResult(status));

  // Only report time for promise resolution (not rejection).
  base::UmaHistogramTimes(key_system_uma_prefix_ + kTimeUMAPrefix + uma_name_,
                          base::TimeTicks::Now() - creation_time_);

  web_cdm_result_.CompleteWithSession(ConvertStatus(status));
}

void NewSessionCdmResultPromise::reject(CdmPromise::Exception exception_code,
                                        uint32_t system_code,
                                        const std::string& error_message) {
  DVLOG(1) << __func__ << ": system_code = " << system_code
           << ", error_message = " << error_message;

  MarkPromiseSettled();
  ReportCdmResultUMA(key_system_uma_prefix_ + uma_name_, system_code,
                     ConvertCdmExceptionToResultForUMA(exception_code));
  web_cdm_result_.CompleteWithError(ConvertCdmException(exception_code),
                                    system_code,
                                    blink::WebString::FromUTF8(error_message));
}

}  // namespace media
