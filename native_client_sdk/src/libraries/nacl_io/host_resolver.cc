// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "nacl_io/host_resolver.h"

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nacl_io/kernel_proxy.h"
#include "nacl_io/log.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/pepper_interface.h"

#ifdef PROVIDES_SOCKET_API

namespace {

void HintsToPPHints(const addrinfo* hints, PP_HostResolver_Hint* pp_hints) {
  memset(pp_hints, 0, sizeof(*pp_hints));

  if (hints->ai_family == AF_INET)
    pp_hints->family = PP_NETADDRESS_FAMILY_IPV4;
  else if (hints->ai_family == AF_INET6)
    pp_hints->family = PP_NETADDRESS_FAMILY_IPV6;

  if (hints->ai_flags & AI_CANONNAME)
    pp_hints->flags = PP_HOSTRESOLVER_FLAG_CANONNAME;
}

void CreateAddrInfo(const addrinfo* hints,
                    struct sockaddr* addr,
                    const char* name,
                    addrinfo** list_start,
                    addrinfo** list_end) {
  addrinfo* ai = static_cast<addrinfo*>(malloc(sizeof(addrinfo)));
  memset(ai, 0, sizeof(*ai));

  if (hints && hints->ai_socktype)
    ai->ai_socktype = hints->ai_socktype;
  else
    ai->ai_socktype = SOCK_STREAM;

  if (hints && hints->ai_protocol)
    ai->ai_protocol = hints->ai_protocol;

  if (name)
    ai->ai_canonname = strdup(name);

  switch (addr->sa_family) {
    case AF_INET6: {
      sockaddr_in6* in =
          static_cast<sockaddr_in6*>(malloc(sizeof(sockaddr_in6)));
      *in = *(sockaddr_in6*)addr;
      ai->ai_family = AF_INET6;
      ai->ai_addr = reinterpret_cast<sockaddr*>(in);
      ai->ai_addrlen = sizeof(*in);
      break;
    }
    case AF_INET: {
      sockaddr_in* in = static_cast<sockaddr_in*>(malloc(sizeof(sockaddr_in)));
      *in = *(sockaddr_in*)addr;
      ai->ai_family = AF_INET;
      ai->ai_addr = reinterpret_cast<sockaddr*>(in);
      ai->ai_addrlen = sizeof(*in);
      break;
    }
    default:
      assert(0);
      return;
  }

  if (*list_start == NULL) {
    *list_start = ai;
    *list_end = ai;
    return;
  }

  (*list_end)->ai_next = ai;
  *list_end = ai;
}

}  // namespace

namespace nacl_io {

HostResolver::HostResolver() : hostent_(), ppapi_(NULL) {
}

HostResolver::~HostResolver() {
  hostent_cleanup();
}

void HostResolver::Init(PepperInterface* ppapi) {
  ppapi_ = ppapi;
}

struct hostent* HostResolver::gethostbyname(const char* name) {
  h_errno = NETDB_INTERNAL;

  struct addrinfo* ai;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_CANONNAME;
  hints.ai_family = AF_INET;
  int err = getaddrinfo(name, NULL, &hints, &ai);
  if (err) {
    switch (err) {
      case EAI_SYSTEM:
        h_errno = NO_RECOVERY;
        break;
      case EAI_NONAME:
        h_errno = HOST_NOT_FOUND;
        break;
      default:
        h_errno = NETDB_INTERNAL;
        break;
    }
    return NULL;
  }

  // We use a single hostent struct for all calls to to gethostbyname
  // (as explicitly permitted by the spec - gethostbyname is NOT supposed to
  // be threadsafe!).  However by using a lock around all the global data
  // manipulation we can at least ensure that the call doesn't crash.
  AUTO_LOCK(gethostbyname_lock_);

  // The first thing we do is free any malloced data left over from
  // the last call.
  hostent_cleanup();

  switch (ai->ai_family) {
    case AF_INET:
      hostent_.h_addrtype = AF_INET;
      hostent_.h_length = sizeof(in_addr);
      break;
    case AF_INET6:
      hostent_.h_addrtype = AF_INET6;
      hostent_.h_length = sizeof(in6_addr);
      break;
    default:
      return NULL;
  }

