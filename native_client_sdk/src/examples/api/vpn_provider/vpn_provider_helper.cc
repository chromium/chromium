// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpn_provider_helper.h"

#include <sstream>

#include "ppapi/cpp/var_dictionary.h"

#ifdef WIN32
#  undef PostMessage
#endif

VpnProviderHelper::VpnProviderHelper(pp::Instance* instance)
    : instance_(instance),
      thread_(pp::InstanceHandle(instance)),
      vpn_(pp::InstanceHandle(instance)),
      factory_(this),
      send_packet_pending_(false) {
  thread_.Start();
}

void VpnProviderHelper::Init() {
  thread_.message_loop().PostWork(
      factory_.NewCallback(&VpnProviderHelper::InitOnThread));
}

void VpnProviderHelper::Bind(const std::string& config,
                             const std::string& name) {
  thread_.message_loop().PostWork(
      factory_.NewCallback(&VpnProviderHelper::BindOnThread, config, name));
}

void VpnProviderHelper::SendPacket(const pp::Var& data) {
  thread_.message_loop().PostWork(
      factory_.NewCallback(&VpnProviderHelper::SendPacketOnThread, data));
}

void VpnProviderHelper::InitOnThread(int32_t result) {
  instance_->PostMessage("NaCl: VpnProviderHelper::InitOnThread()");
  if (!vpn_.IsAvailable()) {
    instance_->PostMessage(
        "NaCl: VpnProviderHelper::InitOnThread(): "
        "VpnProvider interface not available!");
  }

  // Initial Callback registration
  vpn_.ReceivePacket(factory_.NewCallbackWithOutput(
      &VpnProviderHelper::ReceivePacketCompletionCallback));
}

void VpnProviderHelper::BindOnThread(int32_t result,
                                     const std::string& config,
                                     const std::string& name) {
  instance_->PostMessage("NaCl: VpnProviderHelper::BindOnThread()");

  vpn_.Bind(config, name,
            factory_.NewCallback(&VpnProviderHelper::BindCompletionCallback));
}

void VpnProviderHelper::SendPacketOnThread(int32_t result,
                                           const pp::Var& data) {
  if (!send_packet_pending_) {
    send_packet_pending_ = true;
    vpn_.SendPacket(
        data,
        factory_.NewCallback(&VpnProviderHelper::SendPacketCompletionCallback));
  } else {
    std::stringstream ss;
    ss << "NaCl: VpnProviderHelper::SendPacketOnThread: "
       << "Queueing Packet";
    instance_->PostMessage(ss.str());

    send_packet_queue_.push(data);
  }
}

void VpnProviderHelper::BindCompletionCallback(int32_t result) {
  std::stringstream ss;
  ss << "NaCl: VpnProviderHelper::BindCompletionCallback(" << result << ")";
  instance_->PostMessage(ss.str());

  pp::VarDictionary dict;
  dict.Set("cmd", "bindSuccess");
  instance_->PostMessage(dict);
}

void VpnProviderHelper::ReceivePacketCompletionCallback(int32_t result,
                                                        const pp::Var& packet) {
  std::stringstream ss;
  ss << "NaCl: VpnProviderHelper::ReceivePacketCompletionCallback(" << result
     << ")";
  instance_->PostMessage(ss.str());

  /* This is the place where the developer would unpack the packet from the
   * pp:Var and send it the their implementation. Example code below.
   *
   * pp::VarDictionary message;
   * message.Set("operation", "write");
   * message.Set("payload", packet);
   * SendMessageToTun((struct PP_Var*)&message.pp_var());
   */

  // Re-register callback
  if (result == PP_OK) {
    vpn_.ReceivePacket(factory_.NewCallbackWithOutput(
        &VpnProviderHelper::ReceivePacketCompletionCallback));
  }
}

void VpnProviderHelper::SendPacketCompletionCallback(int32_t result) {
  std::stringstream ss;
  ss << "NaCl: VpnProviderHelper::SendPacketCompletionCallback(" << result
     << ")";
  instance_->PostMessage(ss.str());

  // If we have queued packets send them before accepting new ones
  if (!send_packet_queue_.empty()) {
    std::stringstream ss;
    ss << "NaCl: VpnProviderHelper::SendPacketCompletionCallback: "
       << "Sending queued packed.";
    instance_->PostMessage(ss.str());

    vpn_.SendPacket(
        send_packet_queue_.front(),
        factory_.NewCallback(&VpnProviderHelper::SendPacketCompletionCallback));
    send_packet_queue_.pop();
  } else {
    send_packet_pending_ = false;
  }
}
