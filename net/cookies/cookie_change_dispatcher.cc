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
  return cause != CookieChangeCause::INSERTED;
}

}  // namespace net
