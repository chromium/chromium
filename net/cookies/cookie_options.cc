// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#include "net/cookies/cookie_options.h"

#include <tuple>

#include "net/cookies/cookie_util.h"

namespace net {

CookieOptions::SameSiteCookieContext
CookieOptions::SameSiteCookieContext::MakeInclusive() {
  return SameSiteCookieContext(ContextType::SAME_SITE_STRICT,
                               ContextType::SAME_SITE_STRICT);
}

CookieOptions::SameSiteCookieContext
CookieOptions::SameSiteCookieContext::MakeInclusiveForSet() {
  return SameSiteCookieContext(ContextType::SAME_SITE_LAX,
                               ContextType::SAME_SITE_LAX);
}

CookieOptions::SameSiteCookieContext::ContextType
CookieOptions::SameSiteCookieContext::GetContextForCookieInclusion() const {
  DCHECK_LE(schemeful_context_, context_);
  return schemeful_context_;
}

const CookieOptions::SameSiteCookieContext::ContextMetadata&
CookieOptions::SameSiteCookieContext::GetMetadataForCurrentSchemefulMode()
    const {
  return schemeful_metadata();
}

void CookieOptions::SameSiteCookieContext::SetContextTypesForTesting(
    ContextType context_type,
    ContextType schemeful_context_type) {
  context_ = context_type;
  schemeful_context_ = schemeful_context_type;
}

bool CookieOptions::SameSiteCookieContext::CompleteEquivalenceForTesting(
    const SameSiteCookieContext& other) const {
  return (*this == other) && (metadata() == other.metadata()) &&
         (schemeful_metadata() == other.schemeful_metadata());
}

bool operator==(const CookieOptions::SameSiteCookieContext& lhs,
                const CookieOptions::SameSiteCookieContext& rhs) {
  return std::tie(lhs.context_, lhs.schemeful_context_) ==
         std::tie(rhs.context_, rhs.schemeful_context_);
}

// Keep default values in sync with
// services/network/public/mojom/cookie_manager.mojom.
CookieOptions::CookieOptions()
    : same_site_cookie_context_(SameSiteCookieContext(
          SameSiteCookieContext::ContextType::CROSS_SITE)) {}

CookieOptions::CookieOptions(const CookieOptions& other) = default;
CookieOptions::CookieOptions(CookieOptions&& other) = default;
CookieOptions::~CookieOptions() = default;

CookieOptions& CookieOptions::operator=(const CookieOptions&) = default;
CookieOptions& CookieOptions::operator=(CookieOptions&&) = default;

// static
CookieOptions CookieOptions::MakeAllInclusive() {
  CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(SameSiteCookieContext::MakeInclusive());
  options.set_do_not_update_access_time();
  return options;
}

}  // namespace net
