// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_X509_CERTIFICATE_PRIVATE_API_H_
#define PPAPI_THUNK_PPB_X509_CERTIFICATE_PRIVATE_API_H_

#include <stdint.h>

#include "ppapi/c/private/ppb_x509_certificate_private.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_X509Certificate_Private_API {
 public:
  virtual ~PPB_X509Certificate_Private_API() {}

  virtual PP_Bool Initialize(const char* bytes, uint32_t length) = 0;
  virtual PP_Var GetField(PP_X509Certificate_Private_Field field) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_X509_CERTIFICATE_PRIVATE_API_H_
