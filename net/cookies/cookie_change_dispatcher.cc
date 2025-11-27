// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_change_dispatcher.h"

namespace net {

const char* CookieChangeCauseToString(CookieChangeCause cause) {
  switch (cause) {
    case CookieChangeCause::INSERTED:
      return "inserted";
    case CookieChangeCause::EXPLICIT:
      return "explicit";
    case CookieChangeCause::UNKNOWN_DELETION:
      return "unknown";
    case CookieChangeCause::OVERWRITE:
      return "overwrite";
    case CookieChangeCause::EXPIRED:
      return "expired";
    case CookieChangeCause::EVICTED:
      return "evicted";
    case CookieChangeCause::EXPIRED_OVERWRITE:
      return "expired_overwrite";
    case CookieChangeCause::INSERTED_NO_CHANGE_OVERWRITE:
      return "inserted_no_change_overwrite";
    case CookieChangeCause::INSERTED_NO_VALUE_CHANGE_OVERWRITE:
      return "inserted_no_value_change_overwrite";
  }
}

CookieChangeInfo::CookieChangeInfo() = default;

CookieChangeInfo::CookieChangeInfo(const CanonicalCookie& cookie,
                                   CookieAccessResult access_result,
                                   CookieChangeCause cause)
    : cookie(cookie), access_result(access_result), cause(cause) {
  DCHECK(access_result.status.IsInclude());
  if (CookieChangeCauseIsDeletion(cause)) {
    DCHECK_EQ(access_result.effective_same_site,
              CookieEffectiveSameSite::UNDEFINED);
  }
}

CookieChangeInfo::~CookieChangeInfo() = default;

bool CookieChangeCauseIsDeletion(CookieChangeCause cause) {
  switch (cause) {
    case CookieChangeCause::INSERTED:
    case CookieChangeCause::INSERTED_NO_CHANGE_OVERWRITE:
    case CookieChangeCause::INSERTED_NO_VALUE_CHANGE_OVERWRITE:
      return false;
    case CookieChangeCause::EXPIRED:
    case CookieChangeCause::EXPIRED_OVERWRITE:
    case CookieChangeCause::EXPLICIT:
    case CookieChangeCause::EVICTED:
    case CookieChangeCause::OVERWRITE:
    case CookieChangeCause::UNKNOWN_DELETION:
      return true;
  }
  NOTREACHED();
}

}  // namespace net
