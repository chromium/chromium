// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/fake_enterprise_proxy_tab_helper_delegate.h"

#import "ios/web/public/web_state.h"

FakeEnterpriseProxyTabHelperDelegate::FakeEnterpriseProxyTabHelperDelegate() =
    default;

FakeEnterpriseProxyTabHelperDelegate::~FakeEnterpriseProxyTabHelperDelegate() =
    default;

void FakeEnterpriseProxyTabHelperDelegate::SignIn(web::WebState* web_state) {
  ++sign_in_count_;
  last_web_state_ = web_state ? web_state->GetWeakPtr() : nullptr;
}
