// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_change_dispatcher_test_helpers.h"

#include "base/notreached.h"

namespace net {

// Google Test helper.
std::ostream& operator<<(std::ostream& os, const CookieChangeCause& cause) {
  switch (cause) {
    case CookieChangeCause::INSERTED:
      return os << "INSERTED";
    case CookieChangeCause::EXPLICIT:
      return os << "EXPLICIT";
    case CookieChangeCause::UNKNOWN_DELETION:
      return os << "UNKNOWN_DELETION";
    case CookieChangeCause::OVERWRITE:
      return os << "OVERWRITE";
    case CookieChangeCause::EXPIRED:
      return os << "EXPIRED";
    case CookieChangeCause::EVICTED:
      return os << "EVICTED";
    case CookieChangeCause::EXPIRED_OVERWRITE:
      return os << "EXPIRED_OVERWRITE";
  }
  NOTREACHED_IN_MIGRATION();
  return os;
}

}  // namespace net
