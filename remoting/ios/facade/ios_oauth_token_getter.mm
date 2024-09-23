// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/facade/ios_oauth_token_getter.h"

#include "base/strings/sys_string_conversions.h"
#include "remoting/ios/facade/remoting_authentication.h"
#include "remoting/ios/facade/remoting_service.h"

namespace remoting {

IosOauthTokenGetter::IosOauthTokenGetter() : weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

IosOauthTokenGetter::~IosOauthTokenGetter() {}

void IosOauthTokenGetter::CallWithToken(TokenCallback on_access_token) {
}

void IosOauthTokenGetter::InvalidateCache() {
  [RemotingService.instance.authentication invalidateCache];
}

base::WeakPtr<IosOauthTokenGetter> IosOauthTokenGetter::GetWeakPtr() {
  return weak_ptr_;
}

}  // namespace remoting
