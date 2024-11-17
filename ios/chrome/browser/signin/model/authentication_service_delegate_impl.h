// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_DELEGATE_IMPL_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_DELEGATE_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "ios/chrome/browser/signin/model/authentication_service_delegate.h"

class BrowsingDataRemover;
class PrefService;

// Concrete implementation of AuthenticationServiceDelegate injected in the
// AuthenticationService by the factory.
class AuthenticationServiceDelegateImpl final
    : public AuthenticationServiceDelegate {
 public:
  AuthenticationServiceDelegateImpl(BrowsingDataRemover* data_remover,
                                    PrefService* pref_service);

  // AuthenticationServiceDelegate implementation.
  void ClearBrowsingData(base::OnceClosure closure) final;
  void ClearBrowsingDataForSignedinPeriod(base::OnceClosure closure) final;

 private:
  raw_ref<BrowsingDataRemover> const data_remover_;
  raw_ref<PrefService> const pref_service_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_DELEGATE_IMPL_H_
