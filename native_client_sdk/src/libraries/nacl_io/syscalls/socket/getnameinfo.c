/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

#ifdef __native_client__
#ifdef __BIONIC__
// bionic has a slightly different signatute to glibc for getnameinfo
int getnameinfo(const struct sockaddr* sa, socklen_t salen, char* host,
                size_t hostlen, char* serv, size_t servlen, int flags) {
#elif defined(NACL_GLIBC_NEW)
int getnameinfo(const struct sockaddr* sa, socklen_t salen, char* host,
                socklen_t hostlen, char* serv, socklen_t servlen,
                int flags) {
#else
int getnameinfo(const struct sockaddr* sa, socklen_t salen, char* host,
                socklen_t hostlen, char* serv, socklen_t servlen,
                unsigned int flags) {
#endif
  return ki_getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
}
#endif
