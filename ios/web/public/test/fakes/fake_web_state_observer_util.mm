// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_web_state_observer_util.h"

#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/security/ssl_status.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
TestDidChangeVisibleSecurityStateInfo::TestDidChangeVisibleSecurityStateInfo() =
    default;
TestDidChangeVisibleSecurityStateInfo::
    ~TestDidChangeVisibleSecurityStateInfo() = default;
TestDidStartNavigationInfo::TestDidStartNavigationInfo() = default;
TestDidStartNavigationInfo::~TestDidStartNavigationInfo() = default;
TestDidRedirectNavigationInfo::TestDidRedirectNavigationInfo() = default;
TestDidRedirectNavigationInfo::~TestDidRedirectNavigationInfo() = default;
TestDidFinishNavigationInfo::TestDidFinishNavigationInfo() = default;
TestDidFinishNavigationInfo::~TestDidFinishNavigationInfo() = default;
TestUpdateFaviconUrlCandidatesInfo::TestUpdateFaviconUrlCandidatesInfo() =
    default;
TestUpdateFaviconUrlCandidatesInfo::~TestUpdateFaviconUrlCandidatesInfo() =
    default;
}  // namespace web
