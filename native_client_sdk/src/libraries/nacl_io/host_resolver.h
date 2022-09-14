// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_HOST_RESOLVER_H_
#define LIBRARIES_NACL_IO_HOST_RESOLVER_H_

#include "nacl_io/ossocket.h"
#include "nacl_io/pepper_interface.h"
#include "sdk_util/simple_lock.h"

#ifdef PROVIDES_SOCKET_API

namespace nacl_io {

class HostResolver {
 public:
  HostResolver();
  ~HostResolver();

  void Init(PepperInterface* ppapi);

  void freeaddrinfo(struct addrinfo* res);
  int getnameinfo(const struct sockaddr *sa,
                  socklen_t salen,
                  char *host,
                  size_t hostlen,
                  char *serv,
                  size_t servlen,
                  int flags);
  int getaddrinfo(const char* node,
                  const char* service,
                  const struct addrinfo* hints,
                  struct addrinfo** res);
  struct hostent* gethostbyname(const char* name);

 private:
  void hostent_initialize();
  void hostent_cleanup();

  struct hostent hostent_;
  PepperInterface* ppapi_;
  sdk_util::SimpleLock gethostbyname_lock_;
};

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
#endif  // LIBRARIES_NACL_IO_HOST_RESOLVER_H_