  if (ai->ai_canonname != NULL)
    hostent_.h_name = strdup(ai->ai_canonname);
  else
    hostent_.h_name = strdup(name);

  // Aliases aren't supported at the moment, so we just make an empty list.
  hostent_.h_aliases = static_cast<char**>(malloc(sizeof(char*)));
  if (NULL == hostent_.h_aliases)
    return NULL;
  hostent_.h_aliases[0] = NULL;

  // Count number of address in list
  int num_addresses = 0;
  struct addrinfo* current = ai;
  while (current != NULL) {
    // Only count address that have the same type as first address
    if (current->ai_family == hostent_.h_addrtype)
      num_addresses++;
    current = current->ai_next;
  }

  // Allocate address list
  hostent_.h_addr_list = static_cast<char**>(calloc(num_addresses + 1,
                                                    sizeof(char*)));
  if (NULL == hostent_.h_addr_list)
    return NULL;

  // Copy all addresses of the relevant family.
  current = ai;
  char** hostent_addr = hostent_.h_addr_list;
  while (current != NULL) {
    if (current->ai_family != hostent_.h_addrtype) {
      current = current->ai_next;
      continue;
    }
    *hostent_addr = static_cast<char*>(malloc(hostent_.h_length));
    switch (current->ai_family) {
      case AF_INET: {
        sockaddr_in* in = reinterpret_cast<sockaddr_in*>(current->ai_addr);
        memcpy(*hostent_addr, &in->sin_addr.s_addr, hostent_.h_length);
        break;
      }
      case AF_INET6: {
        sockaddr_in6* in6 = reinterpret_cast<sockaddr_in6*>(current->ai_addr);
        memcpy(*hostent_addr, &in6->sin6_addr.s6_addr, hostent_.h_length);
        break;
      }
    }
    current = current->ai_next;
    hostent_addr++;
  }

  freeaddrinfo(ai);

#if !defined(h_addr)
  // Copy element zero of h_addr_list to h_addr when h_addr is not defined
  // as in some libc's h_addr may be a separate member instead of a macro.
  hostent_.h_addr = hostent_.h_addr_list[0];
#endif

  return &hostent_;
}

void HostResolver::freeaddrinfo(struct addrinfo* res) {
  while (res) {
    struct addrinfo* cur = res;
    res = res->ai_next;
    free(cur->ai_addr);
    free(cur->ai_canonname);
    free(cur);
  }
}

int HostResolver::getnameinfo(const struct sockaddr* sa,
                              socklen_t salen,
                              char* host,
                              size_t hostlen,
                              char* serv,
                              size_t servlen,
                              int flags) {
  in_port_t port;
  const void* addr;

  if (host == NULL && serv == NULL) {
    LOG_TRACE("host and serv are NULL.");
    return EAI_NONAME;
  }

  // Currently we only handle numeric hosts and services.
  if (flags & NI_NAMEREQD)
    return EAI_NONAME;

  if (sa->sa_family == AF_INET) {
    if (salen < sizeof(struct sockaddr_in))
      return EAI_FAMILY;

    const struct sockaddr_in* sock =
        reinterpret_cast<const struct sockaddr_in*>(sa);
    port = sock->sin_port;
    addr = &sock->sin_addr.s_addr;
  } else if (sa->sa_family == AF_INET6) {
    if (salen < sizeof(struct sockaddr_in6))
      return EAI_FAMILY;

    const struct sockaddr_in6* sock =
        reinterpret_cast<const struct sockaddr_in6*>(sa);
    port = sock->sin6_port;
    addr = sock->sin6_addr.s6_addr;
  } else {
    return EAI_FAMILY;
  }

  if (host && inet_ntop(sa->sa_family, addr, host, hostlen) == NULL)
    return EAI_OVERFLOW;
  if (serv && (size_t)snprintf(serv, servlen, "%u", htons(port)) >= servlen)
    return EAI_OVERFLOW;

  return 0;
}

int HostResolver::getaddrinfo(const char* node,
                              const char* service,
                              const struct addrinfo* hints_in,
                              struct addrinfo** result) {
  *result = NULL;
  struct addrinfo* end = NULL;

