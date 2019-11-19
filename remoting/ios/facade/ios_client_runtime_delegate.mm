// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "remoting/ios/facade/ios_client_runtime_delegate.h"

#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/oauth_token_getter_proxy.h"
#include "remoting/ios/facade/ios_oauth_token_getter.h"

namespace remoting {

IosClientRuntimeDelegate::IosClientRuntimeDelegate() : weak_factory_(this) {
  runtime_ = ChromotingClientRuntime::GetInstance();
  token_getter_ = std::make_unique<IosOauthTokenGetter>();
}

IosClientRuntimeDelegate::~IosClientRuntimeDelegate() {}

void IosClientRuntimeDelegate::RuntimeWillShutdown() {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  // Nothing to do.
}

void IosClientRuntimeDelegate::RuntimeDidShutdown() {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());
  // Nothing to do.
}

base::WeakPtr<OAuthTokenGetter> IosClientRuntimeDelegate::oauth_token_getter() {
  return token_getter_->GetWeakPtr();
}

base::WeakPtr<IosClientRuntimeDelegate> IosClientRuntimeDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
