// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_change_notifier_passive_factory.h"

#include "net/base/network_change_notifier_passive.h"

namespace network {

NetworkChangeNotifierPassiveFactory::NetworkChangeNotifierPassiveFactory() =
    default;

NetworkChangeNotifierPassiveFactory::~NetworkChangeNotifierPassiveFactory() =
    default;

std::unique_ptr<net::NetworkChangeNotifier>
NetworkChangeNotifierPassiveFactory::CreateInstanceWithInitialTypes(
    net::NetworkChangeNotifier::ConnectionType initial_type,
    net::NetworkChangeNotifier::ConnectionSubtype initial_subtype) {
  return std::make_unique<net::NetworkChangeNotifierPassive>(initial_type,
                                                             initial_subtype);
}

}  // namespace network
