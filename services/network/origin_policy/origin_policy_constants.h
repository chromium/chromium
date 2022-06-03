// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_CONSTANTS_H_
#define SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_CONSTANTS_H_

namespace network {
const char kOriginPolicyWellKnown[] = "/.well-known/origin-policy";

// Maximum policy size (implementation-defined limit in bytes).
// (Limit copied from network::SimpleURLLoader::kMaxBoundedStringDownloadSize.)
static const size_t kOriginPolicyMaxPolicySize = 1024 * 1024;
}  // namespace network

#endif  // SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_CONSTANTS_H_
