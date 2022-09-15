// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_X509_CERTIFICATE_PRIVATE_PROXY_H_
#define PPAPI_PROXY_PPB_X509_CERTIFICATE_PRIVATE_PROXY_H_

#include "base/compiler_specific.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {
namespace proxy {

class PPB_X509Certificate_Private_Proxy
    : public InterfaceProxy {
 public:
  explicit PPB_X509Certificate_Private_Proxy(Dispatcher* dispatcher);

  PPB_X509Certificate_Private_Proxy(const PPB_X509Certificate_Private_Proxy&) =
      delete;
  PPB_X509Certificate_Private_Proxy& operator=(
      const PPB_X509Certificate_Private_Proxy&) = delete;

  ~PPB_X509Certificate_Private_Proxy() override;
  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  static const ApiID kApiID = API_ID_PPB_X509_CERTIFICATE_PRIVATE;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_X509_CERTIFICATE_PRIVATE_PROXY_H_
