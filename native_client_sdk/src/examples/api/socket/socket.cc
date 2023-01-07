// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <sstream>

#include "echo_server.h"

#include "ppapi/cpp/host_resolver.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/tcp_socket.h"
#include "ppapi/cpp/udp_socket.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

class ExampleInstance : public pp::Instance {
 public:
  explicit ExampleInstance(PP_Instance instance)
    : pp::Instance(instance),
      callback_factory_(this),
      send_outstanding_(false),
      echo_server_(NULL) {}

  virtual ~ExampleInstance() {
    delete echo_server_;
  }

  virtual void HandleMessage(const pp::Var& var_message);

 private:
  bool IsConnected();
  bool IsUDP();

  void Connect(const std::string& host, bool tcp);
  void Close();
  void Send(const std::string& message);
  void Receive();

  void OnConnectCompletion(int32_t result);
  void OnResolveCompletion(int32_t result);
  void OnReceiveCompletion(int32_t result);
  void OnReceiveFromCompletion(int32_t result, pp::NetAddress source);
  void OnSendCompletion(int32_t result);

  pp::CompletionCallbackFactory<ExampleInstance> callback_factory_;
  pp::TCPSocket tcp_socket_;
  pp::UDPSocket udp_socket_;
  pp::HostResolver resolver_;
  pp::NetAddress remote_host_;

  char receive_buffer_[kBufferSize];
  bool send_outstanding_;
  EchoServer* echo_server_;
};

#define MSG_CREATE_TCP 't'
#define MSG_CREATE_UDP 'u'
#define MSG_SEND 's'
#define MSG_CLOSE 'c'
#define MSG_LISTEN 'l'

void ExampleInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_string())
    return;
  std::string message = var_message.AsString();
  // This message must contain a command character followed by ';' and
  // arguments like "X;arguments".
  if (message.length() < 2 || message[1] != ';')
    return;
  switch (message[0]) {
    case MSG_CREATE_UDP:
      // The command 'b' requests to create a UDP connection the
      // specified HOST.
      // HOST is passed as an argument like "t;HOST".
      Connect(message.substr(2), false);
      break;
    case MSG_CREATE_TCP:
      // The command 'o' requests to connect to the specified HOST.
      // HOST is passed as an argument like "u;HOST".
      Connect(message.substr(2), true);
      break;
    case MSG_CLOSE:
      // The command 'c' requests to close without any argument like "c;"
      Close();
      break;
    case MSG_LISTEN:
      {
        // The command 'l' starts a listening socket (server).
        int port = atoi(message.substr(2).c_str());
        echo_server_ = new EchoServer(this, port);
        break;
      }
    case MSG_SEND:
      // The command 't' requests to send a message as a text frame. The
      // message passed as an argument like "t;message".
      Send(message.substr(2));
      break;
    default:
      std::ostringstream status;
      status << "Unhandled message from JavaScript: " << message;
      PostMessage(status.str());
      break;
  }
}

bool ExampleInstance::IsConnected() {
  if (!tcp_socket_.is_null())
    return true;
  if (!udp_socket_.is_null())
    return true;

  return false;
}

bool ExampleInstance::IsUDP() {
  return !udp_socket_.is_null();
}

void ExampleInstance::Connect(const std::string& host, bool tcp) {
  if (IsConnected()) {
    PostMessage("Already connected.");
    return;
  }

  if (tcp) {
    if (!pp::TCPSocket::IsAvailable()) {
      PostMessage("TCPSocket not available");
      return;
    }

    tcp_socket_ = pp::TCPSocket(this);
    if (tcp_socket_.is_null()) {
      PostMessage("Error creating TCPSocket.");
      return;
    }
  } else {
    if (!pp::UDPSocket::IsAvailable()) {
      PostMessage("UDPSocket not available");
      return;
    }

    udp_socket_ = pp::UDPSocket(this);
    if (udp_socket_.is_null()) {
      PostMessage("Error creating UDPSocket.");
      return;
    }
  }

  if (!pp::HostResolver::IsAvailable()) {
    PostMessage("HostResolver not available");
    return;
  }

  resolver_ = pp::HostResolver(this);
  if (resolver_.is_null()) {
    PostMessage("Error creating HostResolver.");
    return;
  }

  int port = 80;
  std::string hostname = host;
  size_t pos = host.rfind(':');
  if (pos != std::string::npos) {
    hostname = host.substr(0, pos);
    port = atoi(host.substr(pos+1).c_str());
  }

  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&ExampleInstance::OnResolveCompletion);
  PP_HostResolver_Hint hint = { PP_NETADDRESS_FAMILY_UNSPECIFIED, 0 };
  resolver_.Resolve(hostname.c_str(), port, hint, callback);
  PostMessage("Resolving ...");
}

