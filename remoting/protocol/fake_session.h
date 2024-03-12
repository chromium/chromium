// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_SESSION_H_
#define REMOTING_PROTOCOL_FAKE_SESSION_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "remoting/protocol/fake_stream_socket.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/transport.h"

namespace remoting::protocol {

extern const char kTestJid[];

class FakeAuthenticator;

class FakeSession : public Session {
 public:
  FakeSession();

  FakeSession(const FakeSession&) = delete;
  FakeSession& operator=(const FakeSession&) = delete;

  ~FakeSession() override;

  void SimulateConnection(FakeSession* peer);

  EventHandler* event_handler() { return event_handler_; }
  void set_error(ErrorCode error) { error_ = error; }
  bool is_closed() const { return closed_; }

  // Sets delay for signaling message deliver when connected to a peer.
  void set_signaling_delay(base::TimeDelta signaling_delay) {
    signaling_delay_ = signaling_delay;
  }

  // Adds an |attachment| to |round|, which will be sent to plugins added by
  // AddPlugin() function.
  void SetAttachment(size_t round,
                     std::unique_ptr<jingle_xmpp::XmlElement> attachment);

  // Session interface.
  void SetEventHandler(EventHandler* event_handler) override;
  ErrorCode error() const override;
  const std::string& jid() override;
  const SessionConfig& config() override;
  const Authenticator& authenticator() const override;
  void SetTransport(Transport* transport) override;
  void Close(ErrorCode error) override;
  void AddPlugin(SessionPlugin* plugin) override;

 private:
  // Callback provided to the |transport_|.
  void SendTransportInfo(
      std::unique_ptr<jingle_xmpp::XmlElement> transport_info);

  // Called by the |peer_| to deliver incoming |transport_info|.
  void ProcessTransportInfo(
      std::unique_ptr<jingle_xmpp::XmlElement> transport_info);

  raw_ptr<EventHandler> event_handler_ = nullptr;
  std::unique_ptr<SessionConfig> config_;

  std::string jid_;

  std::unique_ptr<FakeAuthenticator> authenticator_;
  raw_ptr<Transport, DanglingUntriaged> transport_;

  ErrorCode error_ = ErrorCode::OK;
  bool closed_ = false;

  base::WeakPtr<FakeSession> peer_;
  base::TimeDelta signaling_delay_;

  std::vector<std::unique_ptr<jingle_xmpp::XmlElement>> attachments_;

  base::WeakPtrFactory<FakeSession> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_FAKE_SESSION_H_
