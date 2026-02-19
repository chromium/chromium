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
#include "remoting/signaling/signaling_address.h"

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

bool FtlHostChangeNotificationListener::OnSignalStrategyIncomingMessage(
    const SignalingAddress& sender_address,
    const SignalingMessage& message) {
  const ftl::ChromotingMessage* ftl_message =
      std::get_if<ftl::ChromotingMessage>(&message);
  if (!ftl_message || !ftl_message->has_status()) {
    return false;
  }
  // Status messages can only be sent by a backend server (i.e., SYSTEM).
  if (!sender_address.is_system()) {
    return false;
  }

  switch (ftl_message->status().directory_state()) {
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
                 << ftl_message->status().directory_state();
      return false;
  }
}

void FtlHostChangeNotificationListener::OnHostDeleted() {
  listener_->OnHostDeleted();
}

}  // namespace remoting
