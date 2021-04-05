// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/single_login_attempt.h"

#include <stdint.h>

#include <limits>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "jingle/notifier/base/const_communicator.h"
#include "jingle/notifier/base/gaia_token_pre_xmpp_auth.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "net/base/host_port_pair.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"
#include "third_party/libjingle_xmpp/xmpp/xmppclientsettings.h"

namespace notifier {

SingleLoginAttempt::Delegate::~Delegate() {}

SingleLoginAttempt::SingleLoginAttempt(const LoginSettings& login_settings,
                                       Delegate* delegate)
    : login_settings_(login_settings),
      delegate_(delegate),
      settings_list_(
          MakeConnectionSettingsList(login_settings_.GetServers(),
                                     login_settings_.try_ssltcp_first())),
      current_settings_(settings_list_.begin()) {
  if (settings_list_.empty()) {
    NOTREACHED();
    return;
  }
  TryConnect(*current_settings_);
}

SingleLoginAttempt::~SingleLoginAttempt() {}

// In the code below, we assume that calling a delegate method may end
// up in ourselves being deleted, so we always call it last.
//
// TODO(akalin): Add unit tests to enforce the behavior above.

void SingleLoginAttempt::OnConnect(
    base::WeakPtr<jingle_xmpp::XmppTaskParentInterface> base_task) {
  DVLOG(1) << "Connected to " << current_settings_->ToString();
  delegate_->OnConnect(base_task);
}

namespace {

// This function is more permissive than
// net::HostPortPair::FromString().  If the port is missing or
// unparseable, it assumes the default XMPP port.  The hostname may be
// empty.
net::HostPortPair ParseRedirectText(const std::string& redirect_text) {
  std::vector<std::string> parts = base::SplitString(
      redirect_text, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  net::HostPortPair redirect_server;
  redirect_server.set_port(kDefaultXmppPort);
  if (parts.empty()) {
    return redirect_server;
  }
  redirect_server.set_host(parts[0]);
  if (parts.size() <= 1) {
    return redirect_server;
  }
  // Try to parse the port, falling back to kDefaultXmppPort.
  int port = kDefaultXmppPort;
  if (!base::StringToInt(parts[1], &port)) {
    port = kDefaultXmppPort;
  }
  if (port <= 0 || port > std::numeric_limits<uint16_t>::max()) {
    port = kDefaultXmppPort;
  }
  redirect_server.set_port(port);
  return redirect_server;
}

}  // namespace

void SingleLoginAttempt::OnError(jingle_xmpp::XmppEngine::Error error, int subcode,
                                 const jingle_xmpp::XmlElement* stream_error) {
  DVLOG(1) << "Error: " << error << ", subcode: " << subcode
           << (stream_error
                   ? (", stream error: " + XmlElementToString(*stream_error))
                   : std::string());

  DCHECK_EQ(error == jingle_xmpp::XmppEngine::ERROR_STREAM, stream_error != NULL);

  // Check for redirection.  We expect something like:
  //
  // <stream:error><see-other-host xmlns="urn:ietf:params:xml:ns:xmpp-streams"/><str:text xmlns:str="urn:ietf:params:xml:ns:xmpp-streams">talk.google.com</str:text></stream:error> [2]
  //
  // There are some differences from the spec [1]:
  //
  //   - we expect a separate text element with the redirection info
  //     (which is the format Google Talk's servers use), whereas the
  //     spec puts the redirection info directly in the see-other-host
  //     element;
  //   - we check for redirection only during login, whereas the
  //     server can send down a redirection at any time according to
  //     the spec. (TODO(akalin): Figure out whether we need to handle
  //     redirection at any other point.)
  //
  // [1]: http://xmpp.org/internet-drafts/draft-saintandre-rfc3920bis-08.html#streams-error-conditions-see-other-host
  // [2]: http://forums.miranda-im.org/showthread.php?24376-GoogleTalk-drops
  if (stream_error) {
    const jingle_xmpp::XmlElement* other =
        stream_error->FirstNamed(jingle_xmpp::QN_XSTREAM_SEE_OTHER_HOST);
    if (other) {
      const jingle_xmpp::XmlElement* text =
          stream_error->FirstNamed(jingle_xmpp::QN_XSTREAM_TEXT);
      if (text) {
        // Yep, its a "stream:error" with "see-other-host" text,
        // let's parse out the server:port, and then reconnect
        // with that.
        const net::HostPortPair& redirect_server =
            ParseRedirectText(text->BodyText());
        // ParseRedirectText shouldn't return a zero port.
        DCHECK_NE(redirect_server.port(), 0u);
        // If we don't have a host, ignore the redirection and treat
        // it like a regular error.
        if (!redirect_server.host().empty()) {
          delegate_->OnRedirect(
              ServerInformation(
                  redirect_server,
                  current_settings_->ssltcp_support));
          // May be deleted at this point.
          return;
        }
      }
    }
  }

  if (error == jingle_xmpp::XmppEngine::ERROR_UNAUTHORIZED) {
    DVLOG(1) << "Credentials rejected";
    delegate_->OnCredentialsRejected();
    return;
  }

  if (current_settings_ == settings_list_.end()) {
    NOTREACHED();
    return;
  }

  ++current_settings_;
  if (current_settings_ == settings_list_.end()) {
    DVLOG(1) << "Could not connect to any XMPP server";
    delegate_->OnSettingsExhausted();
    return;
  }

  TryConnect(*current_settings_);
}

void SingleLoginAttempt::TryConnect(
    const ConnectionSettings& connection_settings) {
  DVLOG(1) << "Trying to connect to " << connection_settings.ToString();
  // Copy the user settings and fill in the connection parameters from
  // |connection_settings|.
  jingle_xmpp::XmppClientSettings client_settings = login_settings_.user_settings();
  connection_settings.FillXmppClientSettings(&client_settings);

  jingle_xmpp::Jid jid(client_settings.user(), client_settings.host(),
                jingle_xmpp::STR_EMPTY);
  jingle_xmpp::PreXmppAuth* pre_xmpp_auth =
      new GaiaTokenPreXmppAuth(
          jid.Str(), client_settings.auth_token(),
          client_settings.token_service(),
          login_settings_.auth_mechanism());
  xmpp_connection_ = std::make_unique<XmppConnection>(
      client_settings, login_settings_.get_socket_factory_callback(), this,
      pre_xmpp_auth, login_settings_.traffic_annotation());
}

}  // namespace notifier
