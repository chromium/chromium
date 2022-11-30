// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_access_delegate.h"

#include <set>

#include "base/callback.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

CookieAccessDelegate::CookieAccessDelegate() = default;

CookieAccessDelegate::~CookieAccessDelegate() = default;

bool CookieAccessDelegate::ShouldTreatUrlAsTrustworthy(const GURL& url) const {
  return false;
}

}  // namespace net
