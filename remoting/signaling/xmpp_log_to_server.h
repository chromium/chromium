// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_XMPP_LOG_TO_SERVER_H_
#define REMOTING_SIGNALING_XMPP_LOG_TO_SERVER_H_

#include <map>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "remoting/signaling/log_to_server.h"
#include "remoting/signaling/server_log_entry.h"
#include "remoting/signaling/signal_strategy.h"

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting {

class IqSender;

// XmppLogToServer sends log entries to a server through the signaling strategy.
class XmppLogToServer : public LogToServer, public SignalStrategy::Listener {
 public:
  // The instance will be initialized on |caller_task_runner|, and thereafter
  // it must be used on the sequence of |caller_task_runner|. By default it will
  // be initialized on the current active sequence.
  XmppLogToServer(
      ServerLogEntry::Mode mode,
      SignalStrategy* signal_strategy,
      const std::string& directory_bot_jid,
      scoped_refptr<base::SequencedTaskRunner> caller_task_runner = {});
  ~XmppLogToServer() override;

  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  // LogToServer interface.
  void Log(const ServerLogEntry& entry) override;
  ServerLogEntry::Mode mode() const override;

 private:
  void Init();
  void SendPendingEntries();

  ServerLogEntry::Mode mode_;
  SignalStrategy* signal_strategy_;
  std::unique_ptr<IqSender> iq_sender_;
  std::string directory_bot_jid_;

  base::circular_deque<ServerLogEntry> pending_entries_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<XmppLogToServer> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(XmppLogToServer);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_XMPP_LOG_TO_SERVER_H_
