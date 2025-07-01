// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/network_list.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/array_output.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_NetworkList_1_0>() {
  return PPB_NETWORKLIST_INTERFACE_1_0;
}

}  // namespace

NetworkList::NetworkList() {
}

NetworkList::NetworkList(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

// static
bool NetworkList::IsAvailable() {
  return has_interface<PPB_NetworkList_1_0>();
}

uint32_t NetworkList::GetCount() const {
  if (!has_interface<PPB_NetworkList_1_0>())
    return 0;
  return get_interface<PPB_NetworkList_1_0>()->GetCount(pp_resource());
}

std::string NetworkList::GetName(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_1_0>())
    return std::string();
  Var result(PASS_REF,
             get_interface<PPB_NetworkList_1_0>()->GetName(
                 pp_resource(), index));
  return result.is_string() ? result.AsString() : std::string();
}

PP_NetworkList_Type NetworkList::GetType(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_1_0>())
    return PP_NETWORKLIST_TYPE_ETHERNET;
  return get_interface<PPB_NetworkList_1_0>()->GetType(
      pp_resource(), index);
}

PP_NetworkList_State NetworkList::GetState(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_1_0>())
    return PP_NETWORKLIST_STATE_DOWN;
  return get_interface<PPB_NetworkList_1_0>()->GetState(
      pp_resource(), index);
}

int32_t NetworkList::GetIpAddresses(
    uint32_t index,
    std::vector<NetAddress>* addresses) const {
  if (!has_interface<PPB_NetworkList_1_0>())
    return PP_ERROR_NOINTERFACE;
  if (!addresses)
    return PP_ERROR_BADARGUMENT;

  ResourceArrayOutputAdapter<NetAddress> adapter(addresses);
  return get_interface<PPB_NetworkList_1_0>()->GetIpAddresses(
      pp_resource(), index, adapter.pp_array_output());
}

std::string NetworkList::GetDisplayName(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_1_0>())
    return std::string();
  Var result(PASS_REF,
             get_interface<PPB_NetworkList_1_0>()->GetDisplayName(
                 pp_resource(), index));
  return result.is_string() ? result.AsString() : std::string();
}

uint32_t NetworkList::GetMTU(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_1_0>())
    return 0;
  return get_interface<PPB_NetworkList_1_0>()->GetMTU(
      pp_resource(), index);
}

}  // namespace pp
