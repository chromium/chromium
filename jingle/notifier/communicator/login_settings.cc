// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "jingle/notifier/communicator/login_settings.h"

#include "base/check_op.h"
#include "jingle/notifier/base/server_information.h"
#include "net/cert/cert_verifier.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace notifier {

LoginSettings::LoginSettings(
    const jingle_xmpp::XmppClientSettings& user_settings,
    jingle_glue::GetProxyResolvingSocketFactoryCallback
        get_socket_factory_callback,
    const ServerList& default_servers,
    bool try_ssltcp_first,
    const std::string& auth_mechanism,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : user_settings_(user_settings),
      get_socket_factory_callback_(get_socket_factory_callback),
      default_servers_(default_servers),
      try_ssltcp_first_(try_ssltcp_first),
      auth_mechanism_(auth_mechanism),
      traffic_annotation_(traffic_annotation) {
  DCHECK_GT(default_servers_.size(), 0u);
}

LoginSettings::LoginSettings(const LoginSettings& other) = default;

LoginSettings::~LoginSettings() {}

void LoginSettings::set_user_settings(
    const jingle_xmpp::XmppClientSettings& user_settings) {
  user_settings_ = user_settings;
}

ServerList LoginSettings::GetServers() const {
  return GetServersForTime(base::Time::Now());
}

namespace {

// How long a redirect is valid for.
const int kRedirectExpirationTimeMinutes = 5;

}  // namespace

void LoginSettings::SetRedirectServer(
    const ServerInformation& redirect_server) {
  redirect_server_ = redirect_server;
  redirect_expiration_ =
      base::Time::Now() + base::Minutes(kRedirectExpirationTimeMinutes);
}

ServerList LoginSettings::GetServersForTimeForTest(base::Time now) const {
  return GetServersForTime(now);
}

base::Time LoginSettings::GetRedirectExpirationForTest() const {
  return redirect_expiration_;
}

ServerList LoginSettings::GetServersForTime(base::Time now) const {
  return
      (now < redirect_expiration_) ?
      ServerList(1, redirect_server_) :
      default_servers_;
}

}  // namespace notifier
