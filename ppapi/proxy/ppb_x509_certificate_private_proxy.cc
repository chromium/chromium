// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_x509_certificate_private_proxy.h"

#include "ppapi/c/private/ppb_x509_certificate_private.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"
#include "ppapi/shared_impl/private/ppb_x509_util_shared.h"

namespace ppapi {
namespace proxy {

namespace {

class X509CertificatePrivate : public PPB_X509Certificate_Private_Shared {
 public:
  X509CertificatePrivate(PP_Instance instance);

  X509CertificatePrivate(const X509CertificatePrivate&) = delete;
  X509CertificatePrivate& operator=(const X509CertificatePrivate&) = delete;

  ~X509CertificatePrivate() override;

  bool ParseDER(const std::vector<char>& der,
                PPB_X509Certificate_Fields* result) override;
};

X509CertificatePrivate::X509CertificatePrivate(PP_Instance instance)
    : PPB_X509Certificate_Private_Shared(OBJECT_IS_PROXY, instance) {
}

X509CertificatePrivate::~X509CertificatePrivate() {
}

bool X509CertificatePrivate::ParseDER(const std::vector<char>& der,
                                      PPB_X509Certificate_Fields* result) {
  return PPB_X509Util_Shared::GetCertificateFields(der.data(), der.size(),
                                                   result);
}

}  // namespace

//------------------------------------------------------------------------------

PPB_X509Certificate_Private_Proxy::PPB_X509Certificate_Private_Proxy(
    Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_X509Certificate_Private_Proxy::~PPB_X509Certificate_Private_Proxy() {
}

// static
PP_Resource PPB_X509Certificate_Private_Proxy::CreateProxyResource(
    PP_Instance instance) {
  return (new X509CertificatePrivate(instance))->GetReference();
}

bool PPB_X509Certificate_Private_Proxy::OnMessageReceived(
    const IPC::Message& msg) {
  return false;
}

}  // namespace proxy
}  // namespace ppapi
