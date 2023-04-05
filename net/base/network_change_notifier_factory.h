// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_FACTORY_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_FACTORY_H_

#include <memory>

#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net {

// NetworkChangeNotifierFactory provides a mechanism for overriding the default
// instance creation process of NetworkChangeNotifier.
class NET_EXPORT NetworkChangeNotifierFactory {
 public:
  NetworkChangeNotifierFactory() = default;
  virtual ~NetworkChangeNotifierFactory() = default;
  virtual std::unique_ptr<NetworkChangeNotifier> CreateInstanceWithInitialTypes(
      NetworkChangeNotifier::ConnectionType initial_type,
      NetworkChangeNotifier::ConnectionSubtype initial_subtype) = 0;
  std::unique_ptr<NetworkChangeNotifier> CreateInstance() {
    return CreateInstanceWithInitialTypes(
        NetworkChangeNotifier::kDefaultInitialConnectionType,
        NetworkChangeNotifier::kDefaultInitialConnectionSubtype);
  }
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_FACTORY_H_
