// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_FACADE_IOS_CLIENT_RUNTIME_DELEGATE_H_
#define REMOTING_IOS_FACADE_IOS_CLIENT_RUNTIME_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/chromoting_client_runtime.h"

namespace remoting {

class IosOauthTokenGetter;

class IosClientRuntimeDelegate : public ChromotingClientRuntime::Delegate {
 public:
  IosClientRuntimeDelegate();

  IosClientRuntimeDelegate(const IosClientRuntimeDelegate&) = delete;
  IosClientRuntimeDelegate& operator=(const IosClientRuntimeDelegate&) = delete;

  ~IosClientRuntimeDelegate() override;

  // remoting::ChromotingClientRuntime::Delegate overrides.
  void RuntimeWillShutdown() override;
  void RuntimeDidShutdown() override;
  base::WeakPtr<OAuthTokenGetter> oauth_token_getter() override;

  base::WeakPtr<IosClientRuntimeDelegate> GetWeakPtr();

 private:
  std::unique_ptr<IosOauthTokenGetter> token_getter_;
  raw_ptr<ChromotingClientRuntime> runtime_;

  base::WeakPtrFactory<IosClientRuntimeDelegate> weak_factory_;
};

}  // namespace remoting

#endif  // REMOTING_IOS_FACADE_IOS_CLIENT_RUNTIME_DELEGATE_H_
