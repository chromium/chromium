// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/xmpp_log_to_server.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/xmpp_constants.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;

namespace remoting {

XmppLogToServer::XmppLogToServer(
    ServerLogEntry::Mode mode,
    SignalStrategy* signal_strategy,
    const std::string& directory_bot_jid,
    scoped_refptr<base::SequencedTaskRunner> caller_task_runner)
    : mode_(mode),
      signal_strategy_(signal_strategy),
      directory_bot_jid_(directory_bot_jid) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  if (!caller_task_runner || caller_task_runner->RunsTasksInCurrentSequence()) {
    Init();
    return;
  }
  caller_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&XmppLogToServer::Init, weak_factory_.GetWeakPtr()));
}

XmppLogToServer::~XmppLogToServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  signal_strategy_->RemoveListener(this);
}

void XmppLogToServer::OnSignalStrategyStateChange(SignalStrategy::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state == SignalStrategy::CONNECTED) {
    iq_sender_ = std::make_unique<IqSender>(signal_strategy_);
    SendPendingEntries();
  } else if (state == SignalStrategy::DISCONNECTED) {
    iq_sender_.reset();
  }
}

bool XmppLogToServer::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void XmppLogToServer::Log(const ServerLogEntry& entry) {
  pending_entries_.push_back(entry);
  SendPendingEntries();
}

void XmppLogToServer::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  signal_strategy_->AddListener(this);
}

void XmppLogToServer::SendPendingEntries() {
  if (iq_sender_ == nullptr) {
    return;
  }
  if (pending_entries_.empty()) {
    return;
  }
  // Make one stanza containing all the pending entries.
  std::unique_ptr<XmlElement> stanza(ServerLogEntry::MakeStanza());
  while (!pending_entries_.empty()) {
    ServerLogEntry& entry = pending_entries_.front();
    stanza->AddElement(entry.ToStanza().release());
    pending_entries_.pop_front();
  }
  // Send the stanza to the server and ignore the response.
  iq_sender_->SendIq(kIqTypeSet, directory_bot_jid_, std::move(stanza),
                     IqSender::ReplyCallback());
}

ServerLogEntry::Mode XmppLogToServer::mode() const {
  return mode_;
}

}  // namespace remoting