  if (node == NULL && service == NULL) {
    LOG_TRACE("node and service are NULL.");
    return EAI_NONAME;
  }

  // Check the service name (port).  Currently we only handle numeric
  // services.
  long port = 0;
  if (service != NULL) {
    char* cp;
    port = strtol(service, &cp, 10);
    if (port >= 0 && port <= UINT16_MAX && *cp == '\0') {
      port = htons(port);
    } else {
      LOG_TRACE("Service \"%s\" not supported.", service);
      return EAI_SERVICE;
    }
  }

  struct addrinfo default_hints;
  memset(&default_hints, 0, sizeof(default_hints));
  const struct addrinfo* hints = hints_in ? hints_in : &default_hints;

  // Verify values passed in hints structure
  switch (hints->ai_family) {
    case AF_INET6:
    case AF_INET:
    case AF_UNSPEC:
      break;
    default:
      LOG_TRACE("Unknown family: %d.", hints->ai_family);
      return EAI_FAMILY;
  }

  struct sockaddr_in addr_in;
  memset(&addr_in, 0, sizeof(addr_in));
  addr_in.sin_family = AF_INET;
  addr_in.sin_port = port;

  struct sockaddr_in6 addr_in6;
  memset(&addr_in6, 0, sizeof(addr_in6));
  addr_in6.sin6_family = AF_INET6;
  addr_in6.sin6_port = port;

  if (node) {
    // Handle numeric node name.
    if (hints->ai_family == AF_INET || hints->ai_family == AF_UNSPEC) {
      in_addr in;
      if (inet_pton(AF_INET, node, &in)) {
        addr_in.sin_addr = in;
        CreateAddrInfo(hints, (sockaddr*)&addr_in, node, result, &end);
        return 0;
      }
    }

    if (hints->ai_family == AF_INET6 || hints->ai_family == AF_UNSPEC) {
      in6_addr in6;
      if (inet_pton(AF_INET6, node, &in6)) {
        addr_in6.sin6_addr = in6;
        CreateAddrInfo(hints, (sockaddr*)&addr_in6, node, result, &end);
        return 0;
      }
    }
  }

  // Handle AI_PASSIVE (used for listening sockets, e.g. INADDR_ANY)
  if (node == NULL && (hints->ai_flags & AI_PASSIVE)) {
    if (hints->ai_family == AF_INET6 || hints->ai_family == AF_UNSPEC) {
      const in6_addr in6addr_any = IN6ADDR_ANY_INIT;
      memcpy(&addr_in6.sin6_addr.s6_addr, &in6addr_any, sizeof(in6addr_any));
      CreateAddrInfo(hints, (sockaddr*)&addr_in6, NULL, result, &end);
    }

    if (hints->ai_family == AF_INET || hints->ai_family == AF_UNSPEC) {
      addr_in.sin_addr.s_addr = INADDR_ANY;
      CreateAddrInfo(hints, (sockaddr*)&addr_in, NULL, result, &end);
    }
    return 0;
  }

  if (NULL == ppapi_) {
    LOG_ERROR("ppapi_ is NULL.");
    return EAI_SYSTEM;
  }

  // Use PPAPI interface to resolve nodename
  HostResolverInterface* resolver_iface = ppapi_->GetHostResolverInterface();
  VarInterface* var_iface = ppapi_->GetVarInterface();
  NetAddressInterface* netaddr_iface = ppapi_->GetNetAddressInterface();

  if (!(resolver_iface && var_iface && netaddr_iface)) {
    LOG_ERROR("Got NULL interface(s): %s%s%s",
              resolver_iface ? "" : "HostResolver ",
              var_iface ? "" : "Var ",
              netaddr_iface ? "" : "NetAddress");
    return EAI_SYSTEM;
  }

  ScopedResource scoped_resolver(ppapi_,
                                 resolver_iface->Create(ppapi_->GetInstance()));
  PP_Resource resolver = scoped_resolver.pp_resource();

