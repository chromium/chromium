// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/shared_impl/private/net_address_private_impl.h"

#include <stddef.h>
#include <string.h>

#include <string>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/nacl/common/buildflags.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/thunk.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && \
    !BUILDFLAG(IS_MINIMAL_TOOLCHAIN)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

// The net address interface doesn't have a normal C -> C++ thunk since it
// doesn't actually have any proxy wrapping or associated objects; it's just a
// call into base. So we implement the entire interface here, using the thunk
// namespace so it magically gets hooked up in the proper places.

namespace ppapi {

namespace {

// Define our own net-host-net conversion, rather than reuse the one in
// base/sys_byteorder.h, to simplify the NaCl port. NaCl has no byte swap
// primitives.
uint16_t ConvertFromNetEndian16(uint16_t x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return (x << 8) | (x >> 8);
#else
  return x;
#endif
}

uint16_t ConvertToNetEndian16(uint16_t x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return (x << 8) | (x >> 8);
#else
  return x;
#endif
}

static const size_t kIPv4AddressSize = 4;
static const size_t kIPv6AddressSize = 16;

// This structure is a platform-independent representation of a network address.
// It is a private format that we embed in PP_NetAddress_Private and is NOT part
// of the stable Pepper API.
struct NetAddress {
  bool is_valid;
  bool is_ipv6;  // if true, IPv6, otherwise IPv4.
  uint16_t port;  // host order, not network order.
  int32_t flow_info;  // 0 for IPv4
  int32_t scope_id;   // 0 for IPv4
  // IPv4 addresses are 4 bytes. IPv6 are 16 bytes. Addresses are stored in net
  // order (big-endian), which only affects IPv6 addresses, which consist of 8
  // 16-bit components. These will be byte-swapped on small-endian hosts.
  uint8_t address[kIPv6AddressSize];
};

// Make sure that sizeof(NetAddress) is the same for all compilers. This ensures
// that the alignment is the same on both sides of the NaCl proxy, which is
// important because we serialize and deserialize PP_NetAddress_Private by
// simply copying the raw bytes.
static_assert(sizeof(NetAddress) == 28,
              "NetAddress different for compiler");

// Make sure the storage in |PP_NetAddress_Private| is big enough. (Do it here
// since the data is opaque elsewhere.)
static_assert(sizeof(reinterpret_cast<PP_NetAddress_Private*>(0)->data) >=
              sizeof(NetAddress),
              "PP_NetAddress_Private data too small");

base::span<const uint8_t> GetAddressBytes(const NetAddress* net_addr) {
  size_t address_size = net_addr->is_ipv6 ? kIPv6AddressSize : kIPv4AddressSize;
  return base::span(net_addr->address).first(address_size);
}

// Convert to embedded struct if it has been initialized.
NetAddress* ToNetAddress(PP_NetAddress_Private* addr) {
  if (!addr || addr->size != sizeof(NetAddress))
    return nullptr;
  return reinterpret_cast<NetAddress*>(addr->data);
}

const NetAddress* ToNetAddress(const PP_NetAddress_Private* addr) {
  return ToNetAddress(const_cast<PP_NetAddress_Private*>(addr));
}

// Initializes the NetAddress struct embedded in a PP_NetAddress_Private struct.
// Zeroes the memory, so net_addr->is_valid == false.
NetAddress* InitNetAddress(PP_NetAddress_Private* addr) {
  addr->size = sizeof(NetAddress);
  NetAddress* net_addr = ToNetAddress(addr);
  DCHECK(net_addr);
  memset(net_addr, 0, sizeof(NetAddress));
  return net_addr;
}

bool IsValid(const NetAddress* net_addr) {
  return net_addr && net_addr->is_valid;
}

PP_NetAddressFamily_Private GetFamily(const PP_NetAddress_Private* addr) {
  const NetAddress* net_addr = ToNetAddress(addr);
  if (!IsValid(net_addr))
    return PP_NETADDRESSFAMILY_PRIVATE_UNSPECIFIED;
  return net_addr->is_ipv6 ?
         PP_NETADDRESSFAMILY_PRIVATE_IPV6 : PP_NETADDRESSFAMILY_PRIVATE_IPV4;
}

uint16_t GetPort(const PP_NetAddress_Private* addr) {
  const NetAddress* net_addr = ToNetAddress(addr);
  if (!IsValid(net_addr))
    return 0;
  return net_addr->port;
}

// TODO(tsepez): should be declared UNSAFE_BUFFER_USAGE.
PP_Bool GetAddress(const PP_NetAddress_Private* addr,
                   void* address,
                   uint16_t address_size) {
  const NetAddress* net_addr = ToNetAddress(addr);
  if (!IsValid(net_addr))
    return PP_FALSE;
  // SAFETY: The caller of this PPAPI interface is required to pass a valid,
  // writable span in `address` and `address_size`.
  auto dest =
      UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(address), address_size));
  base::span<const uint8_t> src = GetAddressBytes(net_addr);
  // address_size must be big enough.
  if (src.size() > dest.size()) {
    return PP_FALSE;
  }
  dest.copy_prefix_from(src);
  return PP_TRUE;
}

