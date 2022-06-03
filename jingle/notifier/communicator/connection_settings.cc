// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/connection_settings.h"

#include <stdint.h>

#include "net/base/host_port_pair.h"

#include "third_party/libjingle_xmpp/xmpp/xmppclientsettings.h"

namespace notifier {

const uint16_t kSslTcpPort = 443;

ConnectionSettings::ConnectionSettings(const net::HostPortPair& server,
                                       SslTcpMode ssltcp_mode,
                                       SslTcpSupport ssltcp_support)
    : server(server),
      ssltcp_mode(ssltcp_mode),
      ssltcp_support(ssltcp_support) {}

ConnectionSettings::ConnectionSettings()
    : ssltcp_mode(DO_NOT_USE_SSLTCP),
      ssltcp_support(DOES_NOT_SUPPORT_SSLTCP) {}

ConnectionSettings::~ConnectionSettings() {}

bool ConnectionSettings::Equals(const ConnectionSettings& settings) const {
  return
      server == settings.server &&
      ssltcp_mode == settings.ssltcp_mode &&
      ssltcp_support == settings.ssltcp_support;
}

namespace {

const char* SslTcpModeToString(SslTcpMode ssltcp_mode) {
  return (ssltcp_mode == USE_SSLTCP) ? "USE_SSLTCP" : "DO_NOT_USE_SSLTCP";
}

const char* SslTcpSupportToString(SslTcpSupport ssltcp_support) {
  return
      (ssltcp_support == SUPPORTS_SSLTCP) ?
      "SUPPORTS_SSLTCP" :
      "DOES_NOT_SUPPORT_SSLTCP";
}

}  // namespace

std::string ConnectionSettings::ToString() const {
  return
      server.ToString() + ":" + SslTcpModeToString(ssltcp_mode) + ":" +
      SslTcpSupportToString(ssltcp_support);
}

void ConnectionSettings::FillXmppClientSettings(
    jingle_xmpp::XmppClientSettings* client_settings) const {
  client_settings->set_protocol((ssltcp_mode == USE_SSLTCP) ? jingle_xmpp::PROTO_SSLTCP
                                                            : jingle_xmpp::PROTO_TCP);
  client_settings->set_server(server);
}

ConnectionSettingsList MakeConnectionSettingsList(
    const ServerList& servers,
    bool try_ssltcp_first) {
  ConnectionSettingsList settings_list;

  for (ServerList::const_iterator it = servers.begin();
       it != servers.end(); ++it) {
    const ConnectionSettings settings(
        net::HostPortPair({it->server.host(), it->server.port()}),
        DO_NOT_USE_SSLTCP, it->ssltcp_support);

    if (it->ssltcp_support == SUPPORTS_SSLTCP) {
      const ConnectionSettings settings_with_ssltcp(
          net::HostPortPair({it->server.host(), kSslTcpPort}), USE_SSLTCP,
          it->ssltcp_support);

      if (try_ssltcp_first) {
        settings_list.push_back(settings_with_ssltcp);
        settings_list.push_back(settings);
      } else {
        settings_list.push_back(settings);
        settings_list.push_back(settings_with_ssltcp);
      }
    } else {
      settings_list.push_back(settings);
    }
  }

  return settings_list;
}

}  // namespace notifier
