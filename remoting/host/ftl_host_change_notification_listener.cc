// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ftl_host_change_notification_listener.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"

namespace remoting {

FtlHostChangeNotificationListener::FtlHostChangeNotificationListener(
    Listener* listener,
    SignalStrategy* signal_strategy)
    : listener_(listener), signal_strategy_(signal_strategy) {
  DCHECK(signal_strategy_);

  signal_strategy_->AddListener(this);
}

FtlHostChangeNotificationListener::~FtlHostChangeNotificationListener() {
  signal_strategy_->RemoveListener(this);
}

void FtlHostChangeNotificationListener::OnSignalStrategyStateChange(
    SignalStrategy::State state) {}

bool FtlHostChangeNotificationListener::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

bool FtlHostChangeNotificationListener::OnSignalStrategyIncomingMessage(
    const ftl::Id& sender_id,
    const std::string& sender_registration_id,
    const ftl::ChromotingMessage& message) {
  if (sender_id.type() != ftl::IdType_Type_SYSTEM || !message.has_status()) {
    return false;
  }

  switch (message.status().directory_state()) {
    case ftl::HostStatusChangeMessage_DirectoryState_DELETED:
      // OnHostDeleted() may want delete |signal_strategy_|, but SignalStrategy
      // objects cannot be deleted from a Listener callback, so OnHostDeleted()
      // has to be invoked later.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&FtlHostChangeNotificationListener::OnHostDeleted,
                         weak_factory_.GetWeakPtr()));
      return true;
    default:
      LOG(ERROR) << "Received unknown directory state: "
                 << message.status().directory_state();
      return false;
  }
}

void FtlHostChangeNotificationListener::OnHostDeleted() {
  listener_->OnHostDeleted();
}

}  // namespace remoting
