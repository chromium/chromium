// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_HOST_RESOLVER_PRIVATE_RESOURCE_H_
#define PPAPI_PROXY_HOST_RESOLVER_PRIVATE_RESOURCE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "ppapi/proxy/host_resolver_resource_base.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_host_resolver_private_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT HostResolverPrivateResource
    : public HostResolverResourceBase,
      public thunk::PPB_HostResolver_Private_API {
 public:
  HostResolverPrivateResource(Connection connection,
                              PP_Instance instance);

  HostResolverPrivateResource(const HostResolverPrivateResource&) = delete;
  HostResolverPrivateResource& operator=(const HostResolverPrivateResource&) =
      delete;

  ~HostResolverPrivateResource() override;

  // PluginResource overrides.
  thunk::PPB_HostResolver_Private_API* AsPPB_HostResolver_Private_API()
      override;

  // PPB_HostResolver_Private_API implementation.
  int32_t Resolve(const char* host,
                  uint16_t port,
                  const PP_HostResolver_Private_Hint* hint,
                  scoped_refptr<TrackedCallback> callback) override;
  PP_Var GetCanonicalName() override;
  uint32_t GetSize() override;
  bool GetNetAddress(uint32_t index, PP_NetAddress_Private* address) override;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_HOST_RESOLVER_PRIVATE_RESOURCE_H_
