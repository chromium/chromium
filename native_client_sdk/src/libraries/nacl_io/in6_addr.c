/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/ossocket.h"
#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) && !defined(__BIONIC__)

#include <netinet/in.h>

const struct in6_addr in6addr_any = {
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}};
const struct in6_addr in6addr_loopback = {
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}}};
const struct in6_addr in6addr_nodelocal_allnodes = {
    {{0xff, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}}};
const struct in6_addr in6addr_linklocal_allnodes = {
    {{0xff, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}}};
const struct in6_addr in6addr_linklocal_allrouters = {
    {{0xff, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2}}};

#endif  /* defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) ... */