uint32_t GetScopeID(const PP_NetAddress_Private* addr) {
  const NetAddress* net_addr = ToNetAddress(addr);
  if (!IsValid(net_addr))
    return 0;
  return net_addr->scope_id;
}

PP_Bool AreHostsEqual(const PP_NetAddress_Private* addr1,
                      const PP_NetAddress_Private* addr2) {
  const NetAddress* net_addr1 = ToNetAddress(addr1);
  const NetAddress* net_addr2 = ToNetAddress(addr2);
  if (!IsValid(net_addr1) || !IsValid(net_addr2))
    return PP_FALSE;

  if ((net_addr1->is_ipv6 != net_addr2->is_ipv6) ||
      (net_addr1->flow_info != net_addr2->flow_info) ||
      (net_addr1->scope_id != net_addr2->scope_id) ||
      !base::ranges::equal(GetAddressBytes(net_addr1),
                           GetAddressBytes(net_addr2))) {
    return PP_FALSE;
  }

  return PP_TRUE;
}

PP_Bool AreEqual(const PP_NetAddress_Private* addr1,
                 const PP_NetAddress_Private* addr2) {
  // |AreHostsEqual()| will also validate the addresses and return false if
  // either is invalid.
  if (!AreHostsEqual(addr1, addr2))
    return PP_FALSE;

  // AreHostsEqual has validated these net addresses.
  const NetAddress* net_addr1 = ToNetAddress(addr1);
  const NetAddress* net_addr2 = ToNetAddress(addr2);
  return PP_FromBool(net_addr1->port == net_addr2->port);
}

std::string ConvertIPv4AddressToString(const NetAddress* net_addr,
                                       bool include_port) {
  std::string description = base::StringPrintf(
      "%u.%u.%u.%u",
      net_addr->address[0], net_addr->address[1],
      net_addr->address[2], net_addr->address[3]);
  if (include_port)
    base::StringAppendF(&description, ":%u", net_addr->port);
  return description;
}

