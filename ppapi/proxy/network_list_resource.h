// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_NETWORK_LIST_RESOURCE_H_
#define PPAPI_PROXY_NETWORK_LIST_RESOURCE_H_

#include <stdint.h>

#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_network_list_api.h"

namespace ppapi {
namespace proxy {

class NetworkListResource
    : public Resource,
      public thunk::PPB_NetworkList_API {
 public:
  NetworkListResource(PP_Instance instance,
                      const SerializedNetworkList& list);

  NetworkListResource(const NetworkListResource&) = delete;
  NetworkListResource& operator=(const NetworkListResource&) = delete;

  ~NetworkListResource() override;

  // Resource override.
  thunk::PPB_NetworkList_API* AsPPB_NetworkList_API() override;

  // PPB_NetworkList_API implementation.
  uint32_t GetCount() override;
  PP_Var GetName(uint32_t index) override;
  PP_NetworkList_Type GetType(uint32_t index) override;
  PP_NetworkList_State GetState(uint32_t index) override;
  int32_t GetIpAddresses(uint32_t index, const PP_ArrayOutput& output) override;
  PP_Var GetDisplayName(uint32_t index) override;
  uint32_t GetMTU(uint32_t index) override;

 private:
  SerializedNetworkList list_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_NETWORK_LIST_RESOURCE_H_
