// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/x509_certificate_private.h"

#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_X509Certificate_Private_0_1>() {
  return PPB_X509CERTIFICATE_PRIVATE_INTERFACE_0_1;
}

}  // namespace

X509CertificatePrivate::X509CertificatePrivate() : Resource() {}

X509CertificatePrivate::X509CertificatePrivate(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

X509CertificatePrivate::X509CertificatePrivate(const InstanceHandle& instance) {
  if (has_interface<PPB_X509Certificate_Private_0_1>()) {
    PassRefFromConstructor(get_interface<PPB_X509Certificate_Private_0_1>()->
        Create(instance.pp_instance()));
  }
}

// static
bool X509CertificatePrivate::IsAvailable() {
  return has_interface<PPB_X509Certificate_Private_0_1>();
}

bool X509CertificatePrivate::Initialize(const char* bytes, uint32_t length) {
  if (!has_interface<PPB_X509Certificate_Private_0_1>())
    return false;
  PP_Bool result = get_interface<PPB_X509Certificate_Private_0_1>()->Initialize(
      pp_resource(),
      bytes,
      length);
  return PP_ToBool(result);
}

Var X509CertificatePrivate::GetField(
    PP_X509Certificate_Private_Field field) const {
  if (!has_interface<PPB_X509Certificate_Private_0_1>())
    return Var();
  return Var(PassRef(),
      get_interface<PPB_X509Certificate_Private_0_1>()->GetField(pp_resource(),
                                                                 field));
}

}  // namespace pp
