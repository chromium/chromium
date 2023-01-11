// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_HANDLER_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "remoting/host/security_key/security_key_message.h"

namespace remoting {

class SecurityKeyIpcClient;
class SecurityKeyMessageReader;
class SecurityKeyMessageWriter;

// Routes security key messages to the client and receives response from
// the client and forwards them to the local security key tools.
class SecurityKeyMessageHandler {
 public:
  SecurityKeyMessageHandler();

  SecurityKeyMessageHandler(const SecurityKeyMessageHandler&) = delete;
  SecurityKeyMessageHandler& operator=(const SecurityKeyMessageHandler&) =
      delete;

  ~SecurityKeyMessageHandler();

  // Sets up the handler to begin receiving and processing messages.
  void Start(base::File message_read_stream,
             base::File message_write_stream,
             std::unique_ptr<SecurityKeyIpcClient> ipc_client,
             base::OnceClosure error_callback);

  // Sets |reader_| to the instance passed in via |reader|.  This method should
  // be called before Start().
  void SetSecurityKeyMessageReaderForTest(
      std::unique_ptr<SecurityKeyMessageReader> reader);

  // Sets |writer_| to the instance passed in via |writer|.  This method should
  // be called before Start().
  void SetSecurityKeyMessageWriterForTest(
      std::unique_ptr<SecurityKeyMessageWriter> writer);

 private:
  // SecurityKeyMessage handler.
  void ProcessSecurityKeyMessage(std::unique_ptr<SecurityKeyMessage> message);

  // Cleans up resources when an error occurs.
  void OnError();

  // Writes the passed in message data using |writer_|.
  void SendMessage(SecurityKeyMessageType message_type);
  void SendMessageWithPayload(SecurityKeyMessageType message_type,
                              const std::string& message_payload);

  // Used to respond to IPC connection changes.
  void HandleIpcConnectionChange();

  // Used to indicate an IPC connection error has occurred.
  void HandleIpcConnectionError();

  // Handles responses received from the client.
  void HandleSecurityKeyResponse(const std::string& response_data);

  // Handles requests to establich a connection with the Chromoting host.
  void HandleConnectRequest(const std::string& message_payload);

  // handles security key requests by forwrding them to the client.
  void HandleSecurityKeyRequest(const std::string& message_payload);

  // Used to communicate with the security key.
  std::unique_ptr<SecurityKeyIpcClient> ipc_client_;

  // Used to listen for security key messages.
  std::unique_ptr<SecurityKeyMessageReader> reader_;

  // Used to write security key messages to local security key tools.
  std::unique_ptr<SecurityKeyMessageWriter> writer_;

  // Signaled when an error occurs.
  base::OnceClosure error_callback_;

  base::ThreadChecker thread_checker_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_HANDLER_H_