// Format an IPv6 address for human consumption, basically according to RFC
// 5952.
//  - If the scope is nonzero, it is appended to the address as "%<scope>" (this
//    is not in RFC 5952, but consistent with |getnameinfo()| on Linux and
//    Windows).
//  - If |include_port| is true, the address (possibly including the scope) is
//    enclosed in square brackets and ":<port>" is appended, i.e., the overall
//    format is "[<address>]:<port>".
//  - If the address is an IPv4 address embedded IPv6 (per RFC 4291), then the
//    mixed format is used, e.g., "::ffff:192.168.1.2". This is optional per RFC
//    5952, but consistent with |getnameinfo()|.
std::string ConvertIPv6AddressToString(const NetAddress* net_addr,
                                       bool include_port) {
  std::string description(include_port ? "[" : "");

  const uint16_t* address16 =
      reinterpret_cast<const uint16_t*>(net_addr->address);
  // IPv4 address embedded in IPv6.
  if (address16[0] == 0 && address16[1] == 0 &&
      address16[2] == 0 && address16[3] == 0 &&
      address16[4] == 0 &&
      (address16[5] == 0 || address16[5] == 0xffff)) {
    base::StringAppendF(&description, "::%s%u.%u.%u.%u",
                        address16[5] == 0 ? "" : "ffff:", net_addr->address[12],
                        net_addr->address[13], net_addr->address[14],
                        net_addr->address[15]);

    // "Real" IPv6 addresses.
  } else {
    // Find the first longest run of 0s (of length > 1), to collapse to "::".
    int longest_start = 0;
    int longest_length = 0;
    int curr_start = 0;
    int curr_length = 0;
    for (int i = 0; i < 8; i++) {
      if (address16[i] != 0) {
        curr_length = 0;
      } else {
        if (!curr_length)
          curr_start = i;
        curr_length++;
        if (curr_length > longest_length) {
          longest_start = curr_start;
          longest_length = curr_length;
        }
      }
    }

    bool need_sep = false;  // Whether the next item needs a ':' to separate.
    for (int i = 0; i < 8;) {
      if (longest_length > 1 && i == longest_start) {
        description.append("::");
        need_sep = false;
        i += longest_length;
      } else {
        uint16_t v = ConvertFromNetEndian16(address16[i]);
        base::StringAppendF(&description, "%s%x", need_sep ? ":" : "", v);
        need_sep = true;
        i++;
      }
    }
  }

  // Nonzero scopes, e.g., 123, are indicated by appending, e.g., "%123".
  if (net_addr->scope_id != 0)
    base::StringAppendF(&description, "%%%u", net_addr->scope_id);

  if (include_port)
    base::StringAppendF(&description, "]:%u", net_addr->port);

  return description;
}

PP_Var Describe(PP_Module /*module*/,
                const struct PP_NetAddress_Private* addr,
                PP_Bool include_port) {
  std::string str = NetAddressPrivateImpl::DescribeNetAddress(
      *addr, PP_ToBool(include_port));
  if (str.empty())
    return PP_MakeUndefined();
  // We must acquire the lock while accessing the VarTracker, which is part of
  // the critical section of the proxy which may be accessed by other threads.
  ProxyAutoLock lock;
  return StringVar::StringToPPVar(str);
}

PP_Bool ReplacePort(const struct PP_NetAddress_Private* src_addr,
                    uint16_t port,
                    struct PP_NetAddress_Private* dest_addr) {
  const NetAddress* src_net_addr = ToNetAddress(src_addr);
  if (!IsValid(src_net_addr) || !dest_addr)
    return PP_FALSE;
  dest_addr->size = sizeof(NetAddress);  // make sure 'size' is valid.
  NetAddress* dest_net_addr = ToNetAddress(dest_addr);
  *dest_net_addr = *src_net_addr;
  dest_net_addr->port = port;
  return PP_TRUE;
}

void GetAnyAddress(PP_Bool is_ipv6, PP_NetAddress_Private* addr) {
  if (addr) {
    NetAddress* net_addr = InitNetAddress(addr);
    net_addr->is_valid = true;
    net_addr->is_ipv6 = (is_ipv6 == PP_TRUE);
  }
}

void CreateFromIPv4Address(const uint8_t ip[4],
                           uint16_t port,
                           struct PP_NetAddress_Private* addr) {
  if (addr) {
    NetAddress* net_addr = InitNetAddress(addr);
    net_addr->is_valid = true;
    net_addr->is_ipv6 = false;
    net_addr->port = port;
    memcpy(net_addr->address, ip, kIPv4AddressSize);
  }
}

void CreateFromIPv6Address(const uint8_t ip[16],
                           uint32_t scope_id,
                           uint16_t port,
                           struct PP_NetAddress_Private* addr) {
  if (addr) {
    NetAddress* net_addr = InitNetAddress(addr);
    net_addr->is_valid = true;
    net_addr->is_ipv6 = true;
    net_addr->port = port;
    net_addr->scope_id = scope_id;
    memcpy(net_addr->address, ip, kIPv6AddressSize);
  }
}

const PPB_NetAddress_Private_0_1 net_address_private_interface_0_1 = {
  &AreEqual,
  &AreHostsEqual,
  &Describe,
  &ReplacePort,
  &GetAnyAddress
};

