// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_AUTO_CONFIG_LIBRARY_H_
#define SERVICES_NETWORK_PROXY_AUTO_CONFIG_LIBRARY_H_

#include "base/component_export.h"
#include "net/base/ip_address.h"

namespace net {

class ClientSocketFactory;
class AddressList;

}  // namespace net

namespace network {

// Implementations for myIpAddress() and myIpAddressEx() function calls
// available in the PAC environment. These are expected to be called on a worker
// thread as they may block.
//
// Do not use these outside of PAC as they are broken APIs. See comments in the
// implementation file for details.
COMPONENT_EXPORT(NETWORK_SERVICE) net::IPAddressList PacMyIpAddress();
COMPONENT_EXPORT(NETWORK_SERVICE) net::IPAddressList PacMyIpAddressEx();

// Test exposed variants that allows mocking the UDP and DNS dependencies.
COMPONENT_EXPORT(NETWORK_SERVICE)
net::IPAddressList PacMyIpAddressForTest(
    net::ClientSocketFactory* socket_factory,
    const net::AddressList& dns_result);
COMPONENT_EXPORT(NETWORK_SERVICE)
net::IPAddressList PacMyIpAddressExForTest(
    net::ClientSocketFactory* socket_factory,
    const net::AddressList& dns_result);

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_AUTO_CONFIG_LIBRARY_H_
