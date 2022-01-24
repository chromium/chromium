// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/login.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "third_party/libjingle_xmpp/task_runner/taskrunner.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/asyncsocket.h"
#include "third_party/libjingle_xmpp/xmpp/prexmppauth.h"
#include "third_party/libjingle_xmpp/xmpp/xmppclient.h"
#include "third_party/libjingle_xmpp/xmpp/xmppclientsettings.h"
#include "third_party/libjingle_xmpp/xmpp/xmppengine.h"
#include "third_party/webrtc/rtc_base/physical_socket_server.h"
#include "third_party/webrtc_overrides/rtc_base/logging.h"

namespace notifier {

Login::Delegate::~Delegate() {}

Login::Login(Delegate* delegate,
             const jingle_xmpp::XmppClientSettings& user_settings,
             jingle_glue::GetProxyResolvingSocketFactoryCallback
                 get_socket_factory_callback,
             const ServerList& servers,
             bool try_ssltcp_first,
             const std::string& auth_mechanism,
             const net::NetworkTrafficAnnotationTag& traffic_annotation,
             network::NetworkConnectionTracker* network_connection_tracker)
    : delegate_(delegate),
      login_settings_(user_settings,
                      get_socket_factory_callback,
                      servers,
                      try_ssltcp_first,
                      auth_mechanism,
                      traffic_annotation),
      network_connection_tracker_(network_connection_tracker) {
  if (network_connection_tracker_)
    network_connection_tracker_->AddNetworkConnectionObserver(this);
  // TODO(akalin): Add as DNSObserver once bug 130610 is fixed.
  ResetReconnectState();
}

Login::~Login() {
  if (network_connection_tracker_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void Login::StartConnection() {
  DVLOG(1) << "Starting connection...";
  single_attempt_ = std::make_unique<SingleLoginAttempt>(login_settings_, this);
}

void Login::UpdateXmppSettings(
    const jingle_xmpp::XmppClientSettings& user_settings) {
  DVLOG(1) << "XMPP settings updated";
  login_settings_.set_user_settings(user_settings);
}

// In the code below, we assume that calling a delegate method may end
// up in ourselves being deleted, so we always call it last.
//
// TODO(akalin): Add unit tests to enforce the behavior above.

void Login::OnConnect(
    base::WeakPtr<jingle_xmpp::XmppTaskParentInterface> base_task) {
  DVLOG(1) << "Connected";
  ResetReconnectState();
  delegate_->OnConnect(base_task);
}

void Login::OnRedirect(const ServerInformation& redirect_server) {
  DVLOG(1) << "Redirected";
  login_settings_.SetRedirectServer(redirect_server);
  // Drop the current connection, and start the login process again.
  StartConnection();
  delegate_->OnTransientDisconnection();
}

void Login::OnCredentialsRejected() {
  DVLOG(1) << "Credentials rejected";
  TryReconnect();
  delegate_->OnCredentialsRejected();
}

void Login::OnSettingsExhausted() {
  DVLOG(1) << "Settings exhausted";
  TryReconnect();
  delegate_->OnTransientDisconnection();
}

void Login::OnConnectionChanged(network::mojom::ConnectionType type) {
  if (type == network::mojom::ConnectionType::CONNECTION_NONE)
    return;

  DVLOG(1) << "Network changed";
  OnNetworkEvent();
}

void Login::OnDNSChanged() {
  DVLOG(1) << "DNS changed";
  OnNetworkEvent();
}

void Login::OnNetworkEvent() {
  // Reconnect in 1 to 9 seconds (vary the time a little to try to
  // avoid spikey behavior on network hiccups).
  reconnect_interval_ = base::Seconds(base::RandInt(1, 9));
  TryReconnect();
  delegate_->OnTransientDisconnection();
}

void Login::ResetReconnectState() {
  reconnect_interval_ = base::Seconds(base::RandInt(5, 25));
  reconnect_timer_.Stop();
}

void Login::TryReconnect() {
  DCHECK_GT(reconnect_interval_.InSeconds(), 0);
  single_attempt_.reset();
  reconnect_timer_.Stop();
  DVLOG(1) << "Reconnecting in " << reconnect_interval_.InSeconds()
           << " seconds";
  reconnect_timer_.Start(FROM_HERE, reconnect_interval_, this,
                         &Login::DoReconnect);
}

void Login::DoReconnect() {
  // Double reconnect time up to 30 minutes.
  const base::TimeDelta kMaxReconnectInterval = base::Minutes(30);
  reconnect_interval_ *= 2;
  if (reconnect_interval_ > kMaxReconnectInterval)
    reconnect_interval_ = kMaxReconnectInterval;
  DVLOG(1) << "Reconnecting...";
  StartConnection();
}

}  // namespace notifier