const PPB_NetAddress_Private_1_0 net_address_private_interface_1_0 = {
  &AreEqual,
  &AreHostsEqual,
  &Describe,
  &ReplacePort,
  &GetAnyAddress,
  &GetFamily,
  &GetPort,
  &GetAddress
};

const PPB_NetAddress_Private_1_1 net_address_private_interface_1_1 = {
  &AreEqual,
  &AreHostsEqual,
  &Describe,
  &ReplacePort,
  &GetAnyAddress,
  &GetFamily,
  &GetPort,
  &GetAddress,
  &GetScopeID,
  &CreateFromIPv4Address,
  &CreateFromIPv6Address
};

}  // namespace

namespace thunk {

PPAPI_THUNK_EXPORT const PPB_NetAddress_Private_0_1*
GetPPB_NetAddress_Private_0_1_Thunk() {
  return &net_address_private_interface_0_1;
}

PPAPI_THUNK_EXPORT const PPB_NetAddress_Private_1_0*
GetPPB_NetAddress_Private_1_0_Thunk() {
  return &net_address_private_interface_1_0;
}

PPAPI_THUNK_EXPORT const PPB_NetAddress_Private_1_1*
GetPPB_NetAddress_Private_1_1_Thunk() {
  return &net_address_private_interface_1_1;
}

}  // namespace thunk

// For the NaCl target, all we need are the API functions and the thunk.
#if !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_MINIMAL_TOOLCHAIN)

// static
bool NetAddressPrivateImpl::ValidateNetAddress(
    const PP_NetAddress_Private& addr) {
  return IsValid(ToNetAddress(&addr));
}

// static
bool NetAddressPrivateImpl::SockaddrToNetAddress(
    const sockaddr* sa,
    uint32_t sa_length,
    PP_NetAddress_Private* addr) {
  if (!sa || sa_length == 0 || !addr)
    return false;

  // Our platform neutral format stores ports in host order, not net order,
  // so convert them here.
  NetAddress* net_addr = InitNetAddress(addr);
  switch (sa->sa_family) {
    case AF_INET: {
      const struct sockaddr_in* addr4 =
          reinterpret_cast<const struct sockaddr_in*>(sa);
      net_addr->is_valid = true;
      net_addr->is_ipv6 = false;
      net_addr->port = ConvertFromNetEndian16(addr4->sin_port);
      memcpy(net_addr->address, &addr4->sin_addr.s_addr, kIPv4AddressSize);
      break;
    }
    case AF_INET6: {
      const struct sockaddr_in6* addr6 =
          reinterpret_cast<const struct sockaddr_in6*>(sa);
      net_addr->is_valid = true;
      net_addr->is_ipv6 = true;
      net_addr->port = ConvertFromNetEndian16(addr6->sin6_port);
      net_addr->flow_info = addr6->sin6_flowinfo;
      net_addr->scope_id = addr6->sin6_scope_id;
      memcpy(net_addr->address, addr6->sin6_addr.s6_addr, kIPv6AddressSize);
      break;
    }
    default:
      // InitNetAddress sets net_addr->is_valid to false.
      return false;
  }
  return true;}

// static
bool NetAddressPrivateImpl::IPEndPointToNetAddress(
    const net::IPAddressBytes& address,
    uint16_t port,
    PP_NetAddress_Private* addr) {
  if (!addr)
    return false;

  NetAddress* net_addr = InitNetAddress(addr);
  switch (address.size()) {
    case kIPv4AddressSize: {
      net_addr->is_valid = true;
      net_addr->is_ipv6 = false;
      net_addr->port = port;
      std::copy(address.begin(), address.end(), net_addr->address);
      break;
    }
    case kIPv6AddressSize: {
      net_addr->is_valid = true;
      net_addr->is_ipv6 = true;
      net_addr->port = port;
      std::copy(address.begin(), address.end(), net_addr->address);
      break;
    }
    default:
      // InitNetAddress sets net_addr->is_valid to false.
      return false;
  }

  return true;
}

