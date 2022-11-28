// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_PPB_X509_CERTIFICATE_PRIVATE_SHARED_H_
#define PPAPI_SHARED_IMPL_PRIVATE_PPB_X509_CERTIFICATE_PRIVATE_SHARED_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/values.h"
#include "ppapi/c/private/ppb_x509_certificate_private.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_x509_certificate_private_api.h"

namespace IPC {
template <class T>
struct ParamTraits;
}

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_X509Certificate_Fields {
 public:
  PPB_X509Certificate_Fields();
  PPB_X509Certificate_Fields(const PPB_X509Certificate_Fields& fields);

  // Takes ownership of |value|.
  void SetField(PP_X509Certificate_Private_Field field, base::Value value);
  PP_Var GetFieldAsPPVar(PP_X509Certificate_Private_Field field) const;

 private:
  // Friend so ParamTraits can serialize us.
  friend struct IPC::ParamTraits<ppapi::PPB_X509Certificate_Fields>;

  base::Value::List values_;
};

//------------------------------------------------------------------------------

class PPAPI_SHARED_EXPORT PPB_X509Certificate_Private_Shared
    : public thunk::PPB_X509Certificate_Private_API,
      public Resource {
 public:
  PPB_X509Certificate_Private_Shared(ResourceObjectType type,
                                     PP_Instance instance);
  // Used by tcp_socket_shared_impl to construct a certificate resource from a
  // server certificate.
  PPB_X509Certificate_Private_Shared(ResourceObjectType type,
                                     PP_Instance instance,
                                     const PPB_X509Certificate_Fields& fields);

  PPB_X509Certificate_Private_Shared(
      const PPB_X509Certificate_Private_Shared&) = delete;
  PPB_X509Certificate_Private_Shared& operator=(
      const PPB_X509Certificate_Private_Shared&) = delete;

  ~PPB_X509Certificate_Private_Shared() override;

  // Resource overrides.
  PPB_X509Certificate_Private_API* AsPPB_X509Certificate_Private_API() override;

  // PPB_X509Certificate_Private_API implementation.
  PP_Bool Initialize(const char* bytes, uint32_t length) override;
  PP_Var GetField(PP_X509Certificate_Private_Field field) override;

 protected:
  virtual bool ParseDER(const std::vector<char>& der,
                        PPB_X509Certificate_Fields* result);

 private:
  std::unique_ptr<PPB_X509Certificate_Fields> fields_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_PPB_X509_CERTIFICATE_PRIVATE_SHARED_H_
