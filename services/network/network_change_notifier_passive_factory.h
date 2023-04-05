// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_CHANGE_NOTIFIER_PASSIVE_FACTORY_H_
#define SERVICES_NETWORK_NETWORK_CHANGE_NOTIFIER_PASSIVE_FACTORY_H_

#include <memory>

#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"

namespace network {

// This NetworkChangeNotifierFactory will create NetworkChangeNotifierPassive's
// suitable for running in a sandboxed network service as they don't actually
// access the system to watch for network changes.
class NET_EXPORT NetworkChangeNotifierPassiveFactory
    : public net::NetworkChangeNotifierFactory {
 public:
  NetworkChangeNotifierPassiveFactory();

  NetworkChangeNotifierPassiveFactory(
      const NetworkChangeNotifierPassiveFactory&) = delete;
  NetworkChangeNotifierPassiveFactory& operator=(
      const NetworkChangeNotifierPassiveFactory&) = delete;

  ~NetworkChangeNotifierPassiveFactory() override;

  // NetworkChangeNotifierFactory:
  std::unique_ptr<net::NetworkChangeNotifier> CreateInstanceWithInitialTypes(
      net::NetworkChangeNotifier::ConnectionType initial_type,
      net::NetworkChangeNotifier::ConnectionSubtype initial_subtype) override;
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_CHANGE_NOTIFIER_PASSIVE_FACTORY_H_
