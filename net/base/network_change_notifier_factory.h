// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_FACTORY_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_FACTORY_H_

#include <memory>

#include "net/base/net_export.h"

namespace net {

class NetworkChangeNotifier;
// NetworkChangeNotifierFactory provides a mechanism for overriding the default
// instance creation process of NetworkChangeNotifier.
class NET_EXPORT NetworkChangeNotifierFactory {
 public:
  NetworkChangeNotifierFactory() {}
  virtual ~NetworkChangeNotifierFactory() {}
  virtual std::unique_ptr<NetworkChangeNotifier> CreateInstance() = 0;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_FACTORY_H_
