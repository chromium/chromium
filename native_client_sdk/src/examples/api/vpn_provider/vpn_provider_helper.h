// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_API_VPN_PROVIDER_VPN_PROVIDER_HELPER_H_
#define EXAMPLES_API_VPN_PROVIDER_VPN_PROVIDER_HELPER_H_

#include <stdint.h>

#include <queue>
#include <string>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/vpn_provider.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/threading/simple_thread.h"

// Helper class that keeps VpnPacket handling in its own thread.
class VpnProviderHelper {
 public:
  explicit VpnProviderHelper(pp::Instance* instance);

  void Init();
  void Bind(const std::string& config_name, const std::string& config_id);
  void SendPacket(const pp::Var& data);

 private:
  void InitOnThread(int32_t result);
  void BindOnThread(int32_t result,
                    const std::string& config,
                    const std::string& name);
  void SendPacketOnThread(int32_t result, const pp::Var& data);

  // Completion Callbacks
  void BindCompletionCallback(int32_t result);
  void ReceivePacketCompletionCallback(int32_t result, const pp::Var& packet);
  void SendPacketCompletionCallback(int32_t result);

  pp::Instance* instance_;
  pp::SimpleThread thread_;
  pp::VpnProvider vpn_;
  pp::CompletionCallbackFactory<VpnProviderHelper> factory_;

  bool send_packet_pending_;
  std::queue<pp::Var> send_packet_queue_;

  std::string config_name_;
  std::string config_id_;
};

#endif  // EXAMPLES_API_VPN_PROVIDER_VPN_PROVIDER_HELPER_H_
