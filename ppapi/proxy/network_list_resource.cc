// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/network_list_resource.h"

#include <stddef.h>

#include <algorithm>

#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {
namespace proxy {

NetworkListResource::NetworkListResource(PP_Instance instance,
                                         const SerializedNetworkList& list)
    : Resource(OBJECT_IS_PROXY, instance),
      list_(list) {
}

NetworkListResource::~NetworkListResource() {}

thunk::PPB_NetworkList_API* NetworkListResource::AsPPB_NetworkList_API() {
  return this;
}

uint32_t NetworkListResource::GetCount() {
  return static_cast<uint32_t>(list_.size());
}

PP_Var NetworkListResource::GetName(uint32_t index) {
  if (index >= list_.size())
    return PP_MakeUndefined();
  return StringVar::StringToPPVar(list_.at(index).name);
}

PP_NetworkList_Type NetworkListResource::GetType(uint32_t index) {
  if (index >= list_.size())
    return PP_NETWORKLIST_TYPE_UNKNOWN;
  return list_.at(index).type;
}

PP_NetworkList_State NetworkListResource::GetState(uint32_t index) {
  if (index >= list_.size())
    return PP_NETWORKLIST_STATE_DOWN;
  return list_.at(index).state;
}

int32_t NetworkListResource::GetIpAddresses(uint32_t index,
                                            const PP_ArrayOutput& output) {
  ArrayWriter writer(output);
  if (index >= list_.size() || !writer.is_valid())
    return PP_ERROR_BADARGUMENT;

  thunk::EnterResourceCreationNoLock enter(pp_instance());
  if (enter.failed())
    return PP_ERROR_FAILED;

  const std::vector<PP_NetAddress_Private>& addresses =
      list_.at(index).addresses;
  std::vector<PP_Resource> addr_resources;
  for (size_t i = 0; i < addresses.size(); ++i) {
    addr_resources.push_back(
        enter.functions()->CreateNetAddressFromNetAddressPrivate(
            pp_instance(), addresses[i]));
  }
  if (!writer.StoreResourceVector(addr_resources))
    return PP_ERROR_FAILED;

  return PP_OK;
}

PP_Var NetworkListResource::GetDisplayName(uint32_t index) {
  if (index >= list_.size())
    return PP_MakeUndefined();
  return StringVar::StringToPPVar(list_.at(index).display_name);
}

uint32_t NetworkListResource::GetMTU(uint32_t index) {
  if (index >= list_.size())
    return 0;
  return list_.at(index).mtu;
}

}  // namespace proxy
}  // namespace thunk
