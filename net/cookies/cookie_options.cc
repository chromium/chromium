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

  if (cookie_util::IsSchemefulSameSiteEnabled())
    return schemeful_context_;

  return context_;
}

const CookieOptions::SameSiteCookieContext::ContextMetadata&
CookieOptions::SameSiteCookieContext::GetMetadataForCurrentSchemefulMode()
    const {
  return cookie_util::IsSchemefulSameSiteEnabled() ? schemeful_metadata()
                                                   : metadata();
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

bool operator!=(const CookieOptions::SameSiteCookieContext& lhs,
                const CookieOptions::SameSiteCookieContext& rhs) {
  return !(lhs == rhs);
}

bool operator==(
    const CookieOptions::SameSiteCookieContext::ContextMetadata& lhs,
    const CookieOptions::SameSiteCookieContext::ContextMetadata& rhs) {
  return std::tie(lhs.cross_site_redirect_downgrade,
                  lhs.redirect_type_bug_1221316) ==
         std::tie(rhs.cross_site_redirect_downgrade,
                  rhs.redirect_type_bug_1221316);
}

bool operator!=(
    const CookieOptions::SameSiteCookieContext::ContextMetadata& lhs,
    const CookieOptions::SameSiteCookieContext::ContextMetadata& rhs) {
  return !(lhs == rhs);
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
