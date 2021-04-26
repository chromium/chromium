// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_
#define JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "jingle/glue/network_service_config.h"
#include "jingle/notifier/base/server_information.h"
#include "jingle/notifier/communicator/login_settings.h"
#include "jingle/notifier/communicator/single_login_attempt.h"
#include "net/base/network_change_notifier.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "third_party/libjingle_xmpp/xmpp/xmppengine.h"

namespace jingle_xmpp {
class XmppClientSettings;
class XmppTaskParentInterface;
}  // namespace jingle_xmpp

namespace notifier {

class LoginSettings;

// Does the login, keeps it alive (with refreshing cookies and
// reattempting login when disconnected), and figures out what actions
// to take on the various errors that may occur.
//
// TODO(akalin): Make this observe proxy config changes also.
class Login
    : public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public net::NetworkChangeNotifier::DNSObserver,
      public SingleLoginAttempt::Delegate {
 public:
  class Delegate {
   public:
    // Called when a connection has been successfully established.
    virtual void OnConnect(
        base::WeakPtr<jingle_xmpp::XmppTaskParentInterface> base_task) = 0;

    // Called when there's no connection to the server but we expect
    // it to come back come back eventually.  The connection will be
    // retried with exponential backoff.
    virtual void OnTransientDisconnection() = 0;

    // Called when the current login credentials have been rejected.
    // The connection will still be retried with exponential backoff;
    // it's up to the delegate to stop connecting and/or prompt for
    // new credentials.
    virtual void OnCredentialsRejected() = 0;

   protected:
    virtual ~Delegate();
  };

  // Does not take ownership of |delegate|, which must not be NULL.
  Login(Delegate* delegate,
        const jingle_xmpp::XmppClientSettings& user_settings,
        jingle_glue::GetProxyResolvingSocketFactoryCallback
            get_socket_factory_callback,
        const ServerList& servers,
        bool try_ssltcp_first,
        const std::string& auth_mechanism,
        const net::NetworkTrafficAnnotationTag& traffic_annotation,
        network::NetworkConnectionTracker* network_connection_tracker);
  ~Login() override;

  // Starts connecting (or forces a reconnection if we're backed off).
  void StartConnection();

  // The updated settings take effect only the next time when a
  // connection is attempted (either via reconnection or a call to
  // StartConnection()).
  void UpdateXmppSettings(const jingle_xmpp::XmppClientSettings& user_settings);

  // network::NetworkConnectionTracker::NetworkConnectionObserver implementation
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // net::NetworkChangeNotifier::DNSObserver implementation.
  void OnDNSChanged() override;

  // SingleLoginAttempt::Delegate implementation.
  void OnConnect(
      base::WeakPtr<jingle_xmpp::XmppTaskParentInterface> base_task) override;
  void OnRedirect(const ServerInformation& redirect_server) override;
  void OnCredentialsRejected() override;
  void OnSettingsExhausted() override;

 private:
  // Called by the various network notifications.
  void OnNetworkEvent();

  // Stops any existing reconnect timer and sets an initial reconnect
  // interval.
  void ResetReconnectState();

  // Tries to reconnect in some point in the future.  If called
  // repeatedly, will wait longer and longer until reconnecting.
  void TryReconnect();

  // The actual function (called by |reconnect_timer_|) that does the
  // reconnection.
  void DoReconnect();

  Delegate* const delegate_;
  LoginSettings login_settings_;
  network::NetworkConnectionTracker* network_connection_tracker_;
  std::unique_ptr<SingleLoginAttempt> single_attempt_;

  // reconnection state.
  base::TimeDelta reconnect_interval_;
  base::OneShotTimer reconnect_timer_;

  DISALLOW_COPY_AND_ASSIGN(Login);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_COMMUNICATOR_LOGIN_H_