  struct PP_HostResolver_Hint pp_hints;
  HintsToPPHints(hints, &pp_hints);

  int err = resolver_iface->Resolve(resolver,
                                    node,
                                    0,
                                    &pp_hints,
                                    PP_BlockUntilComplete());
  if (err) {
    switch (err) {
      case PP_ERROR_NOACCESS:
        return EAI_SYSTEM;
      case PP_ERROR_NAME_NOT_RESOLVED:
        return EAI_NONAME;
      default:
        return EAI_SYSTEM;
    }
  }

  char* canon_name = NULL;
  if (hints->ai_flags & AI_CANONNAME) {
    PP_Var name_var = resolver_iface->GetCanonicalName(resolver);
    if (PP_VARTYPE_STRING == name_var.type) {
      uint32_t len = 0;
      const char* tmp = var_iface->VarToUtf8(name_var, &len);
      // For some reason GetCanonicalName alway returns an empty
      // string so this condition is never true.
      // TODO(sbc): investigate this issue with PPAPI team.
      if (len > 0) {
        // Copy and NULL-terminate the UTF8 string var.
        canon_name = static_cast<char*>(malloc(len + 1));
        strncpy(canon_name, tmp, len);
        canon_name[len] = '\0';
      }
    }
    if (!canon_name)
      canon_name = strdup(node);
    var_iface->Release(name_var);
  }

  int num_addresses = resolver_iface->GetNetAddressCount(resolver);
  if (0 == num_addresses)
    return EAI_NODATA;

  // Convert address to sockaddr struct.
  for (int i = 0; i < num_addresses; i++) {
    ScopedResource addr(ppapi_, resolver_iface->GetNetAddress(resolver, i));
    PP_Resource resource = addr.pp_resource();
    assert(resource != 0);
    assert(PP_ToBool(netaddr_iface->IsNetAddress(resource)));
    struct sockaddr* sockaddr = NULL;
    switch (netaddr_iface->GetFamily(resource)) {
      case PP_NETADDRESS_FAMILY_IPV4: {
        struct PP_NetAddress_IPv4 pp_addr;
        if (!netaddr_iface->DescribeAsIPv4Address(resource, &pp_addr)) {
          assert(false);
          break;
        }
        memcpy(&addr_in.sin_addr.s_addr, pp_addr.addr, sizeof(in_addr_t));
        sockaddr = (struct sockaddr*)&addr_in;
        break;
      }
      case PP_NETADDRESS_FAMILY_IPV6: {
        struct PP_NetAddress_IPv6 pp_addr;
        if (!netaddr_iface->DescribeAsIPv6Address(resource, &pp_addr)) {
          assert(false);
          break;
        }
        memcpy(&addr_in6.sin6_addr.s6_addr, pp_addr.addr, sizeof(in6_addr));
        sockaddr = (struct sockaddr*)&addr_in6;
        break;
      }
      default:
        return EAI_SYSTEM;
    }

    if (sockaddr != NULL)
      CreateAddrInfo(hints, sockaddr, canon_name, result, &end);

    if (canon_name) {
      free(canon_name);
      canon_name = NULL;
    }
  }

  return 0;
}

// Frees all of the deep pointers in a hostent struct. Called between uses of
// gethostbyname, and when the kernel_proxy object is destroyed.
void HostResolver::hostent_cleanup() {
  if (NULL != hostent_.h_name) {
    free(hostent_.h_name);
  }
  if (NULL != hostent_.h_aliases) {
    for (int i = 0; NULL != hostent_.h_aliases[i]; i++) {
      free(hostent_.h_aliases[i]);
    }
    free(hostent_.h_aliases);
  }
  if (NULL != hostent_.h_addr_list) {
    for (int i = 0; NULL != hostent_.h_addr_list[i]; i++) {
      free(hostent_.h_addr_list[i]);
    }
    free(hostent_.h_addr_list);
  }
  hostent_.h_name = NULL;
  hostent_.h_aliases = NULL;
  hostent_.h_addr_list = NULL;
#if !defined(h_addr)
  // Initialize h_addr separately in the case where it is not a macro.
  hostent_.h_addr = NULL;
#endif
}

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
