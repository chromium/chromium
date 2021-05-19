// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_BASE_NOTIFIER_OPTIONS_H_
#define JINGLE_NOTIFIER_BASE_NOTIFIER_OPTIONS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "jingle/glue/network_service_config.h"
#include "jingle/notifier/base/notification_method.h"
#include "net/base/host_port_pair.h"

namespace network {
class NetworkConnectionTracker;
}

namespace notifier {

struct NotifierOptions {
  NotifierOptions();
  NotifierOptions(const NotifierOptions& other);
  ~NotifierOptions();

  // Indicates that the SSLTCP port (443) is to be tried before the the XMPP
  // port (5222) during login.
  bool try_ssltcp_first;

  // Indicates that insecure connections (e.g., plain authentication,
  // no TLS) are allowed.  Only used for testing.
  bool allow_insecure_connection;

  // Indicates that the login info passed to XMPP is invalidated so
  // that login fails.
  bool invalidate_xmpp_login;

  // Contains a custom URL and port for the notification server, if one is to
  // be used. Empty otherwise.
  net::HostPortPair xmpp_host_port;

  // Indicates the method used by sync clients while sending and listening to
  // notifications.
  NotificationMethod notification_method;

  // Specifies the auth mechanism to use ("X-GOOGLE-TOKEN", "X-OAUTH2", etc),
  std::string auth_mechanism;

  // Specifies how to get sockets out of network service.
  jingle_glue::NetworkServiceConfig network_config;

  // Used to listen for network connection changes.
  network::NetworkConnectionTracker* network_connection_tracker;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_NOTIFIER_OPTIONS_H_
