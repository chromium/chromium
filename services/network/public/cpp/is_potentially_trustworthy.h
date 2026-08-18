// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_IS_POTENTIALLY_TRUSTWORTHY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_IS_POTENTIALLY_TRUSTWORTHY_H_

#include "net/base/is_potentially_trustworthy.h"

// TODO(crbug.com/546285538): This header is deprecated. Migrate all callers to
// use `net/base/is_potentially_trustworthy.h` directly and remove this file.
namespace network {

using net::IsOriginPotentiallyTrustworthy;
using net::IsUrlPotentiallyTrustworthy;
using net::SecureOriginAllowlist;

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_IS_POTENTIALLY_TRUSTWORTHY_H_
