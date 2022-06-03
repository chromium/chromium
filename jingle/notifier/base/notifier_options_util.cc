// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/notifier_options_util.h"

#include "base/check.h"
#include "jingle/notifier/base/const_communicator.h"
#include "jingle/notifier/base/notifier_options.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"
#include "third_party/libjingle_xmpp/xmpp/jid.h"

namespace notifier {

jingle_xmpp::XmppClientSettings MakeXmppClientSettings(
    const NotifierOptions& notifier_options,
    const std::string& email, const std::string& token) {
  jingle_xmpp::Jid jid = jingle_xmpp::Jid(email);
  DCHECK(!jid.node().empty());
  DCHECK(jid.IsValid());

  jingle_xmpp::XmppClientSettings xmpp_client_settings;
  xmpp_client_settings.set_user(jid.node());
  xmpp_client_settings.set_resource("chrome-sync");
  xmpp_client_settings.set_host(jid.domain());
  xmpp_client_settings.set_use_tls(jingle_xmpp::TLS_REQUIRED);
  xmpp_client_settings.set_auth_token(notifier_options.auth_mechanism,
      notifier_options.invalidate_xmpp_login ?
      token + "bogus" : token);
  if (notifier_options.auth_mechanism == jingle_xmpp::AUTH_MECHANISM_OAUTH2)
    xmpp_client_settings.set_token_service("oauth2");
  else
    xmpp_client_settings.set_token_service("chromiumsync");
  if (notifier_options.allow_insecure_connection) {
    xmpp_client_settings.set_allow_plain(true);
    xmpp_client_settings.set_use_tls(jingle_xmpp::TLS_DISABLED);
  }
  return xmpp_client_settings;
}

ServerList GetServerList(
    const NotifierOptions& notifier_options) {
  ServerList servers;
  // Override the default servers with a test notification server if one was
  // provided.
  if (!notifier_options.xmpp_host_port.host().empty()) {
    servers.push_back(
        ServerInformation(notifier_options.xmpp_host_port,
                          DOES_NOT_SUPPORT_SSLTCP));
  } else {
    // The default servers support SSLTCP.
    servers.push_back(
        ServerInformation(
            net::HostPortPair("talk.google.com",
                              notifier::kDefaultXmppPort),
            SUPPORTS_SSLTCP));
    servers.push_back(
        ServerInformation(
            net::HostPortPair("talkx.l.google.com",
                              notifier::kDefaultXmppPort),
            SUPPORTS_SSLTCP));
  }
  return servers;
}

}  // namespace notifier
