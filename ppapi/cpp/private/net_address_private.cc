// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/net_address_private.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_NetAddress_Private_1_1>() {
  return PPB_NETADDRESS_PRIVATE_INTERFACE_1_1;
}

template <> const char* interface_name<PPB_NetAddress_Private_1_0>() {
  return PPB_NETADDRESS_PRIVATE_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_NetAddress_Private_0_1>() {
  return PPB_NETADDRESS_PRIVATE_INTERFACE_0_1;
}

}  // namespace

// static
bool NetAddressPrivate::IsAvailable() {
  return has_interface<PPB_NetAddress_Private_1_1>() ||
      has_interface<PPB_NetAddress_Private_1_0>() ||
      has_interface<PPB_NetAddress_Private_0_1>();
}

// static
bool NetAddressPrivate::AreEqual(const PP_NetAddress_Private& addr1,
                                 const PP_NetAddress_Private& addr2) {
  if (has_interface<PPB_NetAddress_Private_1_1>()) {
    return !!get_interface<PPB_NetAddress_Private_1_1>()->AreEqual(&addr1,
                                                                   &addr2);
  }
  if (has_interface<PPB_NetAddress_Private_1_0>()) {
    return !!get_interface<PPB_NetAddress_Private_1_0>()->AreEqual(&addr1,
                                                                   &addr2);
  }
  if (has_interface<PPB_NetAddress_Private_0_1>()) {
    return !!get_interface<PPB_NetAddress_Private_0_1>()->AreEqual(&addr1,
                                                                   &addr2);
  }
  return false;
}

// static
bool NetAddressPrivate::AreHostsEqual(const PP_NetAddress_Private& addr1,
                                      const PP_NetAddress_Private& addr2) {
  if (has_interface<PPB_NetAddress_Private_1_1>()) {
    return !!get_interface<PPB_NetAddress_Private_1_1>()->AreHostsEqual(&addr1,
                                                                        &addr2);
  }
  if (has_interface<PPB_NetAddress_Private_1_0>()) {
    return !!get_interface<PPB_NetAddress_Private_1_0>()->AreHostsEqual(&addr1,
                                                                        &addr2);
  }
  if (has_interface<PPB_NetAddress_Private_0_1>()) {
    return !!get_interface<PPB_NetAddress_Private_0_1>()->AreHostsEqual(&addr1,
                                                                        &addr2);
  }
  return false;
}

// static
std::string NetAddressPrivate::Describe(const PP_NetAddress_Private& addr,
                                        bool include_port) {
  Module* module = Module::Get();
  if (!module)
    return std::string();

  PP_Var result_pp_var = PP_MakeUndefined();
  if (has_interface<PPB_NetAddress_Private_1_1>()) {
    result_pp_var = get_interface<PPB_NetAddress_Private_1_1>()->Describe(
        module->pp_module(),
        &addr,
        PP_FromBool(include_port));
  } else if (has_interface<PPB_NetAddress_Private_1_0>()) {
    result_pp_var = get_interface<PPB_NetAddress_Private_1_0>()->Describe(
        module->pp_module(),
        &addr,
        PP_FromBool(include_port));
  } else if (has_interface<PPB_NetAddress_Private_0_1>()) {
    result_pp_var = get_interface<PPB_NetAddress_Private_0_1>()->Describe(
        module->pp_module(),
        &addr,
        PP_FromBool(include_port));
  }

  Var result(PASS_REF, result_pp_var);
  return result.is_string() ? result.AsString() : std::string();
}

// static
bool NetAddressPrivate::ReplacePort(const PP_NetAddress_Private& addr_in,
                                    uint16_t port,
                                    PP_NetAddress_Private* addr_out) {
  if (has_interface<PPB_NetAddress_Private_1_1>()) {
    return !!get_interface<PPB_NetAddress_Private_1_1>()->ReplacePort(&addr_in,
                                                                      port,
                                                                      addr_out);
  }
  if (has_interface<PPB_NetAddress_Private_1_0>()) {
    return !!get_interface<PPB_NetAddress_Private_1_0>()->ReplacePort(&addr_in,
                                                                      port,
                                                                      addr_out);
  }
  if (has_interface<PPB_NetAddress_Private_0_1>()) {
    return !!get_interface<PPB_NetAddress_Private_0_1>()->ReplacePort(&addr_in,
                                                                      port,
                                                                      addr_out);
  }
  return false;
}

