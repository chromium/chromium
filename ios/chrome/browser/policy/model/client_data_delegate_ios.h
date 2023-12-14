// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_CLIENT_DATA_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_CLIENT_DATA_DELEGATE_IOS_H_

#include "components/policy/core/common/cloud/client_data_delegate.h"

namespace policy {

// Sets iOS-specific fields in request protos for the DMServer.
class ClientDataDelegateIos : public ClientDataDelegate {
 public:
  ClientDataDelegateIos() = default;
  ClientDataDelegateIos(const ClientDataDelegateIos&) = delete;
  ClientDataDelegateIos& operator=(const ClientDataDelegateIos&) = delete;
  ~ClientDataDelegateIos() override = default;

  void FillRegisterBrowserRequest(
      enterprise_management::RegisterBrowserRequest* request,
      base::OnceClosure callback) const override;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_CLIENT_DATA_DELEGATE_IOS_H_
