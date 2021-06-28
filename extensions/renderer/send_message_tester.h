// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SEND_MESSAGE_TESTER_H_
#define EXTENSIONS_RENDERER_SEND_MESSAGE_TESTER_H_

#include <string>

#include "base/macros.h"

namespace extensions {
class ScriptContext;
class TestIPCMessageSender;
struct MessageTarget;

// A helper class for testing the sendMessage, sendRequest, and connect API
// calls, since these are used across three different API namespaces
// (chrome.runtime, chrome.tabs, and chrome.extension).
class SendMessageTester {
 public:
  SendMessageTester(TestIPCMessageSender* ipc_sender,
                    ScriptContext* script_context,
                    int next_port_id,
                    const std::string& api_namespace);
  ~SendMessageTester();

  // Whether we expect the port to be open or closed at the end of the call.
  enum PortStatus {
    CLOSED,
    OPEN,
  };

  // Tests the sendMessage API with the specified expectations.
  void TestSendMessage(const std::string& args,
                       const std::string& expected_message,
                       const MessageTarget& expected_target,
                       PortStatus expected_port_status);

  // Tests the sendRequest API with the specified expectations.
  void TestSendRequest(const std::string& args,
                       const std::string& expected_message,
                       const MessageTarget& expected_target,
                       PortStatus expected_port_status);

  // Tests the connect API with the specified expectaions.
  void TestConnect(const std::string& args,
                   const std::string& expected_channel,
                   const MessageTarget& expected_target);

 private:
  enum Method {
    SEND_REQUEST,
    SEND_MESSAGE,
  };

  // Common handler for testing sendMessage and sendRequest.
  void TestSendMessageOrRequest(const std::string& args,
                                const std::string& expected_message,
                                const MessageTarget& expected_target,
                                PortStatus expected_port_status,
                                Method method);

  TestIPCMessageSender* ipc_sender_;
  ScriptContext* script_context_;
  int next_port_id_;
  std::string api_namespace_;

  DISALLOW_COPY_AND_ASSIGN(SendMessageTester);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SEND_MESSAGE_TESTER_H_