// static
bool NetAddressPrivate::GetAnyAddress(bool is_ipv6,
                                      PP_NetAddress_Private* addr) {
  if (has_interface<PPB_NetAddress_Private_1_1>()) {
    get_interface<PPB_NetAddress_Private_1_1>()->GetAnyAddress(
        PP_FromBool(is_ipv6),
        addr);
    return true;
  } else if (has_interface<PPB_NetAddress_Private_1_0>()) {
    get_interface<PPB_NetAddress_Private_1_0>()->GetAnyAddress(
        PP_FromBool(is_ipv6),
        addr);
    return true;
  } else if (has_interface<PPB_NetAddress_Private_0_1>()) {
    get_interface<PPB_NetAddress_Private_0_1>()->GetAnyAddress(
        PP_FromBool(is_ipv6),
        addr);
    return true;
  }
  return false;
}

// static
PP_NetAddressFamily_Private NetAddressPrivate::GetFamily(
    const PP_NetAddress_Private& addr) {
  if (has_interface<PPB_NetAddress_Private_1_1>())
    return get_interface<PPB_NetAddress_Private_1_1>()->GetFamily(&addr);
  if (has_interface<PPB_NetAddress_Private_1_0>())
    return get_interface<PPB_NetAddress_Private_1_0>()->GetFamily(&addr);
  return PP_NETADDRESSFAMILY_PRIVATE_UNSPECIFIED;
}

// static
uint16_t NetAddressPrivate::GetPort(const PP_NetAddress_Private& addr) {
  if (has_interface<PPB_NetAddress_Private_1_1>())
    return get_interface<PPB_NetAddress_Private_1_1>()->GetPort(&addr);
  if (has_interface<PPB_NetAddress_Private_1_0>())
    return get_interface<PPB_NetAddress_Private_1_0>()->GetPort(&addr);
  return 0;
}

// static
bool NetAddressPrivate::GetAddress(const PP_NetAddress_Private& addr,
                                   void* address,
                                   uint16_t address_size) {
  if (has_interface<PPB_NetAddress_Private_1_1>()) {
    return PP_ToBool(get_interface<PPB_NetAddress_Private_1_1>()->GetAddress(
        &addr,
        address,
        address_size));
  }
  if (has_interface<PPB_NetAddress_Private_1_0>()) {
    return PP_ToBool(get_interface<PPB_NetAddress_Private_1_0>()->GetAddress(
        &addr,
        address,
        address_size));
  }
  return false;
}

// static
uint32_t NetAddressPrivate::GetScopeID(const PP_NetAddress_Private& addr) {
  if (has_interface<PPB_NetAddress_Private_1_1>())
    return get_interface<PPB_NetAddress_Private_1_1>()->GetScopeID(&addr);
  return 0;
}

// static
bool NetAddressPrivate::CreateFromIPv4Address(
    const uint8_t ip[4],
    uint16_t port,
    struct PP_NetAddress_Private* addr_out) {
  if (has_interface<PPB_NetAddress_Private_1_1>()) {
    get_interface<PPB_NetAddress_Private_1_1>()->CreateFromIPv4Address(
        ip, port, addr_out);
    return true;
  }
  return false;
}

// static
bool NetAddressPrivate::CreateFromIPv6Address(
    const uint8_t ip[16],
    uint32_t scope_id,
    uint16_t port,
    struct PP_NetAddress_Private* addr_out) {
  if (has_interface<PPB_NetAddress_Private_1_1>()) {
    get_interface<PPB_NetAddress_Private_1_1>()->CreateFromIPv6Address(
        ip, scope_id, port, addr_out);
    return true;
  }
  return false;
}

}  // namespace pp
