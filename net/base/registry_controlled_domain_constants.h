// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_REGISTRY_CONTROLLED_DOMAIN_CONSTANTS_H_
#define NET_BASE_REGISTRY_CONTROLLED_DOMAIN_CONSTANTS_H_

#include "base/containers/enum_set.h"

namespace net {

// Tags encoded in the registry-controlled domains DAFSA(s). If the enumerators
// are renumbered or removed, all relevant gperf files must be regenerated
// (either by hand or by running //net/tools/tld_cleanup.cc).
enum class DomainRuleTag {
  // Key excluded from set via exception.
  kException,
  // Key matched a wildcard rule.
  kWildcard,
  // Key matched a private rule.
  kPrivate,
};

using DomainRuleTags = base::
    EnumSet<DomainRuleTag, DomainRuleTag::kException, DomainRuleTag::kPrivate>;
}

#endif  // NET_BASE_REGISTRY_CONTROLLED_DOMAIN_CONSTANTS_H_