void ExampleInstance::OnResolveCompletion(int32_t result) {
  if (result != PP_OK) {
    PostMessage("Resolve failed.");
    return;
  }

  pp::NetAddress addr = resolver_.GetNetAddress(0);
  PostMessage(std::string("Resolved: ") +
              addr.DescribeAsString(true).AsString());

  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&ExampleInstance::OnConnectCompletion);

  if (IsUDP()) {
    PostMessage("Binding ...");
    remote_host_ = addr;
    PP_NetAddress_IPv4 ipv4_addr = { 0, { 0 } };
    udp_socket_.Bind(pp::NetAddress(this, ipv4_addr), callback);
  } else {
    PostMessage("Connecting ...");
    tcp_socket_.Connect(addr, callback);
  }
}

void ExampleInstance::Close() {
  if (!IsConnected()) {
    PostMessage("Not connected.");
    return;
  }

  if (tcp_socket_.is_null()) {
    udp_socket_.Close();
    udp_socket_ = pp::UDPSocket();
  } else {
    tcp_socket_.Close();
    tcp_socket_ = pp::TCPSocket();
  }

  PostMessage("Closed connection.");
}

void ExampleInstance::Send(const std::string& message) {
  if (!IsConnected()) {
    PostMessage("Not connected.");
    return;
  }

  if (send_outstanding_) {
    PostMessage("Already sending.");
    return;
  }

  uint32_t size = message.size();
  const char* data = message.c_str();
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&ExampleInstance::OnSendCompletion);
  int32_t result;
  if (IsUDP())
     result = udp_socket_.SendTo(data, size, remote_host_, callback);
  else
     result = tcp_socket_.Write(data, size, callback);
  std::ostringstream status;
  if (result < 0) {
    if (result == PP_OK_COMPLETIONPENDING) {
      status << "Sending bytes: " << size;
      PostMessage(status.str());
      send_outstanding_ = true;
    } else {
      status << "Send returned error: " << result;
      PostMessage(status.str());
    }
  } else {
    status << "Sent bytes synchronously: " << result;
    PostMessage(status.str());
  }
}

void ExampleInstance::Receive() {
  memset(receive_buffer_, 0, kBufferSize);
  if (IsUDP()) {
    pp::CompletionCallbackWithOutput<pp::NetAddress> callback =
        callback_factory_.NewCallbackWithOutput(
            &ExampleInstance::OnReceiveFromCompletion);
    udp_socket_.RecvFrom(receive_buffer_, kBufferSize, callback);
  } else {
    pp::CompletionCallback callback =
        callback_factory_.NewCallback(&ExampleInstance::OnReceiveCompletion);
    tcp_socket_.Read(receive_buffer_, kBufferSize, callback);
  }
}

void ExampleInstance::OnConnectCompletion(int32_t result) {
  if (result != PP_OK) {
    std::ostringstream status;
    status << "Connection failed: " << result;
    PostMessage(status.str());
    return;
  }

  if (IsUDP()) {
    pp::NetAddress addr = udp_socket_.GetBoundAddress();
    PostMessage(std::string("Bound to: ") +
                addr.DescribeAsString(true).AsString());
  } else {
    PostMessage("Connected");
  }

  Receive();
}

void ExampleInstance::OnReceiveFromCompletion(int32_t result,
                                              pp::NetAddress source) {
  OnReceiveCompletion(result);
}

void ExampleInstance::OnReceiveCompletion(int32_t result) {
  if (result < 0) {
    std::ostringstream status;
    status << "Receive failed with: " << result;
    PostMessage(status.str());
    return;
  }

  PostMessage(std::string("Received: ") + std::string(receive_buffer_, result));
  Receive();
}

void ExampleInstance::OnSendCompletion(int32_t result) {
  std::ostringstream status;
  if (result < 0) {
    status << "Send failed with: " << result;
  } else {
    status << "Sent bytes: " << result;
  }
  send_outstanding_ = false;
  PostMessage(status.str());
}

// The ExampleModule provides an implementation of pp::Module that creates
// ExampleInstance objects when invoked.
class ExampleModule : public pp::Module {
 public:
  ExampleModule() : pp::Module() {}
  virtual ~ExampleModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new ExampleInstance(instance);
  }
};

// Implement the required pp::CreateModule function that creates our specific
// kind of Module.
namespace pp {
Module* CreateModule() { return new ExampleModule(); }
}  // namespace pp
