// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/c/private/ppb_x509_certificate_private.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_x509_certificate_private_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_X509Certificate_Private_API>
    EnterX509CertificatePrivate;

PP_Resource Create(PP_Instance instance) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateX509CertificatePrivate(instance);
}

PP_Bool IsX509CertificatePrivate(PP_Resource resource) {
  EnterX509CertificatePrivate enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool Initialize(PP_Resource certificate,
                   const char *bytes,
                   uint32_t length) {
  EnterX509CertificatePrivate enter(certificate, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->Initialize(bytes, length);
}

PP_Var GetField(PP_Resource certificate,
                PP_X509Certificate_Private_Field field) {
  EnterX509CertificatePrivate enter(certificate, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetField(field);
}

const PPB_X509Certificate_Private g_ppb_x509_certificate_thunk = {
  &Create,
  &IsX509CertificatePrivate,
  &Initialize,
  &GetField
};

}  // namespace

const PPB_X509Certificate_Private_0_1*
GetPPB_X509Certificate_Private_0_1_Thunk() {
  return &g_ppb_x509_certificate_thunk;
}

}  // namespace thunk
}  // namespace ppapi
