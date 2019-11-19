// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_COOKIE_ACCESS_DELEGATE_IMPL_H_
#define SERVICES_NETWORK_COOKIE_ACCESS_DELEGATE_IMPL_H_

#include "base/component_export.h"
#include "net/cookies/cookie_access_delegate.h"
#include "services/network/cookie_settings.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace network {

// This class acts as a delegate for the CookieStore to query the
// CookieManager's CookieSettings for instructions on how to handle a given
// cookie with respect to SameSite.
class COMPONENT_EXPORT(NETWORK_SERVICE) CookieAccessDelegateImpl
    : public net::CookieAccessDelegate {
 public:
  // If |type| is USE_CONTENT_SETTINGS, a non-null |cookie_settings| is
  // expected. |cookie_settings| contains the set of content settings that
  // describes which cookies should be subject to legacy access rules.
  // If non-null, |cookie_settings| is expected to outlive this class.
  CookieAccessDelegateImpl(mojom::CookieAccessDelegateType type,
                           const CookieSettings* cookie_settings = nullptr);

  ~CookieAccessDelegateImpl() override;

  // net::CookieAccessDelegate implementation:
  net::CookieAccessSemantics GetAccessSemantics(
      const net::CanonicalCookie& cookie) const override;
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const GURL& site_for_cookies) const override;

 private:
  const mojom::CookieAccessDelegateType type_;
  const CookieSettings* const cookie_settings_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_COOKIE_ACCESS_DELEGATE_IMPL_H_
