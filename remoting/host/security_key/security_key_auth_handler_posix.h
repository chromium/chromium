// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_AUTH_HANDLER_POSIX_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_AUTH_HANDLER_POSIX_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "remoting/host/security_key/security_key_auth_handler.h"

namespace net {
class UnixDomainServerSocket;
class StreamSocket;
}  // namespace net

namespace remoting {

class SecurityKeySocket;

class SecurityKeyAuthHandlerPosix : public SecurityKeyAuthHandler {
 public:
  // Returns the default security key socket name that will be used if
  // SetSecurityKeySocketName() is never called.
  static base::FilePath GetDefaultSecurityKeySocketName();

  // Returns the name of the socket to listen for security key requests on.
  // GetDefaultSecurityKeySocketName() will be returned if
  // SetSecurityKeySocketName() has not been called.
  static const base::FilePath& GetSecurityKeySocketName();

  // Specify the name of the socket to listen to security key requests on.
  static void SetSecurityKeySocketName(
      const base::FilePath& security_key_socket_name);

  explicit SecurityKeyAuthHandlerPosix(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);

  SecurityKeyAuthHandlerPosix(const SecurityKeyAuthHandlerPosix&) = delete;
  SecurityKeyAuthHandlerPosix& operator=(const SecurityKeyAuthHandlerPosix&) =
      delete;

  ~SecurityKeyAuthHandlerPosix() override;

  // SecurityKeyAuthHandler interface.
  void CreateSecurityKeyConnection() override;
  bool IsValidConnectionId(int security_key_connection_id) const override;
  void SendClientResponse(int security_key_connection_id,
                          const std::string& response) override;
  void SendErrorAndCloseConnection(int security_key_connection_id) override;
  void SetSendMessageCallback(const SendMessageCallback& callback) override;
  size_t GetActiveConnectionCountForTest() const override;
  void SetRequestTimeoutForTest(base::TimeDelta timeout) override;

 private:
  using ActiveSockets = base::flat_map<int, std::unique_ptr<SecurityKeySocket>>;

  // Sets up the socket used for accepting new connections.
  void CreateSocket(bool success);

  // Starts listening for connection.
  void DoAccept();

  // Called when a connection is accepted.
  void OnAccepted(int result);

  // Called when a SecurityKeySocket has done reading.
  void OnReadComplete(int security_key_connection_id);

  // Gets an active socket iterator for |security_key_connection_id|.
  ActiveSockets::const_iterator GetSocketForConnectionId(
      int security_key_connection_id) const;

  // Send an error and closes an active socket.
  void SendErrorAndCloseActiveSocket(const ActiveSockets::const_iterator& iter);

  // A request timed out.
  void RequestTimedOut(int security_key_connection_id);

  // Ensures SecurityKeyAuthHandlerPosix methods are called on the same thread.
  THREAD_CHECKER(thread_checker_);

  // Socket used to listen for authorization requests.
  std::unique_ptr<net::UnixDomainServerSocket> auth_socket_;

  // A temporary holder for an accepted connection.
  std::unique_ptr<net::StreamSocket> accept_socket_;

  // Used to pass security key extension messages to the client.
  SendMessageCallback send_message_callback_;

  // The last assigned security key connection id.
  int last_connection_id_ = 0;

  // Sockets by connection id used to process gnubbyd requests.
  ActiveSockets active_sockets_;

  // Used to perform blocking File IO.
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  // Timeout used for a request.
  base::TimeDelta request_timeout_;

  base::WeakPtrFactory<SecurityKeyAuthHandlerPosix> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_AUTH_HANDLER_POSIX_H_