// static
bool NetAddressPrivateImpl::NetAddressToIPEndPoint(
    const PP_NetAddress_Private& addr,
    net::IPAddressBytes* address,
    uint16_t* port) {
  if (!address || !port)
    return false;

  const NetAddress* net_addr = ToNetAddress(&addr);
  if (!IsValid(net_addr))
    return false;

  *port = net_addr->port;
  address->Assign(GetAddressBytes(net_addr));
  return true;
}
#endif  // !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_MINIMAL_TOOLCHAIN)

// static
std::string NetAddressPrivateImpl::DescribeNetAddress(
    const PP_NetAddress_Private& addr,
    bool include_port) {
  const NetAddress* net_addr = ToNetAddress(&addr);
  if (!IsValid(net_addr))
    return std::string();

  // On Windows, |NetAddressToString()| doesn't work in the sandbox. On Mac,
  // the output isn't consistent with RFC 5952, at least on Mac OS 10.6:
  // |getnameinfo()| collapses length-one runs of zeros (and also doesn't
  // display the scope).
  if (net_addr->is_ipv6)
    return ConvertIPv6AddressToString(net_addr, include_port);
  return ConvertIPv4AddressToString(net_addr, include_port);
}

// static
void NetAddressPrivateImpl::GetAnyAddress(PP_Bool is_ipv6,
    PP_NetAddress_Private* addr) {
  ppapi::GetAnyAddress(is_ipv6, addr);
}

// static
void NetAddressPrivateImpl::CreateNetAddressPrivateFromIPv4Address(
    const PP_NetAddress_IPv4& ipv4_addr,
    PP_NetAddress_Private* addr) {
  CreateFromIPv4Address(ipv4_addr.addr, ConvertFromNetEndian16(ipv4_addr.port),
                        addr);
}

// static
void NetAddressPrivateImpl::CreateNetAddressPrivateFromIPv6Address(
    const PP_NetAddress_IPv6& ipv6_addr,
    PP_NetAddress_Private* addr) {
  CreateFromIPv6Address(ipv6_addr.addr, 0,
                        ConvertFromNetEndian16(ipv6_addr.port), addr);
}

// static
PP_NetAddress_Family NetAddressPrivateImpl::GetFamilyFromNetAddressPrivate(
    const PP_NetAddress_Private& addr) {
  const NetAddress* net_addr = ToNetAddress(&addr);
  if (!IsValid(net_addr))
    return PP_NETADDRESS_FAMILY_UNSPECIFIED;
  return net_addr->is_ipv6 ? PP_NETADDRESS_FAMILY_IPV6 :
                             PP_NETADDRESS_FAMILY_IPV4;
}

// static
bool NetAddressPrivateImpl::DescribeNetAddressPrivateAsIPv4Address(
   const PP_NetAddress_Private& addr,
   PP_NetAddress_IPv4* ipv4_addr) {
  if (!ipv4_addr)
    return false;

  const NetAddress* net_addr = ToNetAddress(&addr);
  if (!IsValid(net_addr) || net_addr->is_ipv6)
    return false;

  ipv4_addr->port = ConvertToNetEndian16(net_addr->port);

  static_assert(sizeof(ipv4_addr->addr) == kIPv4AddressSize,
                "mismatched IPv4 address size");
  memcpy(ipv4_addr->addr, net_addr->address, kIPv4AddressSize);

  return true;
}

// static
bool NetAddressPrivateImpl::DescribeNetAddressPrivateAsIPv6Address(
    const PP_NetAddress_Private& addr,
    PP_NetAddress_IPv6* ipv6_addr) {
  if (!ipv6_addr)
    return false;

  const NetAddress* net_addr = ToNetAddress(&addr);
  if (!IsValid(net_addr) || !net_addr->is_ipv6)
    return false;

  ipv6_addr->port = ConvertToNetEndian16(net_addr->port);

  static_assert(sizeof(ipv6_addr->addr) == kIPv6AddressSize,
                "mismatched IPv6 address size");
  memcpy(ipv6_addr->addr, net_addr->address, kIPv6AddressSize);

  return true;
}

}  // namespace ppapi
