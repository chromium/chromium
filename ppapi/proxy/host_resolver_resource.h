// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_HOST_RESOLVER_RESOURCE_H_
#define PPAPI_PROXY_HOST_RESOLVER_RESOURCE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "ppapi/proxy/host_resolver_resource_base.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_host_resolver_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT HostResolverResource
    : public HostResolverResourceBase,
      public thunk::PPB_HostResolver_API {
 public:
  HostResolverResource(Connection connection, PP_Instance instance);

  HostResolverResource(const HostResolverResource&) = delete;
  HostResolverResource& operator=(const HostResolverResource&) = delete;

  ~HostResolverResource() override;

  // PluginResource overrides.
  thunk::PPB_HostResolver_API* AsPPB_HostResolver_API() override;

  // thunk::PPB_HostResolver_API implementation.
  int32_t Resolve(const char* host,
                  uint16_t port,
                  const PP_HostResolver_Hint* hint,
                  scoped_refptr<TrackedCallback> callback) override;
  PP_Var GetCanonicalName() override;
  uint32_t GetNetAddressCount() override;
  PP_Resource GetNetAddress(uint32_t index) override;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_HOST_RESOLVER_RESOURCE_H_
