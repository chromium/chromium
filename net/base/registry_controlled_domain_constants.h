// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_REGISTRY_CONTROLLED_DOMAIN_CONSTANTS_H_
#define NET_BASE_REGISTRY_CONTROLLED_DOMAIN_CONSTANTS_H_

namespace net {

enum {
  // No tag.
  kDafsaFound = 0,
  // Key excluded from set via exception.
  kDafsaExceptionRule = 1,
  // Key matched a wildcard rule.
  kDafsaWildcardRule = 2,
  // Key matched a private rule.
  kDafsaPrivateRule = 4,
};

}

#endif  // NET_BASE_REGISTRY_CONTROLLED_DOMAIN_CONSTANTS_H_
