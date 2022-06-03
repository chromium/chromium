// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/session_certificate_policy_cache.h"

#include "ios/web/public/browser_state.h"
#include "ios/web/public/security/certificate_policy_cache.h"
#include "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

SessionCertificatePolicyCache::SessionCertificatePolicyCache(
    BrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(browser_state_);
}

SessionCertificatePolicyCache::~SessionCertificatePolicyCache() {}

scoped_refptr<CertificatePolicyCache>
SessionCertificatePolicyCache::GetCertificatePolicyCache() const {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  return web::BrowserState::GetCertificatePolicyCache(browser_state_);
}

}  // namespace web
